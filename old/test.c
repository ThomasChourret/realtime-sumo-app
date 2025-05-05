#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int send_traci_message(int sock, const unsigned char* msg, int len) {
    int sent = send(sock, msg, len, 0);
    if (sent < 0) {
        perror("Erreur lors de l'envoi du message TraCI");
        return -1;
    }
    return sent;
}

int recv_traci_response(int sock, unsigned char* buffer, int buffer_size) {
    int received = recv(sock, buffer, buffer_size, 0);
    if (received < 0) {
        perror("Erreur lors de la réception (ou timeout)");
        return -1;
    } else if (received == 0) {
        printf("Connexion fermée par SUMO.\n");
        return 0;
    }

    printf("Reçu %d octets de SUMO :\n", received);
    for (int i = 0; i < received; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");

    // Tentative de lecture d'une string TraCI
    for (int i = 0; i < received - 2; i++) {
        if (buffer[i] == 0x0b) { // 0x0b = type string
            int strlen = buffer[i + 1];
            printf("Contenu string reçu : ");
            fwrite(&buffer[i + 2], 1, strlen, stdout);
            printf("\n");
            break;
        }
    }

    return received;
}


int main() {
    int sock;
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(8813); // Port TraCI

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to SUMO TraCI\n");

    // Set a 2-second timeout for recv()
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    // Proper TraCI message: CMD_GET_VERSION
    unsigned char traci_msg[] = {
        0x00, 0x04, // message length = 4 bytes (excluding length field itself)
        0x00,       // command ID (compound command)
        0x17        // CMD_GET_VERSION
    };

    // Send TraCI get version request
    if (send(sock, traci_msg, sizeof(traci_msg), 0) < 0) {
        perror("Send failed");
        close(sock);
        return 1;
    }

    int idx = 0;
    
    // Construction du message TraCI ici (ex : get traffic light state)
    const char* id = "TL1";
    int id_len = strlen(id);
    int payload_len = 1 + 1 + 1 + 1 + id_len;
    
    unsigned char request[] = {
        0x00, 0x00, 0x00, 0x07,  // length of the following payload
        0x80,                    // GET
        0x31,                    // VAR_RED_YELLOW_GREEN_STATE
        0x12,                    // object type: traffic light
        0x02,                    // string length
        'J', '4'                 // ID du feu
    };        

    unsigned char response[1024];

    unsigned char get_version[] = {
        0x00, 0x00, 0x00, 0x02,
        0x80, 0x00
    };
    

    sleep(5);

    send_traci_message(sock, get_version, sizeof(get_version));
    sleep(1); // wait a bit before receiving the response
    recv_traci_response(sock, response, sizeof(response));
    

    // Keep alive or close
    sleep(2); // let it sit for a bit

    while(1){
    // Envoi
    send_traci_message(sock, request, idx);
    
    sleep(1); // wait a bit before receiving the response

    // Réception
    recv_traci_response(sock, response, sizeof(response));
    }

    close(sock);
    return 0;
}
