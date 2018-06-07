/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum RTSP_STREAM_TYPE_ {
	RTSP_STREAM_TYPE_AUDIO,
	RTSP_STREAM_TYPE_VIDEO,
} RTSP_STREAM_TYPE_E;


typedef struct rtsp_stream_identify_ {
	RTSP_STREAM_TYPE_E 	type;
	unsigned int 		session_id;
} rtsp_stream_identify_s;


typedef struct rtsp_stream_info_ {
	unsigned int 	ts;
	char* 			buf;
	int 			size;
} rtsp_stream_info_s;


typedef struct rtsp_stream_source_ {
	void* priv;

	int max_frame;

	int (*start)(void* thiz, rtsp_stream_identify_s* pidentify);

	int (*stop)(void* thiz, rtsp_stream_identify_s* pidentify);

	int (*get_sdp)(void* thiz, rtsp_stream_identify_s* pidentify, char* buf, int* size);

	int (*get_next_frame)(void* thiz, rtsp_stream_identify_s* pidentify, rtsp_stream_info_s* psinfo);
} rtsp_stream_source_s;


typedef struct rtsp_server_param_ {
	unsigned short port;
	rtsp_stream_source_s stream_src;
} rtsp_server_param_s;


int rtsp_server_start(void** pphdl, rtsp_server_param_s* pparam);


int rtsp_server_stop(void** pphdl);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
