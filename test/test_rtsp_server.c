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

    param.asrc.priv = NULL;
    param.asrc.max_frame = 200 * 1024;
    param.asrc.start = NULL;
    param.asrc.stop = NULL;
    param.asrc.get_sdp = NULL;
    param.asrc.get_next_frame = NULL;

    param.vsrc.priv = NULL;
    param.vsrc.max_frame = 200 * 1024;
    param.vsrc.start = NULL;
    param.vsrc.stop = NULL;
    param.vsrc.get_sdp = NULL;
    param.vsrc.get_next_frame = NULL;

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

