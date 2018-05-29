/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _RTSP_H_
#define _RTSP_H_

#include "sdp.h"

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
    METHOD method;
    char *uri; /* Memory reserved in unpack_rtsp_req */
    int CSeq;
    int Session;
    TRANSPORT_CAST cast;
    unsigned short client_port;
} RTSP_REQUEST;

typedef struct {
    int code;
    int CSeq;
    int Session;
    TRANSPORT_CAST cast;
    unsigned short client_port;
    unsigned short server_port;
    int Content_Length;
    char *content;
    int options;
} RTSP_RESPONSE;

int unpack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size);

int detect_method(char *tok_start, int text_size);

int detect_attr_req(RTSP_REQUEST *req, char *tok_start, int text_size);

/*
 * Pack the structure RTSP_REQUEST in a string
 * req: request structure
 * req_text: String where the request will be packed
 * text_size: Length of req_text
 * return: number of characters written. 0 is error
 */
int pack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size);

int unpack_rtsp_res2(RTSP_RESPONSE **res, char *res_text, int text_size);

int unpack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size);

int pack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size);

int detect_attr_res(RTSP_RESPONSE *res, char *tok_start, int text_size);

int check_uri(char *uri);

/* Generate request to get description of a media in sdp format
 * uri: Uri of the media
 */
RTSP_REQUEST *rtsp_describe(const unsigned char *uri);

/* Generate request to setup a media.
 * uri: Uri of the media
 * Session: -1 if there is not session already, the session number otherwise
 * cast: UNICAST (TODO: or MULTICAST)
 * client_port: the port where RTP is listening in the client
 */
RTSP_REQUEST *rtsp_setup(const unsigned char *uri, int Session, TRANSPORT_CAST cast,
						unsigned short client_port);

/* Generate request to play a media. Setup must have been called before for this uri
 * uri: Uri of the media
 * Session: Session number. Obligatory
 */
RTSP_REQUEST *rtsp_play(const unsigned char *uri, int Session);

/* Generate request to pause a media. Setup must have been called before for this uri
 * uri: Uri of the media
 * Session: Session number. Obligatory
 */
RTSP_REQUEST *rtsp_pause(const unsigned char *uri, int Session);

/* Generate request to teardown a media. Setup must have been called before for this uri
 * uri: Uri of the media
 * Session: Session number. Obligatory
 */
RTSP_REQUEST *rtsp_teardown(const unsigned char *uri, int Session);

/* Generate not found error
 * req: Request
 */
RTSP_RESPONSE *rtsp_notfound(RTSP_REQUEST *req);

/* Generate server error
 * req: Request
 */
RTSP_RESPONSE *rtsp_servererror(RTSP_REQUEST *req);

/* Generate describe response for the request
 * req: Request
 */
RTSP_RESPONSE *rtsp_describe_res(RTSP_REQUEST *req);

/* Generate setup response for req
 * req: Request
 * server_port: Port in the server sending RTP media
 * client_port: Port in the client receiving RTP media
 * cast: UNICAST (TODO: or MULTICAST)
 * Session: Session id
 */
RTSP_RESPONSE *rtsp_setup_res(RTSP_REQUEST *req, unsigned short server_port,
								unsigned short client_port, TRANSPORT_CAST cast, int Session);

/* Generate play response for the request
 * req: Request
 */
RTSP_RESPONSE *rtsp_play_res(RTSP_REQUEST *req);

/* Generate pause response for the request
 * req: Request
 */
RTSP_RESPONSE *rtsp_pause_res(RTSP_REQUEST *req);

/* Generate teardown response for the request
 * req: Request
 */
RTSP_RESPONSE *rtsp_teardown_res(RTSP_REQUEST *req);

/* Generate options response for the request
 * req: Request
 */
RTSP_RESPONSE *rtsp_options_res(RTSP_REQUEST *req);

/* Free all the memory associated with the RTSP_REQUEST structure
 * req: Request
 */
void free_rtsp_req(RTSP_REQUEST **req);

/* Free all the memory associated with the RTSP_RESPONSE structure
 * res: Response
 */
void free_rtsp_res(RTSP_RESPONSE **res);

#endif
