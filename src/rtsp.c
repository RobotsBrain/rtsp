/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rtsp.h"
#include "utils.h"


#define RTSP_STR            "RTSP/1.0\0"
#define RTSP_URI            "rtsp://\0"
#define SDP_STR             "application/sdp\0"
#define RTP_UDP_STR         "RTP/AVP/UDP\0"
#define RTP_TCP_STR         "RTP/AVP/TCP\0"
#define INTERLEAVED_STR     "interleaved=\0"
#define CLIENT_PORT_STR     "client_port=\0"
#define SERVER_PORT_STR     "server_port=\0"
#define OPTIONS_STR         "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\0"
#define OK_STR              "OK\0"
#define SERVERERROR_STR     "Internal server error\0"
#define NOTFOUND_STR        "Not found\0"
#define NULL_STR            "\0"

const char *CAST_STR[] = {"unicast\0", "multicast\0"};
const char *ATTR_STR[] = {"Accept\0", "Content-Type\0", "Content-Length\0", "CSeq\0", "Session\0", "Transport\0"};
const char *METHOD_STR[] = {"DESCRIBE\0", "PLAY\0", "PAUSE\0", "SETUP\0", "TEARDOWN\0", "OPTIONS\0"};


static int check_uri(char *uri)
{
    if(strstr(uri, RTSP_URI) == uri) {
        return 1;
    }

    return 0;
}

static int detect_method(char *tok_start, int text_size)
{
    int i = 0;
    int method_len = 0;

    method_len = strcspn(tok_start, " ");
    if(method_len == text_size || method_len == 0) {
        return -1;
    }

    for(i = 0; i < sizeof(METHOD_STR)/sizeof(METHOD_STR[0]); ++i) {
        if(memcmp(METHOD_STR[i], tok_start, method_len) == 0) {
            return i;
        }
    }

    return -1;
}

static int detect_attr_req(RTSP_REQUEST *req, char *tok_start, int text_size)
{
    int i = 0;
    int attr = -1;
    int attr_len = 0;

    attr_len = strcspn(tok_start, ":");
    if(attr_len == text_size || attr_len == 0) {
        return 0;
    }

    for(i = 0; i < sizeof(ATTR_STR)/sizeof(ATTR_STR[0]); ++i) {
        if(memcmp(ATTR_STR[i], tok_start, attr_len) == 0) {
            attr = i;
            break;
        }
    }

    tok_start += attr_len;
    text_size -= attr_len - 1;

    /* Ignore spaces after ':' */
    while(*(++tok_start) == ' ') {
        --text_size;
    }

    attr_len = strcspn(tok_start, "\r\n");
    if (attr_len == text_size || attr_len == 0) {
        return 0;
    }

    switch (attr) {
    case ACCEPT_STR:
        /* Only can send SDP */
        if (!strnstr(tok_start, SDP_STR, attr_len))
            return 0;
        break;

    case CSEQ_STR:
        req->CSeq = atoi(tok_start);
        break;

    case SESSION_STR:
        req->Session = atoi(tok_start);
        break;

    case TRANSPORT_STR:
        /* The only acceptable transport is RTP */
        if(strnstr(tok_start, CLIENT_PORT_STR, attr_len) != 0) {
            req->tmode = RTSP_TRANSPORT_MODE_UDP;
        } else {
            if(strnstr(tok_start, INTERLEAVED_STR, attr_len) != 0) {
                req->tmode = RTSP_TRANSPORT_MODE_TCP;
            } else {
                return 0;
            }
        }

        if (strnstr(tok_start, CAST_STR[UNICAST], attr_len)) {
            req->cast = UNICAST;
        } else if (strnstr(tok_start, CAST_STR[MULTICAST], attr_len)) {
            req->cast = MULTICAST;
        } else {
            return 0;
        }

        if(req->tmode == RTSP_TRANSPORT_MODE_UDP) {
            if((tok_start = strnstr(tok_start, CLIENT_PORT_STR, attr_len))) {
                if (!tok_start) {
                    return 0;
                }

                tok_start += strlen(CLIENT_PORT_STR);
                req->client_port = (unsigned short)atoi(tok_start);
                if (req->client_port == 0) {
                    return 0;
                }
            }
        } else if (req->tmode == RTSP_TRANSPORT_MODE_TCP) {
            if((tok_start = strnstr(tok_start, INTERLEAVED_STR, attr_len))) {
                if (!tok_start) {
                    return 0;
                }

                tok_start += strlen(INTERLEAVED_STR);
                req->data_itl = (unsigned short)atoi(tok_start);
            }
        }
        break;

    default:
        return 1;
    }

    return 1;
}

int rtsp_unpack_request(RTSP_REQUEST *req, char *req_text, int text_size)
{
    int attr;
    int tok_len;
    char *tok_start = req_text;

    req->method = -1;
    req->uri = NULL;
    req->CSeq = -1;
    req->Session = -1;
    req->client_port = 0;

    tok_len = strcspn(tok_start, " ");
    if(tok_len == text_size) {
        return -1;
    }

    req->method = detect_method(tok_start, text_size);
    if (req->method == -1) {
        return -1;
    }

    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    tok_len = strcspn(tok_start, " ");
    if(tok_len == text_size) {
        return -1;
    }

    req->uri = malloc(tok_len + 1);
    if(req->uri == NULL) {
        return -1;
    }

    memcpy(req->uri, tok_start, tok_len);  /* Copy uri */
    req->uri[tok_len] = 0;
    if(check_uri(req->uri) == 0) {
        return -1;
    }

    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    /* Get rtsp token */
    tok_len = strcspn(tok_start, "\r\n");
    if (tok_len == text_size) {
        free(req->uri);
        req->uri = 0;
        return -1;
    }

    /* Check if the rtsp token is valid */
    if (strncmp(tok_start, RTSP_STR, 8)) {
        free(req->uri);
        req->uri = 0;
        return -1;
    }

    /* Ignore next \r or \n */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;
    if (*tok_start == '\r' || *tok_start == '\n') {
        ++tok_start;
        --text_size;
    }

    /* Get Attributes until we find an empty line */
    while ( (tok_len = strcspn(tok_start, "\r\n")) ) {
        if (tok_len == text_size) {
            free(req->uri);
            req->uri = 0;
            return -1;
        }

        /* Get all the attributes */
        attr = detect_attr_req(req, tok_start, text_size);
        if (!attr) {
            free(req->uri);
            req->uri = 0;
            return -1;
        }
        tok_start += tok_len + 1;
        text_size -= tok_len + 1;
        if (*tok_start == '\r' || *tok_start == '\n') {
            ++tok_start;
            --text_size;
        }
    }

    /* If text_size is 0, a last \r\n wasn't received */
    if (text_size == 0 || req->method == -1 || req->uri == NULL) {
        return -1;
    }

    /* Obligatory */
    if (req->CSeq == -1) {
        free(req->uri);
        req->uri = NULL;
        return -1;
    }

    /* Session must be present if the method is PLAY, PAUSE or TEARDOWN */
    if (req->Session == -1 && (req->method == PLAY || req->method == PAUSE || req->method == TEARDOWN)) {
        free(req->uri);
        req->uri = NULL;
        return -1;
    }

    if (req->tmode == RTSP_TRANSPORT_MODE_UDP && req->client_port == 0
        && req->method == SETUP) {
        free(req->uri);
        req->uri = NULL;
        return -1;
    }

    return 0;
}

int rtsp_pack_response(RTSP_RESPONSE *res, char *res_text, int text_size)
{
    int ret;
    int written;
    const char *status_str = NULL;

    /* Save space for the last \0 */
    --text_size;

    /* Write protocol and code*/
    if (res->code == -1) {
        return 0;
    }

    if (res->code == 200) {
        status_str = OK_STR;
    } else if (res->code == 500) {
        status_str = SERVERERROR_STR;
    } else if (res->code == 404) {
        status_str = NOTFOUND_STR;
    } else {
        status_str = NULL_STR;
    }

    ret = written = snprintf(res_text, text_size, "RTSP/1.0 %d %s\r\n", res->code, status_str);
    if (ret < 0 || ret >= text_size) {
        return 0;
    }

    /* CSeq must have a value always*/
    if (!res->CSeq) {
        return 0;
    }

    /* Print to res_text */
    ret = snprintf(res_text + written, text_size - written, "CSeq: %d\r\n", res->CSeq);
    /* Check if the printed text was larger than the space available in res_text */
    if (ret < 0 || ret >= text_size-written) {
        return 0;
    }

    /* Add the number of characters written to written */
    written += ret;

    /* Respond to options */
    if (res->options != 0) {
        ret = snprintf(res_text + written, text_size - written, "%s\r\n", OPTIONS_STR);
        if (ret < 0 || ret >= text_size - written) {
            return 0;
        }

        written += ret;
    }

    /* Write session number */
    if (res->Session != -1 && res->Session != 0) {
        ret = snprintf(res_text + written, text_size - written, "Session: %u\r\n", res->Session);
        if (ret < 0 || ret >= text_size - written) {
            return 0;
        }

        written += ret;
    }

    if(res->tmode == RTSP_TRANSPORT_MODE_UDP) {
        if (res->client_port && res->server_port) {
            ret = snprintf(res_text + written, text_size - written, \
                "Transport: RTP/AVP/UDP;%s;client_port=%d-%d;server_port=%d-%d\r\n", \
                CAST_STR[res->cast], res->client_port, res->client_port + 1, \
                res->server_port, res->server_port + 1);
            if (ret < 0 || ret >= text_size - written) {
                return 0;
            }

            written += ret;
        }
    } else if(res->tmode == RTSP_TRANSPORT_MODE_TCP) {
        ret = snprintf(res_text + written, text_size - written, \
            "Transport: RTP/AVP/TCP;%s;interleaved=%d-%d\r\n", \
            CAST_STR[res->cast], res->data_itl, res->data_itl + 1);
        if (ret < 0 || ret >= text_size - written) {
            return 0;
        }

        written += ret;
    }

    if (res->Content_Length > 0) {
        ret = snprintf(res_text + written, text_size - written, "Content-Length: %d\r\n", res->Content_Length);
        if (ret < 0 || ret >= text_size - written) {
            return 0;
        }

        written += ret;
    }
    /* Empty line */
    ret = snprintf(res_text + written, text_size - written, "\r\n");
    if (ret < 0 || ret >= text_size - written) {
        return 0;
    }

    written += ret;

    if (res->content) {
        ret = snprintf(res_text + written, text_size - written, "%s", res->content);
        if (ret < 0 || ret >= text_size - written) {
            return 0;
        }

        written += ret;
    }

    return written;
}

RTSP_RESPONSE *construct_rtsp_response(int code, int Session, TRANSPORT_CAST cast,
                                        unsigned short server_port, unsigned short client_port,
                                        int Content_Length, char *content,
                                        int options, const RTSP_REQUEST *req)
{
    RTSP_RESPONSE *res = NULL;

    res = (RTSP_RESPONSE *)malloc(sizeof(RTSP_RESPONSE));
    if (res == NULL) {
        return NULL;
    }

    res->code = code;
    res->CSeq = req->CSeq;
    res->Session = Session ? Session : req->Session;
    res->cast = cast ? cast : req->cast;
    res->tmode = req->tmode;

    if(req->tmode == RTSP_TRANSPORT_MODE_UDP) {
        res->client_port = client_port ? client_port : req->client_port;
        res->server_port = server_port;
    } else if(req->tmode == RTSP_TRANSPORT_MODE_TCP) {
        res->data_itl = req->data_itl;
        res->ctr_itl = req->data_itl + 1;
    }
    
    res->Content_Length = Content_Length;

    if(Content_Length > 0 && content != NULL) {
        res->content = malloc(Content_Length + 1);
        if (res->content == NULL) {
            return NULL;
        }

        memset(res->content, 0, Content_Length + 1);
        memcpy(res->content, content, Content_Length);
    } else {
        res->Content_Length = 0;
        res->content = NULL;
    }

    res->options = options;

    return res;
}

RTSP_RESPONSE *rtsp_notfound(RTSP_REQUEST *req)
{
    return construct_rtsp_response(404, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_server_error(RTSP_REQUEST *req)
{
    return construct_rtsp_response(501, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_describe_res(RTSP_REQUEST *req, rtsp_stream_source_s* pssrc)
{
    char sdp_str[2048] = {0};
    int sdp_len = 0;
    RTSP_RESPONSE *ret = NULL;

    if(pssrc->get_sdp == NULL) {
        return NULL;
    }

    rtsp_stream_identify_s identify;
    char buf[1024] = {0};
    int size = 1024;

    identify.type = RTSP_STREAM_TYPE_VIDEO;
    pssrc->get_sdp(pssrc->priv, &identify, buf, &size);
    strcpy(sdp_str, buf);

    memset(buf, 0, sizeof(buf));
    size = 1024;
    identify.type = RTSP_STREAM_TYPE_AUDIO;
    pssrc->get_sdp(pssrc->priv, &identify, buf, &size);
    strcat(sdp_str, buf);
    
    sdp_len = strlen(sdp_str);

    ret = construct_rtsp_response(200, -1, 0, 0, 0, sdp_len, sdp_str, 0, req);

    return ret;
}

/* server_port and server_port + 1 must be used for this uri/session */
RTSP_RESPONSE *rtsp_setup_res(RTSP_REQUEST *req, unsigned short server_port,
                                unsigned short client_port, TRANSPORT_CAST cast, int Session)
{
    if (cast == MULTICAST) {
        return construct_rtsp_response(200, Session, cast, server_port, client_port, 0, 0, 0, req);
    } else {
        return construct_rtsp_response(200, Session, cast, server_port, 0, 0, 0, 0, req);
    }
}

RTSP_RESPONSE *rtsp_play_res(RTSP_REQUEST *req)
{
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_pause_res(RTSP_REQUEST *req)
{
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_teardown_res(RTSP_REQUEST *req)
{
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 0, req);
}

RTSP_RESPONSE *rtsp_options_res(RTSP_REQUEST *req)
{
    return construct_rtsp_response(200, 0, 0, 0, 0, 0, 0, 1, req);
}

void rtsp_free_response(RTSP_RESPONSE **res)
{
    if((*res)->content) {
        free((*res)->content);
    }
    free(*res);
    *res = NULL;

    return;
}

