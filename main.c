#include "definitions.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printError("Klienta je nutne spustit s nasledujucimi argumentmi: adresa port pouzivatel.");
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

    /*char msg[10];
    fflush(stdout);
    printf("Zadajte pocet uzlov (3-20): ");
    scanf("%s", msg);*/
    char msg[1024];
    sprintf(msg, "10");
    if (send(sock, msg, sizeof(msg), 0) < 0) {
        printError("Chyba - send.");
    }

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

    /*char msg[1024];
    sprintf(msg, "Test: URL poslane klientom.");
    if (send(sock, msg, sizeof(msg), 0) < 0) {
        printError("Chyba - send.");
    }*/

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

    //uzavretie socketu <unistd.h>
    close(sock);

    return (EXIT_SUCCESS);
}
