#include <arpa/inet.h>
#include <string.h>

#include "rtp.h"



int rtp_build_header(rtp_header_s *phdr, unsigned char payload, unsigned short seq,
						unsigned int ts, unsigned int ssrc)
{
	memset(phdr, 0, sizeof(rtp_header_s));

	phdr->version = 2;
	phdr->padding = 0;
	phdr->extension = 0;
	phdr->csrc_len = 0;
	phdr->marker = 0;
	phdr->payload = payload;
	phdr->seq_no = htons(seq);
	phdr->timestamp = htonl(ts);
	phdr->ssrc = htonl(ssrc);

	return 0;
}

