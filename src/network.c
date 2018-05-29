/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>
Copyright (c) 2012, Bartosz Andrzej Zawada <bebour@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>

#include "network.h"



int TCP_connect(const char *address, unsigned short port)
{
    int i = 0;
    int sockfd = -1;
    struct sockaddr_in addr;
    struct hostent *host;
    struct protoent *TCP = getprotobyname("TCP");

    /* Configuramos las partes conocidas de la dirección */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    bzero(addr.sin_zero, 8);

    /* Obtenemos las IPs del host */
    host = gethostbyname(address);
    if (host == 0 || host->h_addrtype != addr.sin_family)
        return -1;

    /* Se intenta conectar a todas las IPs obtenidas */
    for (i = 0; (host->h_addr_list)[i]; ++i) {

        sockfd = socket(host->h_addrtype, SOCK_STREAM, TCP->p_proto);
        //sockfd = socket(host->h_addrtype, SOCK_STREAM, 0);
        if (sockfd == -1)
            continue;

        /* Copiamos la direccion al struct sockaddr_in */
        memcpy(&(addr.sin_addr), host->h_addr_list[i], host->h_length);

        /* Conectamos con el host destino */
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(sockfd);
            continue;
        }

        return sockfd;
    }

    /* Se devuelve error en caso de no poder establecer conexión */
    return -1;
}

void TCP_disconnect(int sockfd)
{
    close(sockfd);
}

int UDP_open_socket()
{
    struct protoent *udp_proto = getprotobyname("UDP");

    return socket(AF_INET, SOCK_DGRAM, udp_proto->p_proto);
}

int UDP_get_destination(const char *ip, unsigned short port,
                        struct sockaddr_in *dest)
{
    struct hostent *host;

    host = gethostbyname(ip);
    if (!host)
        return -1;
    if (host->h_addrtype != AF_INET)
        return -1;
    if (host->h_addr_list[0] == 0)
        return -1;

    dest->sin_family = AF_INET;
    dest->sin_port = htons(port);
    memcpy(&(dest->sin_addr), host->h_addr_list[0], host->h_length);
    bzero(dest->sin_zero, 8);

    return 0;
}

void UDP_disconnect(int sockfd)
{
    close(sockfd);
}

/* Receive a message with a final empty line and possibly content
 * returns: 0 on bad message, -1 on closed socket, message length if ok */
int receive_message(int sockfd, char * buf, int buf_size)
{
    int ret = 0;
    int read = 0;
    char * tmp = 0;
    size_t tmp_size = 0;
    int content_length = 0;
    FILE * f = 0;

    f = fdopen(sockfd, "r");

    do {
        read = getline(&tmp, &tmp_size, f);
        if (read <= 0) {
            fclose(f);
            free(tmp);
            return(-1);
        }

        if (read+ret >= buf_size) {
            fclose(f);
            free(tmp);
            return(0);
        }

        if (!content_length && !strncmp(tmp, "Content-Length:", 15))
            if (sscanf(tmp, "Content-Length: %d", &content_length) < 1)
                content_length = 0;

        strcpy(buf+ret, tmp);
        ret += read;
    } while (tmp[0] != '\r');

    if (content_length) {
        content_length += ret;
        do {
            read = getline(&tmp, &tmp_size, f);
            if (read <= 0) {
                fclose(f);
                free(tmp);
                return(-1);
            }

            if (read+ret >= buf_size) {
                fclose(f);
                free(tmp);
                return(0);
            }

            memcpy(buf+ret, tmp, read);
            ret += read;
        } while (ret < content_length);
    }

    buf[ret] = 0;
    fprintf(stderr, "\n########## RECEIVED ##########\n%s\n##############################\n", buf);
    free(tmp);

    return ret;
}

int extract_uri(char *uri, char **host, char **path)
{
    char *path_start;
    int uri_len;

    *host = 0;
    *path = 0;

    if (!uri)
        return(0);
    uri_len = strlen(uri);
    if (uri_len < 8)
        return(0);
    *host = 0;
    *path = 0;

    if (memcmp(uri, "rtsp://", 7))
        return(0);
    uri += 7;
    uri_len -= 7;

    path_start = strstr(uri, "/");
    if (!path_start) {
        /* If there isn't a path */
        *host = malloc(uri_len + 1);
        if (!*host)
            return(0);
        strcpy(*host, uri);
        (*host)[uri_len] = 0;
        *path = 0;
        return(1);
    }
    /* If there is a '/' separator */
    if ((path_start - uri) == 0) {
        *host = 0;
        *path = 0;
        return(0);
    }
    /* Copy host */
    *host = malloc(path_start - uri + 1);
    if (!*host) {
        *host = 0;
        *path = 0;
        return(0);
    }
    strncpy(*host, uri, path_start - uri);
    (*host)[path_start - uri] = 0;

    /* Check if there is a path after the separator */
    if (uri_len > path_start - uri + 1) {
        *path = malloc(strlen(path_start) + 1);
        if (!*path)
            return(0);
        strcpy(*path, path_start);
        (*path)[strlen(path_start)] = 0;
    }

    return(1);
}

/* Binds two consecutive UDP ports and returns the first port number
 * rtp_sockfd: file descriptor for the rtp socket. -1 if error
 * rtcp_sockfd: file descriptor for the rtcp socket. -1 if error
 * returns: number of the first port or 0 if error
 */
int bind_UDP_ports(int *rtp_sockfd, int *rtcp_sockfd)
{
    unsigned short first_port;
    struct addrinfo hints, *res;
    char port_str[6];
    int st;
    int counter = 0;

    do {
        *rtp_sockfd = -1;
        *rtcp_sockfd = -1;
        if (counter == MAX_UDP_BIND_ATTEMPTS)
            return(0);
        first_port = rand();
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        if (!snprintf(port_str, 5, "%d", first_port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        *rtp_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (*rtp_sockfd == -1)
            return(0);
        st = bind(*rtp_sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (st == -1) {
            *rtp_sockfd = -1;
            continue;
        }

        if (!snprintf(port_str, 5, "%d", first_port + 1)) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }
        if (getaddrinfo(0, port_str, &hints, &res)) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }

        *rtcp_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (*rtcp_sockfd == -1) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            return(0);
        }
        st = bind(*rtcp_sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (st == -1) {
            close(*rtp_sockfd);
            *rtp_sockfd = -1;
            *rtcp_sockfd = -1;
            continue;
        }

        ++counter;
    } while (*rtp_sockfd == -1);

    return(first_port);
}

#define MAX_QUEUE_SIZE 20

int accept_tcp_requests(unsigned short port, int *sockfd, unsigned int *my_addr,
                        WORKER_CREATOR create_worker)
{
    int st;
    struct addrinfo hints, *res;
    char port_str[6];
    int tmp_sockfd;
    struct sockaddr_storage client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (!snprintf(port_str, 5, "%d", port)) {
        return -1;
    }

    if (getaddrinfo(0, port_str, &hints, &res)) {
        return -1;
    }

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1) {
        return -1;
    }

    st = bind(*sockfd, res->ai_addr, res->ai_addrlen);
    /* Copy server address to my_addr */
    *my_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    if (st == -1) {
        *sockfd = -1;
        return -1;
    }

    st = listen(*sockfd, MAX_QUEUE_SIZE);
    if (st == -1) {
        return -1;
    }

    for (;;) {
        tmp_sockfd = accept(*sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (tmp_sockfd == -1) {
            return -1;
        }

        st = create_worker(tmp_sockfd, &client_addr);
        if(st < 0) {
            close(tmp_sockfd);
        }
    }

    return 0;
}
