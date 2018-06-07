#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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


typedef struct rtp_handle_ {
	char 					start;
	int 					sockfd;
	int 					serport;
	int 					cliport;
	int 					ssrc;
	unsigned long 			seq;
	unsigned int 			timestamp;
	pthread_t 				tid;
	rtsp_stream_source_s	stream_src;
} rtp_handle_s;


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
int rtp_build_nalu(rtp_handle_s* prphdl, unsigned char *inbuffer, int frame_size)
{
	unsigned char nalu_header;
	unsigned char fu_indic;
	unsigned char fu_header;
	unsigned char *p_nalu_data = NULL;
	unsigned char buffer[1500] = {0};
	int time_delay;
	int data_left;
	int fu_start = 1;
	int fu_end   = 0;
	rtp_header_s rtp_header;

	int frame_rate_step = 3600;
	prphdl->timestamp += frame_rate_step;

	rtp_build_header(&rtp_header, prphdl->seq, prphdl->timestamp, prphdl->ssrc);
	
	data_left   = frame_size - NALU_INDIC_SIZE;
	p_nalu_data = inbuffer + NALU_INDIC_SIZE;

	//Single RTP Packet.
    if(data_left <= SINGLE_NALU_DATA_MAX) {
	    rtp_header.seq_no = htons(prphdl->seq++);
	    rtp_header.marker = 1;    
		memcpy(buffer, &rtp_header, sizeof(rtp_header_s));
        memcpy(buffer + RTP_HEADER_SIZE, p_nalu_data, data_left);
        write(prphdl->sockfd, buffer, data_left + RTP_HEADER_SIZE);
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

        rtp_header.seq_no = htons(prphdl->seq++);
		memcpy(buffer, &rtp_header, sizeof(rtp_header_s));
		memcpy(buffer + 14, p_nalu_data, proc_size);
		buffer[12] = fu_indic;
		buffer[13] = fu_header;
		write(prphdl->sockfd, buffer, rtp_size);
		if(fu_end) {
			usleep(36000);
		}
		
		data_left -= proc_size;	
		p_nalu_data += proc_size;
		fu_start = 0;
	}

	return 0;	
}

void* rtp_worker_proc(void* arg)
{
	int ret = -1;
	int size = 0;
	rtp_handle_s* prphdl = (rtp_handle_s*)arg;
	char* buf = (char*)malloc(200 * 1024);
	
	printf("[%s, %d] begin___, (%d, %d)\n",
			__FUNCTION__, __LINE__, prphdl->serport, prphdl->cliport);

	buf = (char*)malloc(200 * 1024);
	if(buf == NULL) {
		printf("can not malloc memory!\n");
		return NULL;
	}

	prphdl->sockfd = create_udp_connect("127.0.0.1", prphdl->serport, prphdl->cliport);

	prphdl->start = 1;

	rtsp_stream_identify_s identify;

	identify.type = RTSP_STREAM_TYPE_VIDEO;

	while(prphdl->start) {
		if(prphdl->stream_src.get_next_frame != NULL) {
			size = 0;
			memset(buf, 0, sizeof(buf));

			ret = prphdl->stream_src.get_next_frame(prphdl->stream_src.priv,
													&identify, buf, &size);
			if(ret == 0 && size > 0) {
				ret = rtp_build_nalu(prphdl, buf, size);
			}
			
			if(ret < 0) {
				// try to sleep some time
			}
		}
	}

	free(buf);
	buf = NULL;

	close(prphdl->sockfd);

	printf("[%s, %d] end___\n", __FUNCTION__, __LINE__);

	return NULL;
}

int rtp_server_start(void **pphdl,  rtp_server_param_s* pparam)
{
	int ret = -1;
	rtp_handle_s* prphdl = NULL;

	prphdl = (rtp_handle_s*)malloc(sizeof(rtp_handle_s));
	if(prphdl == NULL) {
		return -1;
	}

	prphdl->serport = pparam->serport;
	prphdl->cliport = pparam->cliport;
	prphdl->seq = 0;
	prphdl->ssrc = random32(0);
	prphdl->timestamp = 0;

	memcpy(&prphdl->stream_src, pparam->pstream_src, sizeof(rtsp_stream_source_s));

	ret = pthread_create(&prphdl->tid, 0, rtp_worker_proc, prphdl);

	*pphdl = prphdl;

	return 0;
}

int rtp_server_stop(void **pphdl)
{
	rtp_handle_s* prphdl = (rtp_handle_s*)*pphdl;

	if(prphdl == NULL) {
		return -1;
	}

	prphdl->start = 0;
	close(prphdl->sockfd);

	pthread_join(prphdl->tid, NULL);

	free(prphdl);
	prphdl = NULL;

	return 0;
}


