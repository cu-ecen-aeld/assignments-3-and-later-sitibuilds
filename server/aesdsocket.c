#include <sys/socket.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT "9000"
#define SOCKDATAPATH "/var/tmp/aesdsocketdata"
#define RECVBUFSZ 1024

typedef struct recvbuf // Received packet structure
{
    unsigned char *buf;
    int len;
} recvbuf_t;

void recvbuf_free(recvbuf_t *p) {
    if (NULL == p) return;
    free(p->buf);
    free(p);
}

recvbuf_t *recvbuf_init() {
    recvbuf_t *p = malloc(sizeof (recvbuf_t));
    if (p == NULL) return NULL;
    p->buf = NULL;
    p->len = 0;
    return p;
}

bool recvbuf_append(recvbuf_t *p, unsigned char *buf, int len){
    if (NULL == p) return false;
    if (len <= 0) return true;
    else if (NULL == buf && len > 0) return false;

    int l = p->len + len; // Total len
    unsigned char *newbuf = malloc(l * sizeof (unsigned char));
    if (NULL != p->buf)
        memcpy(newbuf, p->buf, p->len);
    memcpy(newbuf+p->len, buf, len);

    unsigned char *prevbuf = p->buf;
    p->buf = newbuf;
    p->len = l;
    if (NULL != prevbuf) free(prevbuf);
    return true;
}

int sockfd = -1;
int datafd = -1;
int acceptfd = -1;

void graceful_exit(int signum) {
    if (-1 != sockfd) {
        close(sockfd);
    }
    if (-1 != datafd) {
        close(datafd);
    }
    if (-1 != acceptfd) {
        close(acceptfd);
    }
    if (0 == access(SOCKDATAPATH, F_OK)) remove(SOCKDATAPATH);
    syslog(LOG_USER | LOG_DEBUG, "Caught signal, exiting");
    closelog();
    exit(0);
}

void cpy_fd(int srcfd, int sockfd) {
    if (-1 == lseek(srcfd, 0, SEEK_SET)){ 
        perror("lseek");
        return;
    }
    unsigned char buf[RECVBUFSZ];
    int nbytes = read(srcfd, buf, RECVBUFSZ);
    while (nbytes != -1 && nbytes != 0){
        send(sockfd, buf, nbytes, 0);
        memset(buf, 0, RECVBUFSZ);
        nbytes = read(srcfd, buf, RECVBUFSZ);
    }
}

char *getaddrstr(struct sockaddr *addr){
    int af; // address family
    void *_addr;
    char *buf;
    int bufsize;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *) addr;
        _addr = (void *) &sin->sin_addr;
        af = AF_INET;
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) addr;
        _addr = (void *) &sin6->sin6_addr;
        af = AF_INET6;
    }
    buf = malloc(INET6_ADDRSTRLEN);
    if (NULL == inet_ntop(af, _addr, buf, INET6_ADDRSTRLEN)) {
        perror("inet_ntop");
        free(buf);
    } else return buf;
    return NULL;
}

int main(int argc, char **argv){
    if (argc > 2) {
        printf("Unrecognized arguments. Expected format: %s [-d]`n", argv[0]);
        exit(-1);
    }

    openlog(NULL, 0, LOG_USER);

    signal(SIGINT, graceful_exit);
    signal(SIGTERM, graceful_exit);

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 
    struct addrinfo *ai_info;
    if (0 != getaddrinfo(NULL, PORT, &hints, &ai_info)){
        perror("getaddrinfo");
        closelog();
        exit(-1);
    }

    sockfd = socket(ai_info->ai_family, ai_info->ai_socktype, ai_info->ai_protocol);
    if (-1 == sockfd){
        perror("socket");
        closelog();
        exit(-1);
    }

    if (0 != bind(sockfd, ai_info->ai_addr, ai_info->ai_addrlen)){
        perror("bind");
        close(sockfd);
        closelog();
        exit(-1);
    }

    if (argc == 2 && 0 == strcmp(argv[1], "-d")){
        if (0 != daemon(0, 0)) {
            perror("daemon");
            exit(-1);
        }
    }

    const char *datapath = SOCKDATAPATH;
    datafd = open(datapath, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (-1 == datafd){
        perror("open");
        close(sockfd);
        closelog();
        exit(-1);
    }

    if (0 != listen(sockfd, 1)){
        perror("listen");
        close(sockfd);
        closelog();
        exit(-1);
    }

    struct sockaddr acceptaddr;
    socklen_t addrsz = sizeof acceptaddr;
    acceptfd = accept(sockfd, &acceptaddr, &addrsz);
    if (-1 == acceptfd) {
        perror("accept");
        close(sockfd);
        closelog();
        exit(-1);
    }
    char *addrstr = getaddrstr(&acceptaddr);
    syslog(LOG_USER | LOG_DEBUG, "Accepted connection from %s", addrstr);
    if (NULL != addrstr) free(addrstr);
    addrstr = NULL;
    recvbuf_t *packt_buf = recvbuf_init();
    while (1) {
        unsigned char buf[RECVBUFSZ];
        int nbytes = recv(acceptfd, buf, RECVBUFSZ, 0);
        if (0 == nbytes) {
            // Client closed connection
            // Accept new
            acceptfd = accept(sockfd, &acceptaddr, &addrsz);
            addrstr = getaddrstr(&acceptaddr);
            syslog(LOG_USER | LOG_DEBUG, "Accepted connection from %s", addrstr);
            if (NULL != addrstr) free(addrstr);
            addrstr = NULL;

        } else if (-1 == nbytes) {
            perror("recv");
            close(sockfd);
            closelog();
            exit(-1);
        } else {
            unsigned char *bbuf = buf;
            int i = 0;
            bool appendbuf_flag = false; // Should we append the current bbuf to our in-memory packet?
            while (i < nbytes){
                if (bbuf[i] == '\n') {
                    if (!recvbuf_append(packt_buf, bbuf, i+1)){
                        perror("recvbuf_append");
                        graceful_exit(-1);
                    }
                    // Write packet to data file
                    write(datafd, packt_buf->buf, packt_buf->len);
                    // Send contents of data file to recipient
                    cpy_fd(datafd, acceptfd);
                    // reset
                    recvbuf_free(packt_buf);
                    packt_buf = recvbuf_init();
                    nbytes -= i + 1;
                    if (nbytes > 0) bbuf = bbuf + i + 1;
                    i = 0;
                    appendbuf_flag = false;
                } else {
                    appendbuf_flag = true;
                    i += 1;
                }
            }
            if (appendbuf_flag && nbytes > 0) {
                if (!recvbuf_append(packt_buf, bbuf, nbytes)){
                    perror("recvbuf_append");
                    graceful_exit(-1);
                }
            }
        }
    }
    
    close(sockfd);
    closelog();
}