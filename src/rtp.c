/*
Copyright (c) 2012, Paula Roquero Fuentes <paula.roquero.fuentes@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "rtp.h"

#define RTP_MIN_SIZE 12

// https://www.cnblogs.com/qingquan/archive/2011/07/28/2120440.html
/********************************************************************
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier            |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|            contributing source (CSRC) identifiers             |
|                             ....                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
********************************************************************/

int pack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size)
{
    unsigned short num_s = 0;
    unsigned long num_l = 0;
    unsigned char start_pkg[] = {128, 0};

    if (pkg_max_size < RTP_MIN_SIZE
        || pkg->d_size > pkg_max_size - RTP_MIN_SIZE) {
        return 0;
    }

    memcpy(packet, start_pkg, 2);

    packet += 2;
    num_s = htons(pkg->header->seq);
    memcpy(packet, &num_s, 2);

    packet += 2;
    num_l = htonl(pkg->header->timestamp);
    memcpy(packet, &num_l, 4);

    packet += 4;
    num_l = htonl(pkg->header->ssrc);
    memcpy(packet, &num_l, 4);

    packet += 4;
    memcpy(packet, pkg->data, pkg->d_size);

    return (RTP_MIN_SIZE + pkg->d_size);
}

int unpack_rtp(RTP_PKG *pkg, unsigned char *packet, int pkg_max_size)
{
    unsigned short num_s = 0;
    unsigned long num_l = 0;
    unsigned char start_pkg[] = {128, 0};

    if (pkg_max_size < RTP_MIN_SIZE) {
        return 0;
    }

    if (memcmp(packet, start_pkg, 2) != 0) {
        return 0;
    }

    packet += 2;
    memcpy(&num_s, packet, 2);
    pkg->header->seq = ntohs(num_s);

    packet += 2;
    memcpy(&num_l, packet, 4);
    pkg->header->timestamp = htonl(num_l);

    packet += 4;
    memcpy(&num_l, packet, 4);
    pkg->header->ssrc = ntohl(num_l);

    packet += 4;
    pkg_max_size -= RTP_MIN_SIZE;
    pkg->d_size = pkg_max_size;
    pkg->data = malloc(pkg_max_size);
    memcpy(pkg->data, packet, pkg_max_size);

    return pkg_max_size;
}

