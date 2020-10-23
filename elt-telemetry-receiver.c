#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT (50050)

int main( int argc,char *argv[] )
{
    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t addrlen;
    char buf[2048];
    char senderstr[16];
    int n;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    while (1) {
        memset(buf, 0, sizeof(buf));
        addrlen = sizeof(senderinfo);
        n = recvfrom(sock, buf, sizeof(buf) - 1, 0,(struct sockaddr *)&senderinfo, &addrlen);
        inet_ntop(AF_INET, &senderinfo.sin_addr, senderstr, sizeof(senderstr));
        printf("recvfrom : %s, port=%d length=%d\n", senderstr, ntohs(senderinfo.sin_port),n);
        int i ;
        for (i = 0;i < n;i++) {
            printf("%02x ",buf[i]) ;
        }
        printf("\n") ;
    }
    close(sock);
    return 0;
}