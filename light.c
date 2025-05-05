// -- INCLUDES -- //

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // shm_open, O_RDWR
#include <sys/mman.h>   // mmap, PROT_READ, PROT_WRITE, MAP_SHARED
#include <sys/stat.h>   // mode constants
#include <unistd.h>     // close
#include <string.h>     // strncpy

#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#define LIGHT_SIZE 6
#define EDGES_SIZE 3

#define DEBUG

// Pointeurs globaux pour accéder aux zones mémoire partagée
char* light_state = NULL;
int* edges_state = NULL;

// Memoire des feux et des vehicules
char light1[2] = "rr";
char light2[2] = "rr";
char light3[2] = "rr";

int edge1 = 0;
int edge2 = 0;
int edge3 = 0;

// Variable pour detection de changement de feux
int edge_changed = 0;

// Mutex pour l'accès au mémoire
pthread_mutex_t *mutex;

// Barrières pour synchronisation
pthread_barrier_t *barrier;

// Data structure pour le thread
struct thread_data {
    int thread_id;
    int cycle;
    int edgeid;
    char *light_state;
    int *edges_state;

    char *light;
    int  *edge;
};

// -- UTILS FUNCTION -- //
int get_elapsed_millis(struct timespec *start, struct timespec *end) {
    if (!start || !end) {  // Check for NULL pointers
        fprintf(stderr, "Error: NULL pointer passed to get_elapsed_millis()\n");
        return -1;
    }

    int elapsed_sec = end->tv_sec - start->tv_sec;
    int elapsed_nano = end->tv_nsec - start->tv_nsec;
    int elapsed_millis = (elapsed_sec * 1000) + (elapsed_nano / 1000000);

    //printf("Elapsed time: %d ms\n", elapsed_millis);
    if (elapsed_millis < 0) return 0;
    return elapsed_millis;
}

void sleep_until_next_activation ( struct timespec * next_activation ) {
    
    int err ;
    
    do {
        // absolute sleep until next_activation
        err = clock_nanosleep ( CLOCK_MONOTONIC , TIMER_ABSTIME , next_activation , NULL ) ;
        // if err is nonzero , we might have woken up
        // too early
    } while (err != 0 && errno == EINTR ) ;
    assert ( err == 0) ;
}

// Fonction pour initialiser les mémoires partagées
int initSharedMemory() {
    int shm_light = shm_open("light", O_RDWR, 0666);
    if (shm_light == -1) {
        perror("shm_open light");
        return -1;
    }

    light_state = (char*) mmap(0, sizeof(char) * LIGHT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_light, 0);
    if (light_state == MAP_FAILED) {
        perror("mmap light");
        return -1;
    }

    int shm_edges = shm_open("edges", O_RDWR, 0666);
    if (shm_edges == -1) {
        perror("shm_open edges");
        return -1;
    }

    edges_state = (int*) mmap(0, sizeof(int) * EDGES_SIZE, PROT_READ, MAP_SHARED, shm_edges, 0);
    if (edges_state == MAP_FAILED) {
        perror("mmap edges");
        return -1;
    }

    return 0;
}

// --- THREAD FUNCTIONS --- //

void * aquireData(void *arg) {
    // Cette fonction thread permet de recuperer l'état des feux et des véhicules

    struct thread_data *data = (struct thread_data *) arg;
    int thread_id = data->thread_id;
    int cycle = data->cycle;
    int edgeid = data->edgeid;
    char *light_state = data->light_state;
    int *edges_state = data->edges_state;
    char *light = data->light;
    int *edge = data->edge;

    struct  timespec *last_start;
    struct  timespec *last_end;
    last_start = (struct timespec *) malloc(sizeof(struct timespec));
    last_end = (struct timespec *) malloc(sizeof(struct timespec));

    pthread_barrier_wait(barrier);
    clock_gettime(CLOCK_MONOTONIC, last_start);

    while (1){

        pthread_mutex_lock(mutex);

        #ifdef DEBUG
        printf("[DEBUG] [AQUIRE %d] Acquisition des données...\n", edgeid);
        #endif

        //memcpy(light, light_state, sizeof(char) * LIGHT_SIZE);
        //memcpy(edges, edges_state, sizeof(int) * EDGES_SIZE);

        for (int i = 0; i < 2; ++i) {
            light[i] = light_state[(edgeid-1)*2 + i];
        }
        int edge_copy = *edge;
        *edge = edges_state[edgeid-1];
        if (edge_copy != *edge) {
            edge_changed = 1;
        }


        #ifdef DEBUG
        printf("[DEBUG] [AQUIRE %d] État du feux : ",edgeid);
        for (int i = 0; i < 2; ++i) {
            printf("%c ", light[i]);
        }
        printf("\n");
        printf("[DEBUG] [AQUIRE %d] Nombre de véhicules : ", edgeid);
        printf("%d", *edge);
        printf("\n");
        #endif

        pthread_mutex_unlock(mutex);

        clock_gettime(CLOCK_MONOTONIC, last_end);
        int elapsed = get_elapsed_millis(last_start, last_end);
        if (elapsed < 0) {
            fprintf(stderr, "[AQUIRE %d] Erreur de calcul du temps écoulé.\n", edgeid);
            return NULL;
        }
        #ifdef DEBUG
        printf("[DEBUG] [AQUIRE %d] Temps de réponse : %d ms\n", edgeid, elapsed);
        printf("\n");
        #endif

        struct timespec next_activation;
        next_activation.tv_sec = last_start->tv_sec + (cycle / 1000);
        next_activation.tv_nsec = last_start->tv_nsec + ((cycle % 1000) * 1000000);
        if (next_activation.tv_nsec >= 1000000000) {
            next_activation.tv_sec++;
            next_activation.tv_nsec -= 1000000000;
        }
        *(last_start)=next_activation;
        sleep_until_next_activation(&next_activation);

    }

    return NULL;
}

void * computeLight(void* agrs){
    // Cette fonction thread permet de calculer l'état futur des feux en fonction de l'état des véhicules
 
    struct  timespec *last_start;
    struct  timespec *last_end;
    struct  timespec *now;
    struct  timespec *last_change;

    last_start = (struct timespec *) malloc(sizeof(struct timespec));
    last_end = (struct timespec *) malloc(sizeof(struct timespec));
    now = (struct timespec *) malloc(sizeof(struct timespec));
    last_change = (struct timespec *) malloc(sizeof(struct timespec));

    clock_gettime(CLOCK_MONOTONIC, last_change);
    last_change->tv_sec -= 5;
    last_change->tv_nsec -= 0;

    pthread_barrier_wait(barrier);
    clock_gettime(CLOCK_MONOTONIC, last_start);

    while (1){

        pthread_mutex_lock(mutex);

        #ifdef DEBUG
        printf("[DEBUG] [COMPUTE] Calcul de l'état des feux...\n");
        #endif

        // Calculer l'état futur des feux en fonction de l'état des véhicules
        //if (edge_changed) {

            // mettre a jour now
            clock_gettime(CLOCK_MONOTONIC, now);

            // Vérifier si le temps écoulé depuis le dernier changement est supérieur à 5 secondes
            int elapsed = get_elapsed_millis(last_change, now);
            if (elapsed < 0) {
                fprintf(stderr, "[COMPUTE] Erreur de calcul du temps écoulé.\n");
                return NULL;
            }
            if (elapsed < 5000) {
                pthread_mutex_unlock(mutex);
                goto end;
            }
            
            edge_changed = 0;

            int total_vehicles = 0;
            int max_vehicles = 0;
            int max_edge = 0;

            for (int i = 0; i < EDGES_SIZE; ++i) {
                total_vehicles += edges_state[i];
                if (edges_state[i] > max_vehicles) {
                    max_vehicles = edges_state[i];
                    max_edge = i;
                }
            }

            // Calculer l'état futur des feux
            if (total_vehicles > 0) {
                for (int i = 0; i < EDGES_SIZE; ++i) {
                    if (i == max_edge) {
                        light_state[i*2] = 'g';
                        light_state[i*2 + 1] = 'g';
                    } else {
                        light_state[i*2] = 'r';
                        light_state[i*2 + 1] = 'r';
                    }
                }
            } else {
                for (int i = 0; i < EDGES_SIZE; ++i) {
                    light_state[i*2] = 'r';
                    light_state[i*2 + 1] = 'r';
                }
            }

            #ifdef DEBUG
            printf("[DEBUG] [COMPUTE] État futur des feux : ");
            for (int i = 0; i < LIGHT_SIZE; ++i) {
                printf("%c ", light_state[i]);
            }
            printf("\n");
            #endif

            // Mettre à jour l'état des feux
            char *light = light_state;
            memcpy(light, light_state, sizeof(char) * LIGHT_SIZE);

            // Mettre a jour last_change
            clock_gettime(CLOCK_MONOTONIC, last_change);
            
        //}

        end:

        pthread_mutex_unlock(mutex);

        clock_gettime(CLOCK_MONOTONIC, last_end);
        elapsed = get_elapsed_millis(last_start, last_end);
        if (elapsed < 0) {
            fprintf(stderr, "[COMPUTE] Erreur de calcul du temps écoulé.\n");
            return NULL;
        }
        #ifdef DEBUG
        printf("[DEBUG] [COMPUTE] Temps de réponse : %d ms\n", elapsed);
        printf("\n");
        #endif

        struct timespec next_activation;
        next_activation.tv_sec = last_start->tv_sec + (1000 / 1000);
        next_activation.tv_nsec = last_start->tv_nsec + ((1000 % 1000) * 1000000);
        if (next_activation.tv_nsec >= 1000000000) {
            next_activation.tv_sec++;
            next_activation.tv_nsec -= 1000000000;
        }
        *(last_start)=next_activation;
        sleep_until_next_activation(&next_activation);

    }
    return NULL;

}

void * logData(void* agrs){
    // Cette fonction thread permet de logger les données dans un fichier
    



}

int main() {

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Initialisation de la mémoire partagée...\n");
    #endif

    if (initSharedMemory() != 0) {
        fprintf(stderr, "[MAIN] Erreur d'initialisation des mémoires partagées.\n");
        return 1;
    }

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Mémoires partagées initialisées avec succès.\n");
    #endif

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Initialisation des feux et des véhicules...\n");
    #endif

    // Initialisation des mutex et barrières
    mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        fprintf(stderr, "[MAIN] Erreur d'initialisation du mutex.\n");
        return 1;
    }

    barrier = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
    if (pthread_barrier_init(barrier, NULL, 4) != 0) {
        fprintf(stderr, "[MAIN] Erreur d'initialisation de la barrière.\n");
        return 1;
    }

    // Initialize thread attributes
    // Set up attributes for task 1
    struct sched_param param[4];
    pthread_attr_t attr[4];

    pthread_attr_init(&attr[0]);
    pthread_attr_setschedpolicy(&attr[0], SCHED_RR);
    param[0].sched_priority = 10;
    pthread_attr_setschedparam(&attr[0], &param[0]);
    pthread_attr_setinheritsched(&attr[0], PTHREAD_EXPLICIT_SCHED);

    // Set up attributes for task 2
    pthread_attr_init(&attr[1]);
    pthread_attr_setschedpolicy(&attr[1], SCHED_RR);
    param[1].sched_priority = 10;
    pthread_attr_setschedparam(&attr[1], &param[1]);
    pthread_attr_setinheritsched(&attr[1], PTHREAD_EXPLICIT_SCHED);
    
    // Set up attributes for task 3
    pthread_attr_init(&attr[2]);
    pthread_attr_setschedpolicy(&attr[2], SCHED_RR);
    param[2].sched_priority = 10;
    pthread_attr_setschedparam(&attr[2], &param[2]);
    pthread_attr_setinheritsched(&attr[2], PTHREAD_EXPLICIT_SCHED);

    // Set up attributes for task 4
    pthread_attr_init(&attr[3]);
    pthread_attr_setschedpolicy(&attr[3], SCHED_RR);
    param[3].sched_priority = 50;
    pthread_attr_setschedparam(&attr[3], &param[3]);
    pthread_attr_setinheritsched(&attr[3], PTHREAD_EXPLICIT_SCHED);

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Feux et véhicules initialisés avec succès.\n");
    #endif

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Création du thread pour acquérir les données...\n");
    #endif

    // Création du thread pour acquérir les données
    pthread_t thread[4];
    struct thread_data data[3];

    for (int i = 0; i < 3; ++i) {
        data[i].thread_id = i;
        data[i].cycle = 1000; // Cycle de 1 seconde
        data[i].edgeid = i + 1;
        data[i].light_state = light_state;
        data[i].edges_state = edges_state;
        if (i == 0) {
            data[i].light = light1;
            data[i].edge = &edge1;
        } else if (i == 1) {
            data[i].light = light2;
            data[i].edge = &edge2;
        } else {
            data[i].light = light3;
            data[i].edge = &edge3;
        }


        if (pthread_create(&thread[i], &attr[i], aquireData, (void *)&data[i]) != 0) {
            //perror("pthread_create");
            fprintf(stderr, "[MAIN] Erreur de création du thread.\n");
            return 1;
        }
    }

    if (pthread_create(&thread[3], &attr[3], computeLight, NULL) != 0) {
        //perror("pthread_create");
        fprintf(stderr, "[MAIN] Erreur de création du thread.\n");
        return 1;
    }

    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Thread créé avec succès.\n");
    #endif

    // Attente de la fin du thread
    for (int i = 0; i < 4; ++i) {
        if (pthread_join(thread[i], NULL) != 0) {
            fprintf(stderr, "[MAIN] Erreur de join du thread.\n");
            return 1;
        }
    }
    #ifdef DEBUG
    printf("[DEBUG] [MAIN] Tous les threads ont terminé.\n");
    #endif

    // Destruction du mutex et de la barrière
    pthread_mutex_destroy(mutex);
    free(mutex);
    pthread_barrier_destroy(barrier);
    free(barrier);

    // Libération mémoire (jamais atteinte ici, mais bonne pratique)
    munmap(light_state, sizeof(char) * LIGHT_SIZE);
    munmap(edges_state, sizeof(int) * EDGES_SIZE);

    return 0;
}
