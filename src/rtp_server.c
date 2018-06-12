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
#define SLICE_SIZE 				1448
#define SLICE_FSIZE 			1435
#define DE_TIME 				3600

#define MIN(a,b) (((a)<(b)) ? (a) : (b))


typedef struct rtp_stream_worker_ {
	char 					start;
	int 					sockfd;
	int 					server_port;
	int 					client_port;
	unsigned short  		data_itl;
    unsigned short  		ctr_itl; 
	unsigned short 			seq;
	int 					ssrc;
	pthread_t 				tid;
	unsigned char* 			buf;
	int 					max_frame_size;
	RTSP_TRANSPORT_MODE_E   tmode;
} rtp_stream_worker_s;


typedef struct rtp_handle_ {
	char 					server_ip[32];
	rtsp_stream_source_s	stream_src;
	rtp_stream_worker_s 	asworker;
	rtp_stream_worker_s		vsworker;
} rtp_server_hdl_s;


int rtp_send_av_data(int fd, const void *buf, size_t count)
{
	int ret = -1;
	size_t size = 0;

	size = write(fd, buf, count);
	// size = send(fd, buf, count, 0);
	if(size == count) {
		// printf("sock(%d) send %d bytes data!\n", fd, size);
		ret = 0;
	}

	return ret;
}

int rtp_build_nalu(rtp_stream_worker_s* pvswk, unsigned int ts, unsigned char *inbuffer, int frame_size)
{
	unsigned char nalu_header;
	unsigned char fu_indic;
	unsigned char fu_header;
	unsigned char *p_nalu_data = inbuffer + NALU_INDIC_SIZE;
	unsigned char buffer[1500] = {0};
	int data_left = frame_size - NALU_INDIC_SIZE;
	int fu_start = 1;
	int fu_end   = 0;
	rtp_header_s rtp_header;

	rtp_build_header(&rtp_header, 96, pvswk->seq, ts, pvswk->ssrc);

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

static int rtp_worker_init(rtp_server_hdl_s* prphdl, rtp_stream_worker_s* pswk, rtsp_stream_identify_s* pidentify)
{
	if(pidentify->type == RTSP_STREAM_TYPE_VIDEO) {
		pswk->max_frame_size = 200 * 1024;
	} else if (pidentify->type == RTSP_STREAM_TYPE_AUDIO) {
		pswk->max_frame_size = 2048;
	}

	if(pswk->tmode == RTSP_TRANSPORT_MODE_UDP) {
		pswk->sockfd = create_udp_connect(prphdl->server_ip, pswk->server_port,
											pswk->client_port);
		if(pswk->sockfd < 0) {
			return -1;
		}
	}

	if(prphdl->stream_src.start != NULL) {
		prphdl->stream_src.start(prphdl->stream_src.priv, pidentify);
	}

	if(prphdl->stream_src.get_max_frame_size != NULL) {
		pswk->max_frame_size = prphdl->stream_src.get_max_frame_size(prphdl->stream_src.priv,
																		pidentify);
	}

	pswk->buf = (unsigned char*)malloc(pswk->max_frame_size);
	if(pswk->buf == NULL) {
		printf("can not malloc memory!\n");
		return -1;
	}

	pswk->start = 1;

	return 0;
}

static int rtp_worker_uninit(rtp_server_hdl_s* prphdl, rtp_stream_worker_s* pswk, rtsp_stream_identify_s* pidentify)
{
	if(prphdl->stream_src.stop != NULL) {
		prphdl->stream_src.stop(prphdl->stream_src.priv, pidentify);
	}

	if(pswk->buf != NULL) {
		free(pswk->buf);
		pswk->buf = NULL;
	}

	if(pswk->sockfd > 0) {
		close(pswk->sockfd);
		pswk->sockfd = -1;
	}

	memset(pswk, 0, sizeof(rtp_stream_worker_s));

	return 0;
}

void* rtp_video_worker_proc(void* arg)
{
	int ret = -1;
	rtsp_stream_identify_s identify;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)arg;
	rtp_stream_worker_s* pvswk = (rtp_stream_worker_s*)&prphdl->vsworker;
	
	printf("[%s, %d] begin___\n", __FUNCTION__, __LINE__);

	identify.type = RTSP_STREAM_TYPE_VIDEO;
	identify.session_id = pvswk->ssrc;

	if(rtp_worker_init(prphdl, pvswk, &identify) < 0) {
		printf("end___, init rtp worker error!\n");
		return NULL;
	}

	while(pvswk->start) {
		if(prphdl->stream_src.get_next_frame != NULL) {
			memset(pvswk->buf, 0, pvswk->max_frame_size);

			rtsp_stream_info_s vsinfo = {0};

			vsinfo.buf = pvswk->buf;
			vsinfo.size = pvswk->max_frame_size;

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

	rtp_worker_uninit(prphdl, pvswk, &identify);

	printf("[%s, %d] end___\n", __FUNCTION__, __LINE__);

	return NULL;
}

int rtp_build_audio_pack(rtp_stream_worker_s* paswk, unsigned int ts, unsigned char *buffer, int size)
{
	int offset = 0;
	char sedbuf[1600] = {0};
	rtp_header_s rtp_header;

	do {
		int snd_len = MIN(size, 1448);

		rtp_build_header(&rtp_header, 97, paswk->seq, ts, paswk->ssrc);

		memcpy(sedbuf, &rtp_header, sizeof(rtp_header_s));
	    memcpy(sedbuf + RTP_HEADER_SIZE, buffer + offset, snd_len);

		rtp_send_av_data(paswk->sockfd, sedbuf, snd_len + RTP_HEADER_SIZE);

		size -= snd_len;
		offset += snd_len;
		paswk->seq++;
	} while(size > 0);

	return 0;
}

void* rtp_audio_worker_proc(void* arg)
{
	int ret = -1;
	rtsp_stream_identify_s identify;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)arg;
	rtp_stream_worker_s* paswk = (rtp_stream_worker_s*)&prphdl->asworker;
	
	printf("[%s, %d] begin___\n", __FUNCTION__, __LINE__);

	identify.type = RTSP_STREAM_TYPE_AUDIO;
	identify.session_id = paswk->ssrc;

	if(rtp_worker_init(prphdl, paswk, &identify) < 0) {
		printf("end___, init rtp worker error!\n");
		return NULL;
	}

	while(paswk->start) {
		if(prphdl->stream_src.get_next_frame != NULL) {
			memset(paswk->buf, 0, paswk->max_frame_size);

			rtsp_stream_info_s vsinfo = {0};

			vsinfo.buf = paswk->buf;
			vsinfo.size = paswk->max_frame_size;

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

	rtp_worker_uninit(prphdl, paswk, &identify);

	printf("[%s, %d] end___\n", __FUNCTION__, __LINE__);

	return NULL;
}

int rtp_server_start_streaming(void* phdl, unsigned char* uri, rtp_server_stream_param_s* pparam)
{
	int ret = -1;
	rtp_stream_worker_s* pworker = NULL;
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)phdl;

	if(prphdl == NULL) {
		return -1;
	}

	if(strlen(prphdl->server_ip) <= 0) {
		char username[32];
		char password[32];
		char address[64];
		int portNum = 0;
		char path[128];

		parse_rtsp_url_info(uri, username, password, address, &portNum, path);
		parse_domain_ip_address(address, prphdl->server_ip);
	}

	if(pparam->type == RTSP_STREAM_TYPE_VIDEO) {
		pworker = &prphdl->vsworker;
	} else if (pparam->type == RTSP_STREAM_TYPE_AUDIO) {
		pworker = &prphdl->asworker;
	}

	if(pparam->tmode == RTSP_TRANSPORT_MODE_UDP) {
		typedef void *(*start_routine)(void *);
		start_routine thread_proc = NULL;
		pthread_t* ptid = NULL;

		if(pparam->type == RTSP_STREAM_TYPE_VIDEO) {
			ptid = &prphdl->vsworker.tid;
			thread_proc = rtp_video_worker_proc;
		} else if (pparam->type == RTSP_STREAM_TYPE_AUDIO) {
			ptid = &prphdl->asworker.tid;
			thread_proc = rtp_audio_worker_proc;
		}

		pworker->server_port = pparam->server_port;
		pworker->client_port = pparam->client_port;

		ret = pthread_create(ptid, NULL, thread_proc, prphdl);
		if(ret != 0) {
			printf("ret: %d, create thread fail!\n", ret);
		}
	} else if (pparam->tmode == RTSP_TRANSPORT_MODE_TCP) {
		pworker->sockfd = pparam->sockfd;
		pworker->data_itl = pparam->data_itl;
		pworker->ctr_itl = pparam->ctr_itl;

		printf("set stream sockfd(%d)\n", pparam->sockfd);

		ret = 0;
	}

	if(ret == 0) {
		pworker->ssrc = random32(0);
		pworker->tmode = pparam->tmode;
	}

	return ret;
}

int rtp_server_stop_streaming(void* phdl)
{
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)phdl;

	if(prphdl == NULL) {
		return -1;
	}

	printf("%s, %d  begin___\n", __FUNCTION__, __LINE__);

	prphdl->asworker.start = 0;
	pthread_join(prphdl->asworker.tid, NULL);

	prphdl->vsworker.start = 0;
	pthread_join(prphdl->vsworker.tid, NULL);

	printf("%s, %d  end___\n", __FUNCTION__, __LINE__);

	return 0;
}

int rtp_server_stream_data(void* phdl)
{
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)phdl;

	if(prphdl == NULL) {
		return -1;
	}

	// video
	{
		rtsp_stream_identify_s identify;
		rtp_stream_worker_s* pvswk = (rtp_stream_worker_s*)&prphdl->vsworker;

		identify.type = RTSP_STREAM_TYPE_VIDEO;
		identify.session_id = pvswk->ssrc;

		if(pvswk->start != 1 && rtp_worker_init(prphdl, pvswk, &identify) < 0) {
			printf("end___, init rtp worker error!\n");
			return -1;
		}

		memset(pvswk->buf, 0, pvswk->max_frame_size);

		rtsp_stream_info_s vsinfo = {0};

		vsinfo.buf = pvswk->buf;
		vsinfo.size = pvswk->max_frame_size;

		int ret = prphdl->stream_src.get_next_frame(prphdl->stream_src.priv,
												&identify, &vsinfo);
		if(ret == 0 && vsinfo.size > 0) {
			ret = rtp_build_nalu(pvswk, vsinfo.ts, vsinfo.buf, vsinfo.size);
		}
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

	printf("%s, %d  begin___\n", __FUNCTION__, __LINE__);

	memset(prphdl, 0, sizeof(rtp_server_hdl_s));

	memcpy(&prphdl->stream_src, pparam->pstream_src, sizeof(rtsp_stream_source_s));

	*pphdl = prphdl;

	printf("%s, %d  end___\n", __FUNCTION__, __LINE__);

	return 0;
}

int rtp_server_uninit(void **pphdl)
{
	rtp_server_hdl_s* prphdl = (rtp_server_hdl_s*)*pphdl;

	if(prphdl == NULL) {
		return -1;
	}

	rtp_server_stop_streaming(prphdl);

	free(prphdl);
	prphdl = NULL;

	return 0;
}


