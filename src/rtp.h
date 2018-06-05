#ifndef __RTP_H__
#define __RTP_H__

#include "rtsp_server.h"



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct rtp_param_ {
	int 					serport;
	int 					cliport;
	int 					ssrc;
	rtsp_stream_source_s* 	pstream_src;
} rtp_param_s;


int rtp_start(void **pphdl, rtp_param_s* pparam);


int rtp_stop(void **pphdl);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif