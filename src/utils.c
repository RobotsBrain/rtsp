/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "md5.h"



char *strnstr(const char *s1, const char *s2, size_t n)
{
    char *buf = NULL;
    char *ptr = NULL;
    char *result = NULL;

    buf = (char*)malloc(n + 1);
    if(buf == NULL) {
        return NULL;
    }

    memset(buf, 0, n + 1);

    strncpy(buf, s1, n);

    ptr = strstr(buf, s2);
    if (!ptr) {
        result = 0;
    } else {
        result = s1 + (ptr - buf);
    }

    free(buf);
    buf = NULL;

    return result;
}

static unsigned int md_32(unsigned char *string, int length)
{
    MD5_CTX context;

    union {
        char         c[16];
        unsigned int x[4];
    } digest;

    unsigned int r = 0;
  
    MD5Init(&context);
    MD5Update(&context, string, length);
    MD5Final((unsigned char *)&digest, &context);

    for (int i = 0; i < 3; i++) {
        r ^= digest.x[i];
    }

    return r;
}

unsigned int random32(int type)
{
    struct {
        int type;
        struct timeval tv;
        clock_t cpu;
        pid_t pid;
        int hid;
        uid_t uid;
        gid_t gid;
        struct utsname name;
    } s;

    gettimeofday(&s.tv, NULL);
    uname(&s.name);
    s.type = type;
    s.cpu = clock();
    s.pid = getpid();
    s.hid = gethostid();
    s.uid = getuid();
    s.gid = getgid();

    return md_32((unsigned char *)&s, sizeof(s));
}

unsigned long random_seq()
{
    unsigned long seed;

    srand((unsigned)time(NULL));  
    seed = 1 + (unsigned int)(rand()%(0xFFFF)); 
    
    return seed;
}

int create_udp_connect(const char *host, int port, int cliport)
{
    int fd, reuse = 1;
    struct sockaddr_in server, rtp_address;
    int result = -1;
    int len = sizeof(struct sockaddr_in);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0){
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); 
  
    server.sin_family = AF_INET;         
    server.sin_addr.s_addr = htonl(INADDR_ANY);              
    server.sin_port = htons(port);

    if((bind(fd, (struct sockaddr *)&server, len)) < 0) {               
        printf("bind rtsp server port error"); 
        return -1;
    }

    rtp_address.sin_family = AF_INET;
    rtp_address.sin_addr.s_addr = inet_addr(host);
    rtp_address.sin_port = htons(cliport);

    result = connect(fd, (struct sockaddr *)&rtp_address, len);
    if(result == -1) {
        printf("connect vrtp socket error\n");
        return -1;
    }

    printf("host: %s, server port: %d, client port: %d\n", host, port, cliport);

    return fd;
}

#define MAX_QUEUE_SIZE      20

int create_tcp_server(const char *host, int port)
{
    int ret = -1;
    int sockfd = -1;
    struct sockaddr_in servAddr;

    printf("port: %d\n", port);

    memset(&servAddr, 0, sizeof(struct sockaddr_in));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("cannot open socket ");
        return -1;
    }
 
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
     
    ret = bind(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if(ret < 0) {
        close(sockfd);
        perror("cannot bind port ");
        return -1;
    }

    ret = listen(sockfd, MAX_QUEUE_SIZE);
    if(ret < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

