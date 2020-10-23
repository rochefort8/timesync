#include <stdio.h> // For printf(), fprintf(), perror()
#include <sys/socket.h> // For socket(), connect(), send(), recv()
#include <arpa/inet.h> // For sockaddr_in, inet_addr()
#include <stdlib.h> // For atoi()
#include <string.h> // For exit(), memset()
#include <unistd.h> // For close()

#define RECVBUFSIZE     32 
#define PORT (49201) 

void errorHandler (char *msg) {
  perror(msg);
  exit(1);
}

struct lehx_timespec_format {
    struct {
        uint8_t h ;
        uint8_t l ;
    } year, month, day, hour, min, sec ;
    uint16_t    zero ;
} ;

int main (int argc, char *argv[]) {

    int sockfd; // socket descriptor
    struct sockaddr_in server_addr;
    char *serverIP;
    char *echoString = "123ABC";
    char recv_buffer[RECVBUFSIZE];
    unsigned int echoStringLen;
    int recvBytes, totalRecvBytes;

    // arguments validation
    if ((argc < 1) || (argc > 2)) {
        fprintf(stderr, "Usage: %s <Server IP> <Echo Word> [<Server Port>]\n", argv[0]);
        exit(1);
    }

    serverIP = argv[1];

    // create socket
    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        errorHandler("socket() failed");
    }

    // create address struct of echo server
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverIP);
    server_addr.sin_port = htons(PORT);

    // connect echo server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        errorHandler("connect() failed");
    }

    echoStringLen = strlen(echoString);

    // send message to echo server
    if (send(sockfd, echoString, echoStringLen, 0) != echoStringLen) {
        errorHandler("send() failed");
    }

    // recieve message from echo server
    totalRecvBytes = 0;
    printf("Recieved: ");
    while (totalRecvBytes < echoStringLen) {
        if ((recvBytes = recv(sockfd, recv_buffer, RECVBUFSIZE-1, 0)) <= 0) {
            errorHandler("recv() failed");
        }
        totalRecvBytes += recvBytes;
        recv_buffer[recvBytes] = '\0';
    }

    if (recvBytes == sizeof(struct lehx_timespec_format)) {
        int i ;
        uint8_t *p = (uint8_t *)recv_buffer ;
        for (i = 0;i < sizeof(struct lehx_timespec_format);i++) {
            printf("%02x ",*p++) ;
        } printf("\n") ;

    } else {
        printf("Size should be %lu ,but %d.",
            sizeof(struct lehx_timespec_format),recvBytes) ;
    }

    // close socket
    close(sockfd);
    exit(0);

}