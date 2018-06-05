#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "rtsp_server.h"



int main(int argc, char **argv)
{
    int ret = -1;
    unsigned short port = 8554;

    while((ret = getopt(argc, argv, "?p:h")) != -1) {
        switch(ret) {
        case 'p':
            port = atoi(optarg);
            break;

        case 'h':
            break;

        default:
            break;
        }
    }

    void* phdl = NULL;
    rtsp_server_param_s param;

    memset(&param, 0, sizeof(rtsp_server_param_s));

    param.port = port;

    param.stream_src.priv = NULL;
    param.stream_src.max_frame = 200 * 1024;
    param.stream_src.start = NULL;
    param.stream_src.stop = NULL;
    param.stream_src.get_sdp = NULL;
    param.stream_src.get_next_frame = NULL;

    ret = rtsp_server_start(&phdl, &param);
    if(ret < 0) {
        printf("start rtsp server fail!\n");
        return -1;
    }

    while(1) {
        sleep(5);
    }

    rtsp_server_stop(&phdl);
    if(ret < 0) {
        printf("stop rtsp server fail!\n");
        return -1;
    }

    return 0;
}

// ffplay rtsp://127.0.0.1:8554 -v debug

