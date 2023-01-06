#include "definitions.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

bool keepRunning = true;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printError("Klienta je nutne spustit s nasledujucimi argumentmi: adresa port pouzivatel (pocet_uzlov).");
    }

    //ziskanie adresy a portu servera <netdb.h>
    struct hostent *server = gethostbyname(argv[1]);
    if (server == NULL) {
        printError("Server neexistuje.");
    }
    int port = atoi(argv[2]);
    if (port <= 0) {
        printError("Port musi byt cele cislo vacsie ako 0.");
    }
    char *userName = argv[3];
    char msg[BUFFER_LENGTH + 1] = "";
    if (argc > 4) {
        int pocetUzlov = atoi(argv[4]);
        if (pocetUzlov < 3 || pocetUzlov > NUM_NODES) {
            printError("Pocet uzlov musi byt v rozsahu od 3 do 20.");
        }
        sprintf(msg, "%s", argv[4]);
    } else {
        sprintf(msg, "%d", 0);
    }

    //vytvorenie socketu <sys/socket.h>
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printError("Chyba - socket.");
    }

    //definovanie adresy servera <arpa/inet.h>
    struct sockaddr_in serverAddress;
    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serverAddress.sin_addr.s_addr, server->h_length);
    serverAddress.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        printError("Chyba - server connect.");
    }

    //poslanie poctu uzlov na ktore sa chce pripojit, nic ak sa pripaja ako uzol
    if (send(sock, msg, BUFFER_LENGTH, 0) < 0) {
        printError("Chyba - send.");
    }

    if (argc > 4) { // pripojenie ako koncovy pouzivatel
        struct sockaddr_in entryNodeAddress;
        recv(sock, &entryNodeAddress, sizeof(entryNodeAddress), 0);
        close(sock);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            printError("Chyba - socket.");
        }

        if (connect(sock, (struct sockaddr *) &entryNodeAddress, sizeof(entryNodeAddress)) < 0) {
            printError("Chyba - node connect.");
        }

        //inicializacia dat zdielanych medzi vlaknami
        DATA data;
        data_init(&data, userName, sock);

        //vytvorenie vlakna pre zapisovanie dat do socketu <pthread.h>
        pthread_t thread;
        pthread_create(&thread, NULL, data_writeData, (void *) &data);

        //v hlavnom vlakne sa bude vykonavat citanie dat zo socketu
        data_readData((void *) &data);

        //pockame na skoncenie zapisovacieho vlakna <pthread.h>
        pthread_join(thread, NULL);
        data_destroy(&data);
    } else { // pripojenie ako uzol
        int socketIn = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in socketInAddr;
        socketInAddr.sin_family = AF_INET;
        socketInAddr.sin_addr.s_addr = INADDR_ANY;
        socketInAddr.sin_port = htons(port - 2);
        int option = 1;
        if (setsockopt(socketIn, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
            printError("Chyba - setsockopt.");
        }
        if (bind(socketIn, (struct sockaddr *) &socketInAddr, sizeof(socketInAddr)) < 0) {
            printError("Chyba - socketIn bind");
        }
        listen(socketIn, 10);

        //recv from server it can send
        if (recv(sock, msg, BUFFER_LENGTH, 0) < 0) {
            printError("Chyba - cont recv");
        }
        if (strcmp(msg, "confirmation") == 0) {
            printf("Server caka na poslanie node addr\n");
        }

        printf("Posielanie client node addr\n");
        // poslanie adresy socketIn serveru
        if (send(sock, &socketInAddr, sizeof(socketInAddr), 0) < 0) {
            printError("Chyba - socketIn addr send");
        }

        // prijatie adresy od servera na ktoru sa ma pripojit
        struct sockaddr_in nextNodeAddr;
        if (recv(sock, &nextNodeAddr, sizeof(nextNodeAddr), 0) < 0) {
            printError("Chyba - nextNode addr recv");
        }
        close(sock);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sock, (struct sockaddr *) &nextNodeAddr, sizeof(nextNodeAddr)) < 0) {
            printError("Chyba - sock connect to next node.");
        }

        NODE node;
        node.id = 100;
        node.socketIn = socketIn;
        node.socketOut = sock;
        node.address = socketInAddr;
        pthread_t nodeThread;
        pthread_create(&nodeThread, NULL, receiveAndForward, (void *) &node);
        pthread_join(nodeThread, NULL);
        close(socketIn);
    }

    //uzavretie socketu <unistd.h>
    close(sock);

    return (EXIT_SUCCESS);
}
