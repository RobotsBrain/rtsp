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
#include "rtp.h"
#include "rtsp_server.h"


#define REQ_BUFFER          4096
#define MAX_RTSP_WORKERS    20 /* Number of processes listening for rtsp connections */

typedef struct {
    unsigned char*  media_uri;  /* Uri for the media */
    unsigned int    ssrc;       /* Use the ssrc to locate the corresponding RTP session */
    unsigned short  client_port;
    unsigned short  server_port;
    void*           prtphdl;
} INTERNAL_MEDIA;

typedef struct {
    unsigned char*  global_uri;     /* Global control uri */
    INTERNAL_MEDIA  (*medias)[1];
    int             n_medias;
} INTERNAL_SOURCE;

typedef struct {
    int                     Session;
    int                     CSeq;
    int                     n_sources;
    INTERNAL_SOURCE         (*sources)[1];
    struct sockaddr_storage client_addr;
} INTERNAL_RTSP;

typedef struct rtsp_server_worker_ {
    int         used;
    int         sockfd;
    pthread_t   tid;
    struct sockaddr_storage client_addr;
    void*       pcontext;
} rtsp_server_worker_s;

typedef struct rtsp_server_hdl_ {
    unsigned short          port;
    pthread_t               rstid;
    hashtable*              psess_hash;
    rtsp_server_worker_s    workers[MAX_RTSP_WORKERS];
} rtsp_server_hdl_s;


RTSP_RESPONSE *rtsp_server_options(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    return rtsp_options_res(req);
}

RTSP_RESPONSE *rtsp_server_describe(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    if (1/* TODO: Check if file exists */) {
        return rtsp_describe_res(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_setup(rtsp_server_worker_s *self, RTSP_REQUEST *req,
                                INTERNAL_RTSP *rtsp_info)
{
    int i, j, st;
    int global_uri_len;
    char* end_global_uri = NULL;
    RTSP_RESPONSE *res;
    int *Session = NULL;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    end_global_uri = strstr(req->uri, "/audio");
    if(end_global_uri == NULL) {
        end_global_uri = strstr(req->uri, "/video");
    }

    if(end_global_uri == NULL) {
        return rtsp_notfound(req);
    }

    global_uri_len = end_global_uri - req->uri;

    if(req->cast == UNICAST) {
        if(1/* TODO: Check if file exists */) {
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
            if (!rtsp_info) { /* Check if the session has disappeared for some reason */
                fprintf(stderr, "caca3\n");
                return rtsp_server_error(req);
            }

            /* Check if the global uri already exists */
            for (i = 0; i < rtsp_info->n_sources; ++i) {
                if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len)) {
                    break;
                }
            }

            /* If it doesn't exist create it */
            if(i == rtsp_info->n_sources) {
                rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->n_sources));
                if (!rtsp_info->sources) {
                    fprintf(stderr, "caca4\n");
                    return rtsp_server_error(req);
                }
                /* Copy global uri */
                rtsp_info->sources[i]->global_uri = malloc (global_uri_len + 1);
                if (!rtsp_info->sources[i]->global_uri) {
                    fprintf(stderr, "caca5\n");
                    return rtsp_server_error(req);
                }
                memcpy(rtsp_info->sources[i]->global_uri, req->uri, global_uri_len);
                rtsp_info->sources[i]->global_uri[global_uri_len] = 0;
                rtsp_info->sources[i]->medias = NULL;
                rtsp_info->sources[i]->n_medias = 0;
            }

            /* Check if the media uri already exists */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri))) {
                    break;
                }
            }

            /* If it doesn't exist create it */
            if (j == rtsp_info->sources[i]->n_medias) {
                rtsp_info->sources[i]->medias = realloc(rtsp_info->sources[i]->medias, sizeof(INTERNAL_SOURCE) * ++(rtsp_info->sources[i]->n_medias));
                if (!rtsp_info->sources[i]->n_medias) {
                    fprintf(stderr, "caca6\n");
                    return rtsp_server_error(req);
                }
                /* Copy global uri */
                rtsp_info->sources[i]->medias[j]->media_uri = malloc (strlen(req->uri) + 1);
                if (!rtsp_info->sources[i]->medias[j]->media_uri) { 
                    fprintf(stderr, "caca7\n");
                    return rtsp_server_error(req);
                }
                memcpy(rtsp_info->sources[i]->medias[j]->media_uri, req->uri, strlen(req->uri));
                rtsp_info->sources[i]->medias[j]->media_uri[strlen(req->uri)] = 0;

                /* Put the client udp port in the structure */
                ((struct sockaddr_in*)&rtsp_info->client_addr)->sin_port = htons(req->client_port);

                rtsp_info = gethashtable(&prshdl->psess_hash, &(req->Session));
                if (!rtsp_info) { /* Check if the session has disappeared for some reason */
                    fprintf(stderr, "caca30\n");
                    return rtsp_server_error(req);
                }

                rtsp_info->sources[i]->medias[j]->ssrc = random32(0);

                printf("~~~~~~~~~~ ssrc: %ld\n", rtsp_info->sources[i]->medias[j]->ssrc);
                rtsp_info->sources[i]->medias[j]->client_port = req->client_port;
                rtsp_info->sources[i]->medias[j]->server_port = req->client_port + 1000;
            }

            res = rtsp_setup_res(req, rtsp_info->sources[i]->medias[j]->server_port, 0, UNICAST, 0);
            return res;
        } else {
            return rtsp_notfound(req);
        }
    } else {  /* TODO: Multicast */
        fprintf(stderr, "caca8\n");
    }

    return rtsp_server_error(req);
}

RTSP_RESPONSE *server_simple_command(rtsp_server_worker_s *self, RTSP_REQUEST *req,
                                    RTSP_RESPONSE *(*rtsp_command)(RTSP_REQUEST *))
{
    int i, j, st;
    int global_uri_len;
    int global_uri = 0;
    unsigned int ssrc;
    char *end_global_uri = NULL;
    INTERNAL_RTSP *rtsp_info = NULL;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    end_global_uri = strstr(req->uri, "/audio");
    if (end_global_uri == NULL) {
        end_global_uri = strstr(req->uri, "/video");
    }

    if (end_global_uri == NULL) {
        end_global_uri = req->uri + strlen(req->uri);
        global_uri = 1;
    }

    global_uri_len = end_global_uri - req->uri;

    if (1/* TODO: Check if file exists */) {
        rtsp_info = gethashtable(&prshdl->psess_hash, &req->Session);
        /* Check if the session has disappeared for some reason */
        if (!rtsp_info) {
            fprintf(stderr, "caca9\n");
            return rtsp_server_error(req);
        }

        /* Get global uri */
        for (i = 0; i < rtsp_info->n_sources; ++i) {
            if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len)) {
                break;
            }
        }

        /* If it doesn't exist return error*/
        if (i == rtsp_info->n_sources) {
            fprintf(stderr, "caca10\n");
            return rtsp_server_error(req);
        }

        if (!global_uri) {  /* If the uri isn't global */
            /* Get the media uri */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri))) {
                    break;
                }
            }
            /* If it doesn't exist return error */
            if (j == rtsp_info->sources[i]->n_medias) {
                fprintf(stderr, "caca11\n");
                return rtsp_server_error(req);
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
                return rtsp_server_error(req);
            }
        }

        return rtsp_command(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_play(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    int i, j, st;
    int global_uri_len;
    int global_uri = 0;
    unsigned int ssrc;
    char *end_global_uri = NULL;
    INTERNAL_RTSP *rtsp_info = NULL;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    end_global_uri = strstr(req->uri, "/audio");
    if (end_global_uri == NULL) {
        end_global_uri = strstr(req->uri, "/video");
    }

    if (end_global_uri == NULL) {
        end_global_uri = req->uri + strlen(req->uri);
        global_uri = 1;
    }

    global_uri_len = end_global_uri - req->uri;

    if (1/* TODO: Check if file exists */) {
        rtsp_info = gethashtable(&prshdl->psess_hash, &req->Session);
        /* Check if the session has disappeared for some reason */
        if (!rtsp_info) {
            fprintf(stderr, "caca9\n");
            return rtsp_server_error(req);
        }

        /* Get global uri */
        for (i = 0; i < rtsp_info->n_sources; ++i) {
            if (!memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len)) {
                break;
            }
        }

        /* If it doesn't exist return error*/
        if (i == rtsp_info->n_sources) {
            fprintf(stderr, "caca10\n");
            return rtsp_server_error(req);
        }

        if (!global_uri) {  /* If the uri isn't global */
            /* Get the media uri */
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri))) {
                    break;
                }
            }
            /* If it doesn't exist return error */
            if (j == rtsp_info->sources[i]->n_medias) {
                fprintf(stderr, "caca11\n");
                return rtsp_server_error(req);
            }
        } else {
            /* Apply to all the medias in the global uri */
            fprintf(stderr, "medias: %d\n", rtsp_info->sources[i]->n_medias);
            st = 1;
            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~ %s\n", req->uri);
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {

                if(strstr(rtsp_info->sources[i]->medias[j]->media_uri, "video"))
                rtp_start(&rtsp_info->sources[i]->medias[j]->prtphdl, rtsp_info->sources[i]->medias[j]->server_port,
                      rtsp_info->sources[i]->medias[j]->client_port, rtsp_info->sources[i]->medias[j]->ssrc);
            }

            if (!st) {
                fprintf(stderr, "caca13\n");
                return rtsp_server_error(req);
            }
        }

        return rtsp_play_res(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_pause(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    return server_simple_command(self, req, rtsp_pause_res);
}

RTSP_RESPONSE *rtsp_server_teardown(rtsp_server_worker_s* self, RTSP_REQUEST *req)
{
    int i, j;
    int global_uri = 0;
    int global_uri_len = 0;
    char *end_global_uri = NULL;
    INTERNAL_RTSP *rtsp_info = NULL;
    RTSP_RESPONSE *res = NULL;
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
        return rtsp_server_error(req);
    }
    /* Check if the global uri exists */
    for (i = 0; i < rtsp_info->n_sources; ++i) {
        if (strlen(rtsp_info->sources[i]->global_uri) == global_uri_len &&
                !memcmp(req->uri, rtsp_info->sources[i]->global_uri, global_uri_len)) {
            break;
        }      
    }

    if (i == rtsp_info->n_sources) {
        fprintf(stderr, "caca21\n");
        free(res);
        return rtsp_server_error(req);
    }

    if (global_uri) {
        /* Delete all medias with this uri */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
            free(rtsp_info->sources[i]->medias[j]->media_uri);
        }

        /* Free medias array */
        free(rtsp_info->sources[i]->medias);

        /* Move the other sources */
        memmove(&(rtsp_info->sources[i]), &(rtsp_info->sources[i+1]), rtsp_info->n_sources - i - 1);
        (--rtsp_info->n_sources);
        /* Change size of sources array */
        rtsp_info->sources = realloc(rtsp_info->sources, sizeof(INTERNAL_SOURCE) * rtsp_info->n_sources);
    } else {
        /* Check if the media uri exists */
        for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
            if (!strncmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri))) {
                break;
            }
        }

        if (j == rtsp_info->sources[i]->n_medias) {
            fprintf(stderr, "caca22\n");
            free(res);
            return rtsp_server_error(req);
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

static void get_session(rtsp_server_worker_s *self, int *ext_session, INTERNAL_RTSP **rtsp_info)
{
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    if(*ext_session > 0) {
        *rtsp_info = gethashtable(&prshdl->psess_hash, ext_session);
        if (*rtsp_info) {
            return;
        }
    }

    while(*ext_session < 1) {
        srand(time(0));
        *ext_session = rand();
        *rtsp_info = gethashtable(&prshdl->psess_hash, ext_session);
        if (*rtsp_info) {
            *ext_session = -1;
        }
    }

    return;
}

void *rtsp_server_worker_proc(void *arg)
{
    rtsp_server_worker_s* self = (rtsp_server_worker_s*)arg;
    int sockfd = self->sockfd;
    int ret, st;
    int CSeq = 0;
    int server_error;
    int Session;
    char buf[REQ_BUFFER] = {0};
    RTSP_REQUEST req[1];
    RTSP_RESPONSE *res = NULL;
    INTERNAL_RTSP *rtsp_info = NULL;

    for(;;) {
        rtsp_info = NULL;
        Session = -1;

        do {
            server_error = 0;

            memset(buf, 0, REQ_BUFFER);

            st = read(sockfd, buf, REQ_BUFFER);
            if (st == -1) {
                return 0;
            } else if (!st) {
                server_error = 1;
            }
fprintf(stderr, "\n########################## RECEIVED ##########################\n%s", buf);
            st = unpack_rtsp_req(req, buf, st);
            if (server_error && st) {
                fprintf(stderr, "caca1\n");
                res = rtsp_server_error(req);
                if (res) {
                    st = rtsp_pack_response(res, buf, REQ_BUFFER);
                    if (st) {
                        buf[st] = 0;
                        send(sockfd, buf, st, 0);
                    }
                    rtsp_free_response(&res);
                }
            } else if (server_error || !st) {
                memcpy(buf, "RTSP/1.0 500 Internal server error\r\n\r\n", 38);
                buf[38] = 0;
                send(sockfd, buf, strlen(buf), 0);
            }
        } while(server_error || !st);
        
        get_session(self, &(req->Session), &rtsp_info);

        if (req->CSeq <= CSeq || 
             ((req->method == PLAY || req->method == PAUSE || req->method == TEARDOWN) && rtsp_info == NULL)) {
            res = rtsp_server_error(req);
        } else {
            CSeq = req->CSeq;
            switch(req->method) {
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
                res = rtsp_server_play(self, req);
                break;

            case PAUSE:
                res = rtsp_server_pause(self, req);
                break;

            case TEARDOWN:
                res = rtsp_server_teardown(self, req);
                break;

            default:
                res = rtsp_server_error(req);
                break;
            }  
        }

        if(res) {
            st = rtsp_pack_response(res, buf, REQ_BUFFER);
            if (st) {
                fprintf(stderr, "\n########################## RESPONSE ##########################\n%s\n", buf);
                send(sockfd, buf, st, 0);
            }
            rtsp_free_response(&res);
        }

        if(req->uri) {
            free(req->uri);
        }
    }

    return NULL;
}

int rtsp_worker_create(rtsp_server_hdl_s* prshdl, int sockfd,
                        struct sockaddr_storage *client_addr)
{
    int ret = -1;
    int i = 0;

    do {
        for(i = 0; i < MAX_RTSP_WORKERS; ++i) {
            if (prshdl->workers[i].used == 0) {
                printf("find a free worker(%d) to do...\n", i);
                break;
            }       
        }

        if(i >= MAX_RTSP_WORKERS) {
            printf("can not find!\n");
            break;
        }

        memset(&prshdl->workers[i], 0, sizeof(rtsp_server_worker_s));

        prshdl->workers[i].sockfd = sockfd;
        prshdl->workers[i].pcontext = prshdl;

        memcpy(&(prshdl->workers[i].client_addr), client_addr, sizeof(struct sockaddr_storage));

        ret = pthread_create(&prshdl->workers[i].tid, 0, rtsp_server_worker_proc, &prshdl->workers[i]);
        if(ret != 0) {
            printf("create thread fail!\n");
            break;
        }

        prshdl->workers[i].used = 1;
        ret = 0;
    } while(0);

    return ret;
}

void *rtsp_server_proc(void *arg)
{
    int ret = -1;
    int sockfd = -1, cli_sockfd = -1;
    struct sockaddr_storage client_addr;
    unsigned int client_addr_len = sizeof(client_addr);
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)arg;

    sockfd = create_tcp_server(NULL, prshdl->port);
    if(sockfd < 0) {
        return NULL;
    }

    for (;;) {
        memset(&client_addr, 0, sizeof(struct sockaddr_storage));

        cli_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (cli_sockfd == -1) {
            break;
        }

        ret = rtsp_worker_create(prshdl, cli_sockfd, &client_addr);
        if(ret < 0) {
            close(cli_sockfd);
            cli_sockfd = -1;
        }
    }

    close(sockfd);
    sockfd = -1;

    return NULL;
}

int rtsp_server_start(void** pphdl, rtsp_server_param_s* pparam)
{
    int ret = -1;
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

    prshdl->port = pparam->port;

    ret = pthread_create(&prshdl->rstid, 0, rtsp_server_proc, prshdl);
    if(ret != 0) {
        printf("create rtsp server thread fail!\n");
        free(prshdl);
        return -1;
    }

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

