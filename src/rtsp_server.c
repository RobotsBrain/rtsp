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

#include "network.h"
#include "hashtable.h"
#include "rtsp.h"
#include "rtsp_server.h"


#define REQ_BUFFER          4096
#define MAX_RTSP_WORKERS    20 /* Number of processes listening for rtsp connections */

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
    time_t      time_contacted;
    pthread_t   thread_id;
    struct sockaddr_storage client_addr;
} WORKER;


WORKER workers[MAX_RTSP_WORKERS][1];
hashtable *session_hash;


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
                st = puthashtable(&session_hash, Session, rtsp_info);
            }

            fprintf(stderr, "Getting session: %d\n", req->Session);
            rtsp_info = gethashtable(&session_hash, &(req->Session));
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

                rtsp_info = gethashtable(&session_hash, &(req->Session));
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
        rtsp_info = gethashtable(&session_hash, &req->Session);
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
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j)
                if (!memcmp(req->uri, rtsp_info->sources[i]->medias[j]->media_uri, strlen(req->uri)))
                    break;
            /* If it doesn't exist return error */
            if (j == rtsp_info->sources[i]->n_medias) {
                fprintf(stderr, "caca11\n");
                return rtsp_servererror(req);
            }

            /* Apply to only one media */
            // ssrc = rtsp_info->sources[i]->medias[j]->ssrc;
            // st = rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
            // if (!st) {
            //     pthread_mutex_unlock(&hash_mutex);
            //     fprintf(stderr, "caca12\n");
            //     return(rtsp_servererror(req));
            // }
        } else {
            /* Apply to all the medias in the global uri */
            fprintf(stderr, "medias: %d\n", rtsp_info->sources[i]->n_medias);
            st = 1;
            for (j = 0; j < rtsp_info->sources[i]->n_medias; ++j) {
                // ssrc = rtsp_info->sources[i]->medias[j]->ssrc;
                // fprintf(stderr, "El ssrc del media %d es %d\n", j, ssrc);
                // /* Try to free all medias */
                // if (st)
                //     st = rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
                // else
                //     rtp_command(rtsp_info->sources[i]->medias[j]->media_uri, ssrc);
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

    res = server_simple_command(self, req, rtsp_teardown_res);

    fprintf(stderr, "Borrando uri: %s\n", req->uri);
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

    /* TODO: Borrar media */
    /* Get the session */
    rtsp_info = gethashtable(&session_hash, &(req->Session));
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

/* Checks if a session already exists. If it doesn't, create one */
int get_session(int *ext_session, INTERNAL_RTSP **rtsp_info)
{
    if (*ext_session > 0) {
        /* Check that the session truly exists */
        *rtsp_info = gethashtable(&session_hash, ext_session);
        if (*rtsp_info) {
            return 1;
        }
    }

    while (*ext_session < 1) {
        *ext_session = rand();
        *rtsp_info = gethashtable(&session_hash, ext_session);
        if (*rtsp_info) {
            *ext_session = -1;
        }
    }

    return 1;
}

void *rtsp_worker_fun(void *arg)
{
    WORKER *self = arg;
    int sockfd;
    char buf[REQ_BUFFER];
    int ret;
    int st;
    RTSP_REQUEST req[1];
    RTSP_RESPONSE *res;
    int server_error;
    int Session;
    INTERNAL_RTSP *rtsp_info = 0;
    int CSeq = 0;

    sockfd = self->sockfd;

    for(;;) {
        rtsp_info = 0;
        Session = -1;
        /* Wait to get a correct request */
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
        
        st = get_session(&(req->Session), &rtsp_info);  /* Get or create session */
        /* Check that CSeq is incrementing */
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

            self->time_contacted = time(0);
        }

        if (res) {
            st = pack_rtsp_res(res, buf, REQ_BUFFER);
            if (st) {
                fprintf(stderr, "\n########################## RESPONSE ##########################\n");
                buf[st] = 0;
                write(2, buf, st);
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

int rtsp_worker_create(int tmp_sockfd, struct sockaddr_storage *client_addr)
{
    int ret = -1;
    int i = 0;
    int st = -1;

    do {
        for(i = 0; i < MAX_RTSP_WORKERS; ++i) {
            if (!workers[i]->used) {
                break;
            }       
        }

        if(i >= MAX_RTSP_WORKERS) {
            break;
        }

        st = pthread_create(&workers[i]->thread_id, 0, rtsp_worker_fun, &workers[i]);
        if (st) {
            break;
        }

        workers[i]->used = 1;
        workers[i]->sockfd = tmp_sockfd;  /* Save socket info */
        workers[i]->time_contacted = time(0);  /* Set as accesed now */

        memcpy(&(workers[i]->client_addr), client_addr, sizeof(struct sockaddr_storage));
        ret = 0;
    } while(0);

    return ret;
}

int rtsp_server(unsigned short port)
{
    int i;
    int st;

    srand(time(0));

    session_hash = 0;
    int sockfd = -1;

    for (i = 0; i < MAX_RTSP_WORKERS; ++i) {
        workers[i]->used = 0;
    }

    session_hash = newhashtable(longhash, longequal, MAX_RTSP_WORKERS * 2, 1);
    if (!session_hash) {
        return 0;
    }

    accept_tcp_requests(port, &sockfd, NULL, rtsp_worker_create);

    return 0;
}


