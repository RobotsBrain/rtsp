#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "utils.h"
#include "rtp_server.h"



/* SINGLE_NALU_DATA_MAX = RTP_SIZE_MAX - RTP_HEADER_SIZE*/
#define SINGLE_NALU_DATA_MAX  	1448
/* SLICE_NALU_DATA_MAX = RTP_SIZE_MAX - RTP_HEADER_SIZE - FU_A_INDI_SIZE - FU_A_HEAD_SIZE */
#define SLICE_NALU_DATA_MAX   	1446
#define NALU_INDIC_SIZE 		4
#define NALU_HEAD_SIZE  		1
#define FU_A_INDI_SIZE  		1
#define FU_A_HEAD_SIZE  		1
#define READ_LEN 				150000
#define SLICE_SIZE 				1448
#define SLICE_FSIZE 			1435
#define DE_TIME 				3600

#define MIN(a,b) (((a)<(b)) ? (a) : (b))


typedef struct rtp_stream_worker_ {
	char 			start;
	int 			sockfd;
	int 			server_port;
	int 			client_port;
	unsigned short 	seq;
	int 			ssrc;
	pthread_t 		tid;	
} rtp_stream_worker_s;


typedef struct rtp_handle_ {
	unsigned int 			ssrc;
	rtsp_stream_source_s	stream_src;
	rtp_stream_worker_s 	asworker;
	rtp_stream_worker_s		vsworker;
} rtp_server_hdl_s;


int rtp_send_av_data(int fd, const void *buf, size_t count)
{
	int ret = -1;
	size_t size = write(fd, buf, count);
	if(size == count) {
		ret = 0;
	}

	return ret;
}

/*******************************************************************************
 * RTP Packet:
 * 1. NALU length small than 1460-sizeof(RTP header):
 *    (RTP Header) + (NALU without Start Code)
 * 2. NALU length larger than MTU:
 *    (RTP Header) + (FU Indicator) + (FU Header) + (NALU Slice)
 *                 + (FU Indicator) + (FU Header) + (NALU Slice)
 *                 + ...
 *
 * inbuffer--NALU: 00 00 00 01      1 Byte     XX XX XX
 *                | Start Code| |NALU Header| |NALU Data|
 *
 * NALU Slice: Cut NALU Data into Slice.
 *
 * NALU Header: F|NRI|TYPE
 *              F: 1 bit,
 *              NRI: 2 bits,
 *              Type: 5 bits
 *
 * FU Indicator: Like NALU Header, Type is FU-A(28)
 * 
 * FU Header: S|E|R|Type
 *            S: 1 bit, Start, First Slice should set
 *            E: 1 bit, End, Last Slice should set
 *            R: 1 bit, Reserved
 *            Type: 5 bits, Same with NALU Header's Type.
 ******************************************************************************/
int rtp_build_nalu(rtp_stream_worker_s* pvswk, unsigned int ts, unsigned char *inbuffer, int frame_size)
{
	unsigned char nalu_header;
	unsigned char fu_indic;
	unsigned char fu_header;
	unsigned char *p_nalu_data = NULL;
	unsigned char buffer[1500] = {0};
	int data_left;
	int fu_start = 1;
	int fu_end   = 0;
	rtp_header_s rtp_header;

	rtp_build_header(&rtp_header, 96, pvswk->seq, ts, pvswk->ssrc);
	
	data_left   = frame_size - NALU_INDIC_SIZE;
	p_nalu_data = inbuffer + NALU_INDIC_SIZE;

	//Single RTP Packet.
    if(data_left <= SINGLE_NALU_DATA_MAX) {
	    rtp_header.seq_no = htons(pvswk->seq++);
	    rtp_header.marker = 1;    
		memcpy(buffer, &rtp_header, sizeof(rtp_header_s));
        memcpy(buffer + RTP_HEADER_SIZE, p_nalu_data, data_left);

        rtp_send_av_data(pvswk->sockfd, buffer, data_left + RTP_HEADER_SIZE);
        
		usleep(DE_TIME);
        return 0;
    }

	//FU-A RTP Packet.
	nalu_header = inbuffer[4];
	fu_indic    = (nalu_header&0xE0)|28;	
	data_left   -= NALU_HEAD_SIZE;
	p_nalu_data += NALU_HEAD_SIZE;

	while(data_left > 0) {
	    int proc_size = MIN(data_left, SLICE_NALU_DATA_MAX);
		int rtp_size = proc_size + RTP_HEADER_SIZE + FU_A_HEAD_SIZE + FU_A_INDI_SIZE;
		fu_end = (proc_size == data_left);

		fu_header = nalu_header & 0x1F;
		if(fu_start) {
			fu_header |= 0x80;
		} else if(fu_end) {
			fu_header |= 0x40;
		}

        rtp_header.seq_no = htons(pvswk->seq++);
		memcpy(buffer, &rtp_header, sizeof(rtp_header_s));
		memcpy(buffer + 14, p_nalu_data, proc_size);
		buffer[12] = fu_indic;
		buffer[13] = fu_header;

		rtp_send_av_data(pvswk->sockfd, buffer, rtp_size);

		if(fu_end) {
			usleep(36000);
		}
		
		data_left -= proc_size;	
		p_nalu_data += proc_size;
		fu_start = 0;
	}

	return 0;	
}

#define BUF_SIZE 200 * 1024

void* rtp_video_worker_proc(void* arg)
{
	int ret = -1;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)arg;
	rtp_stream_worker_s* pvswk = (rtp_stream_worker_s*)&prphdl->vsworker;
	unsigned char* buf = NULL;
	
	printf("[%s, %d] begin___, (%d, %d)\n",
			__FUNCTION__, __LINE__, pvswk->server_port, pvswk->client_port);

	buf = (unsigned char*)malloc(BUF_SIZE);
	if(buf == NULL) {
		printf("can not malloc memory!\n");
		return NULL;
	}

	pvswk->sockfd = create_udp_connect("127.0.0.1", pvswk->server_port, pvswk->client_port);
	if(pvswk->sockfd < 0) {
		return NULL;
	}

	pvswk->start = 1;

	rtsp_stream_identify_s identify;

	identify.type = RTSP_STREAM_TYPE_VIDEO;

	while(pvswk->start) {
		if(prphdl->stream_src.get_next_frame != NULL) {
			memset(buf, 0, BUF_SIZE);

			rtsp_stream_info_s vsinfo = {0};

			vsinfo.buf = buf;
			vsinfo.size = sizeof(buf);

			ret = prphdl->stream_src.get_next_frame(prphdl->stream_src.priv,
													&identify, &vsinfo);
			if(ret == 0 && vsinfo.size > 0) {
				ret = rtp_build_nalu(pvswk, vsinfo.ts, vsinfo.buf, vsinfo.size);
			}
			
			if(ret < 0) {
				// try to sleep some time
			}
		}
	}

	free(buf);
	buf = NULL;

	close(pvswk->sockfd);

	printf("[%s, %d] end___\n", __FUNCTION__, __LINE__);

	return NULL;
}

int rtp_build_audio_pack(rtp_stream_worker_s* paswk, unsigned int ts, unsigned char *buffer, int size)
{
	rtp_header_s rtp_header;
	char sedbuf[2048] = {0};

	rtp_build_header(&rtp_header, 97, paswk->seq, ts, paswk->ssrc);

	memcpy(sedbuf, &rtp_header, sizeof(rtp_header_s));
    memcpy(sedbuf + RTP_HEADER_SIZE, buffer, size);

    rtp_send_av_data(paswk->sockfd, sedbuf, size + RTP_HEADER_SIZE);

    paswk->seq++;

	return 0;
}

void* rtp_audio_worker_proc(void* arg)
{
	int ret = -1;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)arg;
	rtp_stream_worker_s* paswk = (rtp_stream_worker_s*)&prphdl->asworker;
	unsigned char buf[2048] = {0};
	
	printf("[%s, %d] begin___, (%d, %d)\n",
			__FUNCTION__, __LINE__, paswk->server_port, paswk->client_port);

	paswk->sockfd = create_udp_connect("127.0.0.1", paswk->server_port, paswk->client_port);
	if(paswk->sockfd < 0) {
		return NULL;
	}

	paswk->start = 1;

	rtsp_stream_identify_s identify;

	identify.type = RTSP_STREAM_TYPE_AUDIO;

	while(paswk->start) {
		if(prphdl->stream_src.get_next_frame != NULL) {
			memset(buf, 0, sizeof(buf));

			rtsp_stream_info_s vsinfo = {0};

			vsinfo.buf = buf;
			vsinfo.size = sizeof(buf);

			ret = prphdl->stream_src.get_next_frame(prphdl->stream_src.priv,
													&identify, &vsinfo);
			if(ret == 0 && vsinfo.size > 0) {
				ret = rtp_build_audio_pack(paswk, vsinfo.ts, vsinfo.buf, vsinfo.size);
			}
			
			if(ret < 0) {
				// try to sleep some time
			}
		}
	}

	close(paswk->sockfd);

	printf("[%s, %d] end___\n", __FUNCTION__, __LINE__);

	return NULL;
}

int rtp_server_start_streaming(void* phdl, rtp_server_stream_param_s* pparam)
{
	int ret = -1;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)phdl;

	if(prphdl == NULL) {
		return -1;
	}

	if(pparam->type == RTSP_STREAM_TYPE_VIDEO) {
		prphdl->vsworker.ssrc = random32(0);
		prphdl->vsworker.server_port = pparam->server_port;
		prphdl->vsworker.client_port = pparam->client_port;

		ret = pthread_create(&prphdl->vsworker.tid, NULL, rtp_video_worker_proc, prphdl);
	} else if (pparam->type == RTSP_STREAM_TYPE_AUDIO) {
		prphdl->asworker.ssrc = random32(0);
		prphdl->asworker.server_port = pparam->server_port;
		prphdl->asworker.client_port = pparam->client_port;

		ret = pthread_create(&prphdl->asworker.tid, NULL, rtp_audio_worker_proc, prphdl);
	}

	if(ret != 0) {
		printf("ret: %d, create thread fail!\n", ret);
	}

	return ret;
}

int rtp_server_stop_streaming(void* phdl)
{
	// int ret = -1;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)phdl;

	if(prphdl == NULL) {
		return -1;
	}

	return 0;
}

int rtp_server_init(void **pphdl,  rtp_server_param_s* pparam)
{
	rtp_server_hdl_s* prphdl = NULL;

	prphdl = (rtp_server_hdl_s*)malloc(sizeof(rtp_server_hdl_s));
	if(prphdl == NULL) {
		return -1;
	}

	prphdl->ssrc = random32(0);

	memcpy(&prphdl->stream_src, pparam->pstream_src, sizeof(rtsp_stream_source_s));

	*pphdl = prphdl;

	return 0;
}

int rtp_server_uninit(void **pphdl)
{
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)*pphdl;

	if(prphdl == NULL) {
		return -1;
	}

	free(prphdl);
	prphdl = NULL;

	return 0;
}


