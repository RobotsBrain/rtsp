#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "rtsp_server.h"



int main(int argc, char **argv)
{
    int res = 0;
    unsigned short port = 8554;

    while((res = getopt(argc, argv, "?p:h")) != -1) {
        switch(res) {
        case 'p':
            port = atoi(optarg);
            break;

        case 'h':
            break;

        default:
            break;
        }
    }

    int ret = -1;
    void* phdl = NULL;

    ret = rtsp_server_start(&phdl, port);
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

