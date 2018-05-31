/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>
Copyright (c) 2012, Bartosz Andrzej Zawada <bebour@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _SOCKETLIB_H_
#define _SOCKETLIB_H_

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int TCP_connect(const char *address, unsigned short port);

void TCP_disconnect(int sockfd);

int UDP_open_socket();

int UDP_get_destination(const char *ip, unsigned short port, struct sockaddr_in *dest);

void UDP_disconnect(int sockfd);

#define MAX_UDP_BIND_ATTEMPTS 100
#define RTP_BUFFER_SIZE 512


/* Get the host and the path of an RTSP uri
 * uri: Pointer to the null terminated string with the uri
 * host: Pointer to the null terminated string with the host. MUST be freed. null if it doesn't exist
 * path: Pointer to the null terminated string with the path. MUST be freed. null if it doesn't exist
 * return: 1 ok, 0 err
 */
int extract_uri(char *uri, char **host, char **path);

/* Binds two consecutive UDP ports and returns the first port number
 * rtp_sockfd: file descriptor for the rtp socket
 * rtcp_sockfd: file descriptor for the rtcp socket
 * returns: number of the first port or 0 if error
 */
int bind_UDP_ports(int *rtp_sockfd, int *rtcp_sockfd);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
