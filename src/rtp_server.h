#ifndef __RTP_SERVER_H__
#define __RTP_SERVER_H__

#include "rtsp_server.h"
#include "rtp.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct rtp_param_ {
	int 					serport;
	int 					cliport;
	int 					ssrc;
	rtsp_stream_source_s* 	pstream_src;
} rtp_server_param_s;


int rtp_server_start(void **pphdl, rtp_server_param_s* pparam);


int rtp_server_stop(void **pphdl);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif