#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rtsp_server.h"



int main(int argc, char **argv)
{
    unsigned short rtsp_port = 2000;
    unsigned short rtp_port = 2001;
    int ret;

    if (argc >= 2) {
        ret = atoi(argv[1]);
        if (ret > 1024 && ret < 60000) {
            rtsp_port = ret;
        }
    }

    if (argc == 3) {
        ret = atoi(argv[2]);
        if (ret > 1024 && ret < 60000)
            rtp_port = ret;
    }

    if (rtsp_port == rtp_port) {
        fprintf(stderr, "Ports must be different\n");
        return 0;
    }

    rtsp_server(rtsp_port, rtp_port);

    return(0);
}

// ffplay rtsp://127.0.0.1:2000 -v debug

