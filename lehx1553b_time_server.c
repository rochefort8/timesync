#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#define PORT        (49201)
#define FD_INVALID  (-1)
#define MAX_CLIENTS (20)
#define RECVBUFSIZE (2048)

struct lehx_timespec_format {
    struct {
        uint8_t h ;
        uint8_t l ;
    } year, month, day, hour, min, sec ;
    uint16_t    zero ;
} ;

uint8_t htod(uint8_t hex)
{
    uint8_t upper = hex / 10 ;
    uint8_t lower = hex % 10 ;
    return ((upper<<4) | lower);
}

int set_current_time_in(struct lehx_timespec_format *buf)
{
    struct tm *now; 
    struct timeval total_usec; 

    int ret = gettimeofday(&total_usec, NULL) ;
    if (ret == -1) {
        fprintf(stderr,"gettimeofday ERRNO=%d", errno);
        return -1;
    }

    // timeval -> Year/Month/Day/...
    now     = localtime(&total_usec.tv_sec); 

    bzero(buf,sizeof(struct lehx_timespec_format)) ;

    int year = now->tm_year+1900 ;
    buf->year.h     = htod(year / 100) ;
    buf->year.l     = htod(year % 100) ;   
    buf->month.l    = htod(now->tm_mon+1) ; 
    buf->day.l      = htod(now->tm_mday) ; 
    buf->hour.l     = htod(now->tm_hour) ;
    buf->min.l      = htod(now->tm_min) ;
    buf->sec.l      = htod(now->tm_sec) ;

//    usec = total_usec.tv_usec; //マイクロ秒

    int i ;
    uint8_t *p = (uint8_t *)buf ;
    for (i = 0;i < sizeof(struct lehx_timespec_format);i++) {
        printf("%02x ",*p++) ;
    } printf("\n") ;
//    printf("%02x/%02x/%02x %02x:%02x:%02x \n",year,month,day,hour,min,sec);

    return 0 ;
}

int handle_tcp_client (int sockfd )
{
    char    buf[RECVBUFSIZE];

    int cnt = recv( sockfd , buf, sizeof(buf), 0 );
    if (cnt > 0 ) {
        struct lehx_timespec_format t ;
        set_current_time_in (&t) ;
        send( sockfd, &t, sizeof(struct lehx_timespec_format), 0) ;
    }
    return cnt ;
}

int main(int argc, char** argv)
{
    int     sockfd;             
    int     client_sockfd_array[MAX_CLIENTS];        
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);
    struct sockaddr_in from_addr;
    int     cnt;

    for ( int i = 0; i < sizeof(client_sockfd_array)/sizeof(client_sockfd_array[0]); i++ ){
        client_sockfd_array[i] = FD_INVALID ;
    }

    if (( sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
        fprintf( stdout, "socket error : fd = %d\n", sockfd );
        return -1;
    }

    addr.sin_family      = AF_INET;
    addr.sin_port        = ntohs(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ( bind( sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
        fprintf( stdout, "bind error\n" );
        return -1;
    }

    if ( listen( sockfd, 1 ) < 0 ) {
        fprintf( stdout, "listen error\n" );
        return -1;
    }

    int     maxfd;        
    fd_set  rfds;           
    struct timeval  tv;     

    while ( 1 ){

        FD_ZERO( &rfds );
        FD_SET( sockfd, &rfds );
        maxfd = sockfd;

        for ( int i = 0; i < sizeof(client_sockfd_array)/sizeof(client_sockfd_array[0]); i++ ){
            if ( client_sockfd_array[i] != FD_INVALID ){
                FD_SET( client_sockfd_array[i], &rfds );
                if ( client_sockfd_array[i] > maxfd ) maxfd = client_sockfd_array[i];
            }
        }
        // Set timeout to 10sec 
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        cnt = select( maxfd+1, &rfds, NULL, NULL, &tv );
        if (cnt == 0 ) {
            // Timeout
            continue ;
        }
        if ( cnt < 0 ){
            // Continue when signal catches
            if ( errno == EINTR ) continue;
            // Terminate for other cases
            goto end;
        } 
        if ( FD_ISSET( sockfd, &rfds )){

            // Establish connection
            for ( int i = 0; i < sizeof(client_sockfd_array)/sizeof(client_sockfd_array[0]); i++ ){
                if ( client_sockfd_array[i] == FD_INVALID ){
                    if (( client_sockfd_array[i] = accept(sockfd, (struct sockaddr *)&from_addr, &len )) < 0 ) {
                        goto end;
                    }
                    fprintf( stdout, "socket:%d  connected. \n", client_sockfd_array[i] );
                    break;
                }
            }
        }

        for ( int i = 0; i < sizeof(client_sockfd_array)/sizeof(client_sockfd_array[0]); i++ ){
            if ( FD_ISSET( client_sockfd_array[i], &rfds )){

                int ret = handle_tcp_client (client_sockfd_array[i]) ;

                if ( ret > 0 ) {

                } else if ( ret == 0 ) {
                    fprintf( stdout, "socket:%d  disconnected. \n", client_sockfd_array[i] );
                    close( client_sockfd_array[i] );
                    client_sockfd_array[i] = -1;
                } else {
                    goto end;
                }

            }
        }
    }
end:
    for ( int i = 0; i < sizeof(client_sockfd_array)/sizeof(client_sockfd_array[0]); i++ ){
        close(client_sockfd_array[i]);
    }
    close(sockfd);
    return 0;
}