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

#include "utils.h"
#include "rtsp.h"
#include "rtp_server.h"
#include "rtsp_server.h"
#include "rtsp_server_internal.h"



RTSP_RESPONSE *rtsp_server_options(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    return rtsp_options_res(req);
}

RTSP_RESPONSE *rtsp_server_describe(rtsp_server_worker_s *self, RTSP_REQUEST *req, rtsp_stream_source_s* pssrc)
{
    if (1/* TODO: Check if file exists */) {
        return rtsp_describe_res(req, pssrc);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_setup(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    RTSP_RESPONSE *res = NULL;
    // rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    if(self->mssion.src_num > 2) {
        return NULL;
    } else if(self->mssion.src_num > 0) {
        for(int i = 0; i < self->mssion.src_num; ++i) {
            if(memcmp(self->mssion.medias[i].uri, req->uri, strlen(req->uri)) == 0) {
                return NULL;
            }
        }
    }

    if(req->cast == UNICAST) {
        if(1/* TODO: Check if file exists */) {
            if(self->mssion.session <= 0) {
                self->mssion.session = random32(0);
            }

            req->Session = self->mssion.session;

            int index = self->mssion.src_num;

            memcpy(self->mssion.medias[index].uri, req->uri, strlen(req->uri));
            self->mssion.medias[index].client_port = req->client_port;
            self->mssion.medias[index].server_port = req->client_port + 1000;

            self->mssion.src_num++;

            res = rtsp_setup_res(req, self->mssion.medias[index].server_port, 0, UNICAST, 0);
        
            return res;
        }
    }

    return NULL;
}

RTSP_RESPONSE *rtsp_server_play(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    if (1/* TODO: Check if file exists */) {
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~ %s %d\n", req->uri, self->mssion.src_num);
        rtp_server_param_s param;

        memset(&param, 0, sizeof(rtp_server_param_s));

        param.pstream_src = &(prshdl->stream_src);
        rtp_server_init(&self->prtphdl, &param);

        for (int i = 0; i < self->mssion.src_num; ++i) {
            rtp_server_stream_param_s sparam;

            memset(&sparam, 0, sizeof(rtp_server_stream_param_s));

            if(strstr((const char *)self->mssion.medias[i].uri, "video") != NULL) {
                sparam.type = RTSP_STREAM_TYPE_VIDEO;
            } else if(strstr((const char *)self->mssion.medias[i].uri, "audio") != NULL){
                sparam.type = RTSP_STREAM_TYPE_AUDIO;
            }

            sparam.server_port = self->mssion.medias[i].server_port;
            sparam.client_port = self->mssion.medias[i].client_port;

            rtp_server_start_streaming(self->prtphdl, self->mssion.medias[i].uri, &sparam);
        }

        return rtsp_play_res(req);
    } else {
        return rtsp_notfound(req);
    }
}

RTSP_RESPONSE *rtsp_server_pause(rtsp_server_worker_s *self, RTSP_REQUEST *req)
{
    return NULL;
}

RTSP_RESPONSE *rtsp_server_teardown(rtsp_server_worker_s* self, RTSP_REQUEST *req)
{
    return NULL;
}

void *rtsp_server_worker_proc(void *arg)
{
    rtsp_server_worker_s* self = (rtsp_server_worker_s*)arg;
    int sockfd = self->sockfd;
    int st;
    int CSeq = 0;
    char buf[REQ_BUFFER] = {0};
    RTSP_REQUEST req[1];
    RTSP_RESPONSE *res = NULL;
    rtsp_server_hdl_s* prshdl = (rtsp_server_hdl_s*)(self->pcontext);

    self->start = 1;

    while(self->start) {
        memset(buf, 0, REQ_BUFFER);
        memset(req, 0, sizeof(RTSP_REQUEST));

        st = read(sockfd, buf, REQ_BUFFER);
        if (st == -1) {
            return NULL;
        } else if(st == 0) {
            break;
        }

        fprintf(stderr, "\n########################## RECEIVED\n%s", buf);
        
        st = rtsp_unpack_request(req, buf, st);
        if(req->CSeq <= CSeq) {
            res = rtsp_server_error(req);
        } else {
            CSeq = req->CSeq;

            switch(req->method) {
            case OPTIONS:
                res = rtsp_server_options(self, req);
                break;

            case DESCRIBE:
                res = rtsp_server_describe(self, req, &prshdl->stream_src);
                break;

            case SETUP:
                res = rtsp_server_setup(self, req);
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
                fprintf(stderr, "\n########################## RESPONSE\n%s\n", buf);
                send(sockfd, buf, st, 0);
            }
            rtsp_free_response(&res);
        }

        if(req->uri != NULL) {
            free(req->uri);
            req->uri = NULL;
        }
    }

    if(self->prtphdl != NULL) {
        rtp_server_stop_streaming(self->prtphdl);
        rtp_server_uninit(&self->prtphdl);
    }

    close(self->sockfd);
    memset(self, 0, sizeof(rtsp_server_worker_s));

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

    sockfd = create_tcp_server(prshdl->ipaddr, prshdl->port);
    if(sockfd < 0) {
        return NULL;
    }

    prshdl->start = 1;

    while(prshdl->start) {
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

    prshdl->port = pparam->port;
    strcpy(prshdl->ipaddr, pparam->ipaddr);
    memcpy(&prshdl->stream_src, &pparam->stream_src, sizeof(rtsp_stream_source_s));

    ret = pthread_create(&prshdl->rstid, NULL, rtsp_server_proc, prshdl);
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

    prshdl->start = 0;

    pthread_join(prshdl->rstid, NULL);

    for(int i = 0; i < MAX_RTSP_WORKERS; ++i) {
        if (prshdl->workers[i].used == 1) {
            prshdl->workers[i].start = 0;
            pthread_join(prshdl->workers[i].tid, NULL);
        }       
    }

    free(prshdl);
    prshdl = NULL;

    return 0;
}

