/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _RTSP_H_
#define _RTSP_H_

#include "sdp.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    UNICAST = 0,
    MULTICAST
} TRANSPORT_CAST;

typedef enum {
    ACCEPT_STR = 0,
    CONTENT_TYPE_STR,
    CONTENT_LENGTH_STR,
    CSEQ_STR,
    SESSION_STR,
    TRANSPORT_STR
} ATTR;

typedef enum {
    DESCRIBE = 0,
    PLAY,
    PAUSE,
    SETUP,
    TEARDOWN,
    OPTIONS
} METHOD;

typedef struct {
    METHOD          method;
    char*           uri; /* Memory reserved in unpack_rtsp_req */
    int             CSeq;
    int             Session;
    TRANSPORT_CAST  cast;
    unsigned short  client_port;
} RTSP_REQUEST;

typedef struct {
    int             code;
    int             CSeq;
    int             Session;
    TRANSPORT_CAST  cast;
    unsigned short  client_port;
    unsigned short  server_port;
    int             Content_Length;
    char*           content;
    int             options;
} RTSP_RESPONSE;


int rtsp_unpack_request(RTSP_REQUEST *req, char *req_text, int text_size);


int pack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size);


int unpack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size);

int rtsp_pack_response(RTSP_RESPONSE *res, char *res_text, int text_size);

RTSP_REQUEST *rtsp_describe(const unsigned char *uri);

RTSP_REQUEST *rtsp_setup(const unsigned char *uri, int Session, TRANSPORT_CAST cast,
						unsigned short client_port);

RTSP_REQUEST *rtsp_play(const unsigned char *uri, int Session);

RTSP_REQUEST *rtsp_pause(const unsigned char *uri, int Session);

RTSP_REQUEST *rtsp_teardown(const unsigned char *uri, int Session);

RTSP_RESPONSE *rtsp_notfound(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_server_error(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_describe_res(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_setup_res(RTSP_REQUEST *req, unsigned short server_port,
								unsigned short client_port, TRANSPORT_CAST cast,
                                int Session);

RTSP_RESPONSE *rtsp_play_res(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_pause_res(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_teardown_res(RTSP_REQUEST *req);

RTSP_RESPONSE *rtsp_options_res(RTSP_REQUEST *req);

void rtsp_free_response(RTSP_RESPONSE **res);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
