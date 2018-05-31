/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <pthread.h>

#include "hashtable.h"
#include "rtsp.h"
#include "rtsp_server.h"


#define REQ_BUFFER          4096
#define MAX_RTSP_WORKERS    20 /* Number of processes listening for rtsp connections */
#define MAX_QUEUE_SIZE      20


typedef struct {
    unsigned char *media_uri; /* Uri for the media */
    unsigned int ssrc; /* Use the ssrc to locate the corresponding RTP session */
} INTERNAL_MEDIA;

typedef struct {
    unsigned char *global_uri; /* Global control uri */
    INTERNAL_MEDIA (*medias)[1];
    int n_medias;
} INTERNAL_SOURCE;

typedef struct {
    int Session;
    int CSeq;
    struct sockaddr_storage client_addr;
    INTERNAL_SOURCE (*sources)[1];
    int n_sources;
} INTERNAL_RTSP;

typedef struct {
    int         used;
    int         sockfd;
    time_t      ct;
    pthread_t   tid;
    struct sockaddr_storage client_addr;
    void*       pcontext;
} WORKER;

typedef struct rtsp_server_hdl_ {
    unsigned short  port;
    pthread_t       rstid;
    hashtable*      psess_hash;
    WORKER          workers[MAX_RTSP_WORKERS];
} rtsp_server_hdl_s;


RTSP_RESPONSE *rtsp_server_options(WORKER *self, RTSP_REQUEST *req)
{
    return rtsp_options_res(req);
}

RTSP_RESPONSE *rtsp_server_describe(WORKER *self, RTSP_REQUEST *req)
{
    if (1/* TODO: Check if file exists */) {
        return rtsp_describe_res(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_setup(WORKER *self, RTSP_REQUEST *req,
                                INTERNAL_RTSP *rtsp_info)
{
    int i, j, st;
    int global_uri_len;
    char * end_global_uri;
    RTSP_RESPONSE *res;
    int *Session;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri) {
        end_global_uri = strstr(req->uri, "/video");
    }

    if (!end_global_uri) {
        return rtsp_notfound(req);
    }

    global_uri_len = end_global_uri - req->uri;

    if (req->cast == UNICAST) {
        if (1/* TODO: Check if file exists */) {
            /* Create new rtsp_info */
            if (!rtsp_info) {
                fprintf(stderr, "Creating new session\n");
                rtsp_info = malloc(sizeof(INTERNAL_RTSP));

                rtsp_info->n_sources = 0;
                rtsp_info->sources = 0;
                rtsp_info->Session = req->Session;

                memcpy(&(rtsp_info->client_addr), &(self->client_addr), sizeof(struct sockaddr_storage));

                Session = malloc(sizeof(unsigned int));
                *Session = req->Session;
                st = puthashtable(&prshdl->psess_hash, Session, rtsp_info);
            }

            fprintf(stderr, "Getting session: %d\n", req->Session);
            rtsp_info = gethashtable(&prshdl->psess_hash, &(req->Session));
            /* Check if the session has disappeared for some reason */
            if (!rtsp_info) {
                fprintf(stderr, "caca3\n");
                return rtsp_servererror(req);
            }

            /* Check if the global uri already exists */
            for (i = 0; i < rtsp_info->n_sources; ++i)
                if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
                    break;

            /* If it doesn't exist create it */
            if (i == rtsp_info->n_sources) {
                rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->n_sources));
                if (!rtsp_info->sources) {
                    fprintf(stderr, "caca4\n");
                    return rtsp_servererror(req);
                }
                /* Copy global uri */
                rtsp_info->sources[i]->global_uri = malloc (global_uri_len + 1);
                if (!rtsp_info->sources[i]->global_uri) {
                    fprintf(stderr, "caca5\n");
                    return rtsp_servererror(req);
                }
                memcpy(rtsp_info->sources[i]->global_uri, req->uri, global_uri_len);
                rtsp_info->sources[i]->global_uri[global_uri_len] = 0;
                rtsp_info->sources[i]->medias = 0;
                rtsp_info->sources[i]->n_medias = 0;
            }

            /* Check if the media uri already exists */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                    break;
            /* If it doesn't exist create it */
            if (j == rtsp_info->sources[i]->n_medias) {
                rtsp_info->sources[i]->medias = realloc(rtsp_info->sources[i]->medias, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->sources[i]->n_medias));
                if (!rtsp_info->sources[i]->n_medias) {
                    fprintf(stderr, "caca6\n");
                    return rtsp_servererror(req);
                }
                /* Copy global uri */
                rtsp_info->sources[i]->medias[j]->media_uri = malloc (strlen(req->uri) + 1);
                if (!rtsp_info->sources[i]->medias[j]->media_uri) { 
                    fprintf(stderr, "caca7\n");
                    return rtsp_servererror(req);
                }
                memcpy(rtsp_info->sources[i]->medias[j]->media_uri, req->uri, strlen(req->uri));
                rtsp_info->sources[i]->medias[j]->media_uri[strlen(req->uri)] = 0;

                /* Put the client udp port in the structure */
                ((struct sockaddr_in*)&rtsp_info->client_addr)->sin_port = htons(req->client_port);

                rtsp_info = gethashtable(&prshdl->psess_hash, &(req->Session));
                /* Check if the session has disappeared for some reason */
                if (!rtsp_info) {
                    fprintf(stderr, "caca30\n");
                    return rtsp_servererror(req);
                }
                /* Assign ssrc */
                rtsp_info->sources[i]->medias[j]->ssrc = 3333; 
            }

            /* TODO: this */
            res = rtsp_setup_res(req, 4534, 0, UNICAST, 0);
            return res;
        } else {
            return rtsp_notfound(req);
        }
    } else {
        /* TODO: Multicast */
        fprintf(stderr, "caca8\n");
        return rtsp_servererror(req);
    }
}

RTSP_RESPONSE *server_simple_command(WORKER *self, RTSP_REQUEST *req,
                                    RTSP_RESPONSE *(*rtsp_command)(RTSP_REQUEST *))
{
    char *end_global_uri;
    int global_uri_len;
    int global_uri;
    unsigned int ssrc;
    INTERNAL_RTSP *rtsp_info;
    int i;
    int j;
    int st;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    global_uri = 0;
    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri)
        end_global_uri = strstr(req->uri, "/video");
    if (!end_global_uri) {
        end_global_uri = req->uri + strlen(req->uri);
        global_uri = 1;
    }
    global_uri_len = end_global_uri - req->uri;

    if (1/* TODO: Check if file exists */) {
        rtsp_info = gethashtable(&prshdl->psess_hash, &req->Session);
        /* Check if the session has disappeared for some reason */
        if (!rtsp_info) {
            fprintf(stderr, "caca9\n");
            return rtsp_servererror(req);
        }

        /* Get global uri */
        for (i = 0; i < rtsp_info->n_sources; ++i)
            if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
                break;

        /* If it doesn't exist return error*/
        if (i == rtsp_info->n_sources) {
            fprintf(stderr, "caca10\n");
            return rtsp_servererror(req);
        }

        /* If the uri isn't global */
        if (!global_uri) {
            /* Get the media uri */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri))) {
                    break;
                }
            }
            /* If it doesn't exist return error */
            if (j == rtsp_info->sources[i]->n_medias) {
                fprintf(stderr, "caca11\n");
                return rtsp_servererror(req);
            }

            /* Apply to only one media */
            // ssrc = rtsp_info->sources[i]->medias[j]->ssrc;
        } else {
            /* Apply to all the medias in the global uri */
            fprintf(stderr, "medias: %d\n", rtsp_info->sources[i]->n_medias);
            st = 1;
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {

            }

            if (!st) {
                fprintf(stderr, "caca13\n");
                return(rtsp_servererror(req));
            }
        }

        return rtsp_command(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_play(WORKER *self, RTSP_REQUEST *req)
{
    return server_simple_command(self, req, rtsp_play_res);
}

RTSP_RESPONSE *rtsp_server_pause(WORKER *self, RTSP_REQUEST *req)
{
    return server_simple_command(self, req, rtsp_pause_res);
}

RTSP_RESPONSE *rtsp_server_teardown(WORKER *self, RTSP_REQUEST *req)
{
    int i, j;
    INTERNAL_RTSP *rtsp_info;
    RTSP_RESPONSE *res;
    char *end_global_uri;
    int global_uri = 0;
    int global_uri_len;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    res = server_simple_command(self, req, rtsp_teardown_res);

    fprintf(stderr, "uri: %s\n", req->uri);

    end_global_uri = strstr(req->uri, "/audio");
    if (!end_global_uri) {
        end_global_uri = strstr(req->uri, "/video");
    }

    if (!end_global_uri) {
        global_uri = 1;
    }

    if (global_uri) {
        global_uri_len = strlen(req->uri);
    } else {
        global_uri_len = end_global_uri - req->uri;
    }

    rtsp_info = gethashtable(&prshdl->psess_hash, &(req->Session));
    if (!rtsp_info) {
        fprintf(stderr, "caca20\n");
        free(res);
        return rtsp_servererror(req);
    }
    /* Check if the global uri exists */
    for (i = 0; i < rtsp_info->n_sources; ++i)
        if (strlen(rtsp_info->sources[i]->global_uri) == global_uri_len &&
                !memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len))
            break;
    if (i == rtsp_info->n_sources) {
        fprintf(stderr, "caca21\n");
        free(res);
        return rtsp_servererror(req);
    }
    if (global_uri) {
        /* Delete all medias with this uri */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
            free(rtsp_info->sources[i]->medias[j]->media_uri);
        /* Free medias array */
        free(rtsp_info->sources[i]->medias);
        /* Move the other sources */
        memmove(&(rtsp_info->sources[i]), &(rtsp_info->sources[i+1]), rtsp_info->n_sources - i - 1);
        (--rtsp_info->n_sources);
        /* Change size of sources array */
        rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * rtsp_info->n_sources);
    } else {
        /* Check if the media uri exists */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
            if (!strncmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                break;
        if (j == rtsp_info->sources[i]->n_medias) {
            fprintf(stderr, "caca22\n");
            free(res);
            return rtsp_servererror(req);
        }
        /* Free this media */
        free(rtsp_info->sources[i]->medias[j]->media_uri);
        /* Move the other medias */
        memmove(&(rtsp_info->sources[i]->medias[j]), &(rtsp_info->sources[i]->medias[j+1]), rtsp_info->sources[i]->n_medias - j - 1);
        --(rtsp_info->sources[i]->n_medias);
        /* Change size of sources array */
        rtsp_info->sources[i]->medias = realloc(rtsp_info->sources[i]->medias, sizeof(INTERNAL_MEDIA) * rtsp_info->sources[i]->n_medias);

    }

    return res;
}

static int get_session(WORKER *self, int *ext_session, INTERNAL_RTSP **rtsp_info)
{
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    if (*ext_session > 0) {
        /* Check that the session truly exists */
        *rtsp_info = gethashtable(&prshdl->psess_hash, ext_session);
        if (*rtsp_info) {
            return 1;
        }
    }

    while (*ext_session < 1) {
        *ext_session = rand();
        *rtsp_info = gethashtable(&prshdl->psess_hash, ext_session);
        if (*rtsp_info) {
            *ext_session = -1;
        }
    }

    return 1;
}

static int receive_message(int sockfd, char * buf, int buf_size)
{
    int ret = 0;
    int read = 0;
    char* tmp = NULL;
    size_t tmp_size = 0;
    int content_length = 0;
    FILE* f = 0;

    f = fdopen(sockfd, "r");

    do {
        read = getline(&tmp, &tmp_size, f);
        if (read <= 0) {
            fclose(f);
            free(tmp);
            return -1;
        }

        if (read + ret >= buf_size) {
            fclose(f);
            free(tmp);
            return 0;
        }

        if (!content_length && !strncmp(tmp, "Content-Length:", 15)) {
            if (sscanf(tmp, "Content-Length: %d", &content_length) < 1) {
                content_length = 0;
            }
        }

        strcpy(buf + ret, tmp);
        ret += read;
    } while(tmp[0] != '\r');

    if (content_length) {
        content_length += ret;
        do {
            read = getline(&tmp, &tmp_size, f);
            if (read <= 0) {
                fclose(f);
                free(tmp);
                return -1;
            }

            if (read + ret >= buf_size) {
                fclose(f);
                free(tmp);
                return 0;
            }

            memcpy(buf + ret, tmp, read);
            ret += read;
        } while(ret < content_length);
    }

    buf[ret] = 0;
    fprintf(stderr, "\n\n########################## RECEIVED ##########################\n%s", buf);
    free(tmp);

    return ret;
}

void *rtsp_server_worker_proc(void *arg)
{
    WORKER *self = (WORKER *)arg;
    int sockfd = self->sockfd;
    char buf[REQ_BUFFER];
    int ret;
    int st;
    RTSP_REQUEST req[1];
    RTSP_RESPONSE *res;
    int server_error;
    int Session;
    INTERNAL_RTSP *rtsp_info = NULL;
    int CSeq = 0;

    for(;;) {
        rtsp_info = 0;
        Session = -1;

        do {
            server_error = 0;

            st = receive_message(sockfd, buf, REQ_BUFFER);
            if (st == -1) {
                return 0;
            } else if (!st) {
                server_error = 1;
            }

            st = unpack_rtsp_req(req, buf, st);
            /* If there was an error return err*/
            if (server_error && st) {
                fprintf(stderr, "caca1\n");
                res = rtsp_servererror(req);
                if (res) {
                    st = pack_rtsp_res(res, buf, REQ_BUFFER);
                    if (st) {
                        buf[st] = 0;
                        send(sockfd, buf, st, 0);
                    }
                    free_rtsp_res(&res);
                }
            } else if (server_error || !st) {
                memcpy(buf, "RTSP/1.0 500 Internal server error\r\n\r\n", 38);
                buf[38] = 0;
                send(sockfd, buf, strlen(buf), 0);
            }
        } while (server_error || !st);
        
        st = get_session(self, &(req->Session), &rtsp_info);
        
        if (req->CSeq <= CSeq) {
            res = rtsp_servererror(req);
        } else {
            CSeq = req->CSeq;
            switch (req->method) {
            case OPTIONS:
                req->Session = 0;
                res = rtsp_server_options(self, req);
                break;

            case DESCRIBE:
                req->Session = 0;
                res = rtsp_server_describe(self, req);
                break;

            case SETUP:
                res = rtsp_server_setup(self, req, rtsp_info);
                break;

            case PLAY:
                fprintf(stderr, "Recibido play\n");
                if (!rtsp_info) {
                    res = rtsp_servererror(req);
                } else {
                    res = rtsp_server_play(self, req);
                }
                break;

            case PAUSE:
                if (!rtsp_info) {
                    res = rtsp_servererror(req);
                } else {
                    res = rtsp_server_pause(self, req);
                }
                break;

            case TEARDOWN:
                if (!rtsp_info) {
                    res = rtsp_servererror(req);
                } else {
                    res = rtsp_server_teardown(self, req);
                }
                break;

            default:
                fprintf(stderr, "caca2\n");
                res = rtsp_servererror(req);
                break;
            }

            self->ct = time(0);
        }

        if (res) {
            st = pack_rtsp_res(res, buf, REQ_BUFFER);
            if (st) {
                fprintf(stderr, "\n########################## RESPONSE ##########################\n%s\n", buf);
                send(sockfd, buf, st, 0);
            }
            free_rtsp_res(&res);
        }

        if (req->uri) {
            free(req->uri);
        }
    }

    return NULL;
}

int rtsp_worker_create(rtsp_server_hdl_s* prshdl, int sockfd, struct sockaddr_storage *client_addr)
{
    int ret = -1;
    int i = 0;

    do {
        for(i = 0; i < MAX_RTSP_WORKERS; ++i) {
            if (prshdl->workers[i].used == 0) {
                break;
            }       
        }

        if(i >= MAX_RTSP_WORKERS) {
            break;
        }

        memset(&prshdl->workers[i], 0, sizeof(WORKER));

        prshdl->workers[i].sockfd = sockfd;
        prshdl->workers[i].ct = time(0);
        prshdl->workers[i].pcontext = prshdl;

        memcpy(&(prshdl->workers[i].client_addr), client_addr, sizeof(struct sockaddr_storage));

        ret = pthread_create(&prshdl->workers[i].tid, 0, rtsp_server_worker_proc, &prshdl->workers[i]);
        if(ret) {
            break;
        }

        prshdl->workers[i].used = 1;
        ret = 0;
    } while(0);

    return ret;
}

void *rtsp_server_proc(void *arg)
{
    int st;
    int sockfd = -1;
    int cli_sockfd;
    struct sockaddr_in servAddr;
    struct sockaddr_storage client_addr;
    unsigned int client_addr_len = sizeof(client_addr);
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)arg;
    unsigned short port = prshdl->port;

    printf("port: %d\n", prshdl->port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("cannot open socket ");
        return NULL;
    }
 
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
     
    if(bind(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("cannot bind port ");
        return NULL;
    }

    st = listen(sockfd, MAX_QUEUE_SIZE);
    if (st == -1) {
        return NULL;
    }

    for (;;) {
        cli_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (cli_sockfd == -1) {
            return NULL;
        }

        st = rtsp_worker_create(prshdl, cli_sockfd, &client_addr);
        if(st < 0) {
            close(cli_sockfd);
        }
    }

    close(sockfd);
    sockfd = -1;

    return NULL;
}

int rtsp_server_start(void** pphdl, unsigned short port)
{
    rtsp_server_hdl_s* prshdl = NULL;

    prshdl = (rtsp_server_hdl_s*)malloc(sizeof(rtsp_server_hdl_s));
    if(prshdl == NULL) {
        return -1;
    }

    memset(prshdl, 0, sizeof(rtsp_server_hdl_s));

    prshdl->psess_hash = newhashtable(longhash, longequal, MAX_RTSP_WORKERS * 2, 1);
    if(prshdl->psess_hash == NULL) {
        free(prshdl);
        return -1;
    }

    prshdl->port = port;

    pthread_create(&prshdl->rstid, 0, rtsp_server_proc, prshdl);

    srand(time(0));

    *pphdl = prshdl;

    return 0;
}

int rtsp_server_stop(void** pphdl)
{
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)*pphdl;

    if(prshdl == NULL) {
        return -1;
    }

    free(prshdl);
    prshdl = NULL;

    return 0;
}

