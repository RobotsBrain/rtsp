/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _PARSE_RTP_
#define _PARSE_RTP_


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    unsigned short seq;
    unsigned int timestamp;
    unsigned int ssrc;
} RTP_HEADER;


typedef struct {
    RTP_HEADER header[1];
    unsigned char *data; /* Reserved memory when parsing */
    int d_size;
} RTP_PKG;


int pack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size);


int unpack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
