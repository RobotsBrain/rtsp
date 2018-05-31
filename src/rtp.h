#ifndef __RTP_H__
#define __RTP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


int rtp_start(void **pphdl, int serport, int cliport, int ssrc);


int rtp_stop(void **pphdl);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif