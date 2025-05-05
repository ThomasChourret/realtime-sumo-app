#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // shm_open, O_RDWR
#include <sys/mman.h>   // mmap, PROT_READ, PROT_WRITE, MAP_SHARED
#include <sys/stat.h>   // mode constants
#include <unistd.h>     // close
#include <string.h>     // strlen, strncpy

#define LIGHT_SIZE 6
#define EDGES_SIZE 3

int main() {
    // Ouvrir la mémoire partagée pour les feux
    int shm_light = shm_open("light", O_RDWR, 0666);
    if (shm_light == -1) {
        perror("shm_open light");
        return 1;
    }

    char* light_state = (char*) mmap(0, sizeof(char) * LIGHT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_light, 0);
    if (light_state == MAP_FAILED) {
        perror("mmap light");
        return 1;
    }

    // Ouvrir la mémoire partagée pour les voies (lecture uniquement)
    int shm_edges = shm_open("edges", O_RDWR, 0666);
    if (shm_edges == -1) {
        perror("shm_open edges");
        return 1;
    }

    int* edges_state = (int*) mmap(0, sizeof(int) * EDGES_SIZE, PROT_READ, MAP_SHARED, shm_edges, 0);
    if (edges_state == MAP_FAILED) {
        perror("mmap edges");
        return 1;
    }

    // Boucle interactive
    char input[16];
    while (1) {
        printf("\n--- Menu ---\n");
        printf("1. Lire état des feux\n");
        printf("2. Lire état des voies\n");
        printf("3. Modifier état des feux\n");
        printf("4. Quitter\n");
        printf("Choix: ");
        fgets(input, sizeof(input), stdin);

        if (input[0] == '1') {
            printf("État des feux : ");
            for (int i = 0; i < LIGHT_SIZE; ++i) {
                printf("%c ", light_state[i]);
            }
            printf("\n");
        }
        else if (input[0] == '2') {
            printf("Nombre de véhicules sur chaque voie : ");
            for (int i = 0; i < EDGES_SIZE; ++i) {
                printf("%d ", edges_state[i]);
            }
            printf("\n");
        }
        else if (input[0] == '3') {
            char new_state[16];
            printf("Entrer nouvel état (6 caractères, ex: 'rrrggg') : ");
            fgets(new_state, sizeof(new_state), stdin);
            // Supprimer le saut de ligne
            new_state[strcspn(new_state, "\n")] = '\0';
            if (strlen(new_state) != LIGHT_SIZE) {
                printf("Erreur : vous devez entrer exactement 6 caractères.\n");
            } else {
                strncpy(light_state, new_state, LIGHT_SIZE);
                printf("Feux mis à jour.\n");
            }
        }
        else if (input[0] == '4') {
            break;
        }
        else {
            printf("Option invalide.\n");
        }
    }

    // Nettoyage
    munmap(light_state, sizeof(char) * LIGHT_SIZE);
    munmap(edges_state, sizeof(int) * EDGES_SIZE);
    close(shm_light);
    close(shm_edges);

    return 0;
}
