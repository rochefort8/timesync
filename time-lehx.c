#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <time.h>
#include <errno.h>


#define RECVBUFSIZE     32 
#define PORT (49201) 
#define SERVER_IP_ADDRESS "18.180.242.174"

struct lehx_timespec_format {
    struct {
        uint8_t h ;
        uint8_t l ;
    } year, month, day, hour, min, sec ;
    uint16_t    zero ;
} ;

static int sockfd = -1 ;

static inline int dtoh(int dec) {
    return ((dec >> 4) * 10 + (dec & 0x0f));
}

static int lehx_open(void)
{
    if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        return -1 ;
    }
    return 0 ;
}

static int lehx_close(void)
{
    close(sockfd) ;
    return 0 ;
}

static int system_set_time(struct tm *timeptr)
{
    struct timespec ts ;
    int ret ;

//    printf(asctime(timeptr));

    ts.tv_sec   = mktime(timeptr) ;
    ts.tv_nsec  = 0 ;

    ret = clock_settime(CLOCK_REALTIME, &ts);
    if (ret != 0) {
        printf("Could not set time. errno=%d\n",errno) ;
        return -1 ;
    }
    return 0 ;
}

static int lehx_send_command(void)
{
    char *address = SERVER_IP_ADDRESS ;
    struct sockaddr_in server_addr;
    char *echoString = "101000" ;
    int send_length ;

    // create address struct of echo server
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address);
    server_addr.sin_port = htons(PORT);

    // connect echo server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        return -1 ;
    }

    send_length = strlen(echoString) ;

    // send message to echo server
    if (send(sockfd, echoString, send_length, 0) != send_length) {
        perror("send() failed");
        return -1 ;
    }
    return 0 ;
}

static int lehx_receive_response(void)
{
    int recvBytes, totalRecvBytes;
    int ret ;
    char recv_buffer[RECVBUFSIZE] ;

    totalRecvBytes = 0;
    
    while (totalRecvBytes < sizeof(struct lehx_timespec_format)) {
        if ((recvBytes = recv(sockfd, recv_buffer, RECVBUFSIZE-1, 0)) <= 0) {
            perror ("recv() failed");
            return -1 ;
        }
        totalRecvBytes += recvBytes;
        recv_buffer[recvBytes] = '\0';
    }

    if (recvBytes == sizeof(struct lehx_timespec_format)) {
# if 0        
        int i ;
        uint8_t *p = (uint8_t *)recv_buffer ;
        for (i = 0;i < sizeof(struct lehx_timespec_format);i++) {
            printf("%02x ",*p++) ;
        } printf("\n") ;
#endif
        struct lehx_timespec_format *pt = (struct lehx_timespec_format *)recv_buffer ;
        struct tm now ;

        // TODO
        // valodation of time info

        now.tm_sec     = dtoh ((int)pt->sec.l  & 0x7f ) ;
        now.tm_min     = dtoh ((int)pt->min.l  & 0x7f ) ;
        now.tm_hour    = dtoh ((int)pt->hour.l & 0x3f ) ;
        now.tm_mday    = dtoh ((int)pt->day.l  & 0x3f) ;

        // TODO !!
        now.tm_wday    = 0 ;
        now.tm_mon     = dtoh ((int)pt->month.l  & 0x1f ) - 1 ;
        now.tm_year    = dtoh ((int)pt->year.l)  + 100 ;

        printf("TIME : %s", asctime(&now)) ;
        ret = system_set_time(&now) ;

    } else {
        printf("Size should be %lu ,but %d.",
            sizeof(struct lehx_timespec_format),recvBytes) ;
        return -1 ;
    }
    return 0 ;
}

static int lehx_do_timesync()
{
    int ret ;

    ret = lehx_open() ;
    if ( ret != 0 ) {
        printf ("Error") ;
        return -1 ;
    }

    ret = lehx_send_command() ;
    if ( ret != 0 ) {
        printf ("Error") ;
        goto end ;
    }

    ret = lehx_receive_response () ; 
    if ( ret != 0 ) {
        printf ("Error") ;
        goto end ;
    }

end :    
    lehx_close();
    return ret ;
}

int main (int argc, char *argv[]) {
    lehx_do_timesync () ;
    return 0 ;
}
