#ifndef __RTP_H__
#define __RTP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************************************************************
 0					 1					 2					 3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC	|M| 	PT		|		sequence number 		|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|							timestamp							|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|			synchronization source (SSRC) identifier			|
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|			 contributing source (CSRC) identifiers 			|
|							  ....								|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

****************************************************************/

#define RTP_SIZE_MAX    	1460
#define RTP_HEADER_SIZE 	12


typedef struct rtp_header_ {
	/* byte 0 */
	unsigned char csrc_len:4;
	unsigned char extension:1;
	unsigned char padding:1;
	unsigned char version:2;
	
	/* byte 1 */
	unsigned char payload:7;
	unsigned char marker:1;

	/* bytes 2, 3 */
	unsigned short seq_no;

	/* bytes 4-7 */
	unsigned int timestamp;

	/* bytes 8-11 */
	unsigned int ssrc;					/* stream number is used here. */
} rtp_header_s;


int rtp_build_header(rtp_header_s *phdr, unsigned short seq,
						unsigned int ts, unsigned int ssrc);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif