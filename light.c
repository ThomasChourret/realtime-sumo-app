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
char light[LIGHT_SIZE] = "rrrrrr"; // Initialisation des feux à rouge
int edges[EDGES_SIZE] = {0, 0, 0}; // Initialisation des véhicules à 0

// Mutex pour l'accès au mémoire
pthread_mutex_t *mutex;

// Barrières pour synchronisation
pthread_barrier_t *barrier;

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

    struct  timespec *last_start;
    struct  timespec *last_end;
    last_start = (struct timespec *) malloc(sizeof(struct timespec));
    last_end = (struct timespec *) malloc(sizeof(struct timespec));
    int     cycle = 1000; 

    pthread_barrier_wait(barrier);
    clock_gettime(CLOCK_MONOTONIC, last_start);

    while (1){

        pthread_mutex_lock(mutex);

        #ifdef DEBUG
        printf("[DEBUG] Acquisition des données...\n");
        #endif

        memcpy(light, light_state, sizeof(char) * LIGHT_SIZE);
        memcpy(edges, edges_state, sizeof(int) * EDGES_SIZE);

        #ifdef DEBUG
        printf("[DEBUG] État des feux : ");
        for (int i = 0; i < LIGHT_SIZE; ++i) {
            printf("%c ", light_state[i]);
        }
        printf("\n");
        printf("[DEBUG] Nombre de véhicules sur chaque voie : ");
        for (int i = 0; i < EDGES_SIZE; ++i) {
            printf("%d ", edges_state[i]);
        }
        printf("\n");
        #endif

        pthread_mutex_unlock(mutex);

        clock_gettime(CLOCK_MONOTONIC, last_end);
        int elapsed = get_elapsed_millis(last_start, last_end);
        if (elapsed < 0) {
            fprintf(stderr, "Erreur de calcul du temps écoulé.\n");
            return NULL;
        }
        #ifdef DEBUG
        printf("[DEBUG] Temps de réponse : %d ms\n", elapsed);
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
    
}

int main() {

    #ifdef DEBUG
    printf("[DEBUG] Initialisation de la mémoire partagée...\n");
    #endif

    if (initSharedMemory() != 0) {
        fprintf(stderr, "Erreur d'initialisation des mémoires partagées.\n");
        return 1;
    }

    #ifdef DEBUG
    printf("[DEBUG] Mémoires partagées initialisées avec succès.\n");
    #endif

    #ifdef DEBUG
    printf("[DEBUG] Initialisation des feux et des véhicules...\n");
    #endif

    // Initialisation des mutex et barrières
    mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        fprintf(stderr, "Erreur d'initialisation du mutex.\n");
        return 1;
    }

    barrier = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
    if (pthread_barrier_init(barrier, NULL, 1) != 0) {
        fprintf(stderr, "Erreur d'initialisation de la barrière.\n");
        return 1;
    }

    #ifdef DEBUG
    printf("[DEBUG] Feux et véhicules initialisés avec succès.\n");
    #endif

    #ifdef DEBUG
    printf("[DEBUG] Création du thread pour acquérir les données...\n");
    #endif

    // Création du thread pour acquérir les données
    pthread_t thread;
    if (pthread_create(&thread, NULL, aquireData, NULL) != 0) {
        fprintf(stderr, "Erreur de création du thread.\n");
        return 1;
    }

    #ifdef DEBUG
    printf("[DEBUG] Thread créé avec succès.\n");
    #endif

    // Attente de la fin du thread
    if (pthread_join(thread, NULL) != 0) {
        fprintf(stderr, "Erreur d'attente du thread.\n");
        return 1;
    }

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
