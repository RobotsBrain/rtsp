#ifndef __RTSP_SERVER_INTERNAL_H__
#define __RTSP_SERVER_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define REQ_BUFFER          4096
#define MAX_RTSP_WORKERS    20

typedef struct rtsp_media_ {
    unsigned char   uri[128];
    unsigned short  client_port;
    unsigned short  server_port;
    void*           prtphdl;
} rtsp_media_s;


typedef struct rtsp_session_ {
    int             session;
    unsigned int    ssrc;
    int             src_num;
    rtsp_media_s    medias[2];
} rtsp_session_s;


typedef struct rtsp_server_worker_ {
    char                    start;
    int                     used;
    int                     sockfd;
    pthread_t               tid;
    rtsp_session_s          mssion;
    struct sockaddr_storage client_addr;
    void*                   pcontext;
} rtsp_server_worker_s;


typedef struct rtsp_server_hdl_ {
    char                    start;
    unsigned short          port;
    pthread_t               rstid;
    rtsp_stream_source_s    stream_src;
    rtsp_server_worker_s    workers[MAX_RTSP_WORKERS];
} rtsp_server_hdl_s;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif