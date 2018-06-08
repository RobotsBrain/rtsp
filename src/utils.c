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
#include <time.h>
#include <netdb.h>

#include "md5.h"



#define MAX_QUEUE_SIZE      20

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

    ptr = strstr((const char*)buf, s2);
    if(ptr == NULL) {
        result = NULL;
    } else {
        result = (char*)s1 + (ptr - buf);
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

// int create_udp_connect(const char *host, int server_port, int cliport)
// {
//     struct sockaddr_in addr;
//     int sock = -1;

//     if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
//         perror("socket");
//         return -1;
//     }

//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(server_port);
//     addr.sin_addr.s_addr = inet_addr(host);

//     if (addr.sin_addr.s_addr == INADDR_NONE) {
//         printf("Incorrect ip address!");
//         close(sock);
//         return -1;
//     }

//     return sock;
// }

int create_udp_connect(const char *host, int server_port, int cliport)
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
    server.sin_port = htons(server_port);

    if((bind(fd, (struct sockaddr *)&server, len)) < 0) {               
        printf("bind server port error"); 
        return -1;
    }

    rtp_address.sin_family = AF_INET;
    rtp_address.sin_addr.s_addr = inet_addr(host);
    rtp_address.sin_port = htons(cliport);

    result = connect(fd, (struct sockaddr *)&rtp_address, len);
    if(result == -1) {
        printf("connect socket error\n");
        return -1;
    }

    printf("host: %s, server port: %d, client port: %d\n", host, server_port, cliport);

    return fd;
}

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

int parse_domain_ip_address(const char *doname, char* ipAddr)
{
    char** pptr = NULL;
    struct hostent *hptr = NULL;
    char   str[32] = {0};

    if((hptr = gethostbyname(doname)) == NULL) {
        printf("host: %s\n", doname);
        return -1;
    }

    switch(hptr->h_addrtype) {
    case AF_INET:
    case AF_INET6:
        pptr = hptr->h_addr_list;
        for(; *pptr != NULL; pptr++) {
            strcpy(ipAddr, inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str)));
            return 0;
        }
        break;

    default:
        break;
    }

    return -1;
}

int parse_rtsp_url_info(char const* url, char* username, char* password, char* address, int* portNum, char* path)
{
    uint32_t const prefixLength = 7;  
    char const* from = &url[prefixLength];      
    char const* tmpPos = NULL;  
  
    if ((tmpPos = strchr(from, '@')) != NULL) {  
        // found <username> (and perhaps <password>).  
        char const* usernameStart = from;  
        char const* passwordStart = NULL;  
        char const* p = tmpPos;  
  
        if ((tmpPos = strchr(from, ':')) != NULL && tmpPos < p) {  
            passwordStart = tmpPos + 1;
            uint32_t passwordLen = p - passwordStart;  
            strncpy(password, passwordStart, passwordLen);
            password[passwordLen] = '\0'; // Set the ending character.  
        }  
              
        uint32_t usernameLen = 0;  
        if (passwordStart != NULL) {
            usernameLen = tmpPos - usernameStart;  
        } else {  
            usernameLen = p - usernameStart;      
        }         
        strncpy(username, usernameStart, usernameLen);
        username[usernameLen] = '\0';  // Set the ending character.  
  
        from = p + 1; // skip the '@'  
    }  
  
    const char* pathStart = NULL;  
    if ((tmpPos = strchr(from, '/')) != NULL) {  
        uint32_t pathLen = strlen(tmpPos + 1);  //Skip '/'  
        strncpy(path, tmpPos + 1, pathLen + 1);  
        pathStart = tmpPos;  
    }  
  
    // Next, will parse the address and port.  
    tmpPos = strchr(from, ':');  
    if (tmpPos == NULL) {  
        if (pathStart == NULL) {  
            uint32_t addressLen = strlen(from);  
            strncpy(address, from, addressLen + 1);  //Already include '\0'  
        } else {  
            uint32_t addressLen = pathStart - from;  
            strncpy(address, from, addressLen);  
            address[addressLen] = '\0';   //Set the ending character.  
        }

        *portNum = 554; // Has not the specified port, and will use the default value  
    } else if (tmpPos != NULL) {  
        uint32_t addressLen = tmpPos - from;
        strncpy(address, from, addressLen);  
        address[addressLen] = '\0';  //Set the ending character.  
        *portNum = strtoul(tmpPos + 1, NULL, 10);   
    }

    if(strlen(address) <= 0) {
        return -1;
    }

    return 0;
}

