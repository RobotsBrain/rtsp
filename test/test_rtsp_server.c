#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "rtsp_server.h"



typedef struct test_rtsp_server_handle_ {
    int     vfd;
    char*   buf;
    int     offset;
    int     size;
} test_rtsp_server_handle_s;


int get_one_nalu(char *pBufIn, int nInSize, char *pNalu, int* nNaluSize)
{
    char *p = pBufIn;
    int nStartPos = 0, nEndPos = 0;

    while (1) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            nStartPos = p - pBufIn;
            break;
        }

        p++;

        if (p - pBufIn >= nInSize - 4) {
            return 0;
        }
    }

    p++;

    while (1) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            nEndPos = p - pBufIn;
            break;
        }

        p++;

        if (p - pBufIn >= nInSize - 4) {
            nEndPos = nInSize;
            break;
        }
    }

    *nNaluSize = nEndPos - nStartPos;
    memcpy(pNalu, pBufIn + nStartPos, *nNaluSize);

    return 1;
}

int start(void* thiz, rtsp_stream_identify_s* pidentify)
{
    if(thiz == NULL) {
        return -1;
    }

    test_rtsp_server_handle_s* pthdl = (test_rtsp_server_handle_s*)thiz;

    printf("[%s, %d]\n", __FUNCTION__, __LINE__);

    return 0;
}

int stop(void* thiz, rtsp_stream_identify_s* pidentify)
{
    if(thiz == NULL) {
        return -1;
    }

    test_rtsp_server_handle_s* pthdl = (test_rtsp_server_handle_s*)thiz;

    printf("[%s, %d]\n", __FUNCTION__, __LINE__);

    return 0;
}

int get_sdp(void* thiz, rtsp_stream_identify_s* pidentify, char* buf, int* size)
{
    if(thiz == NULL) {
        return -1;
    }

    test_rtsp_server_handle_s* pthdl = (test_rtsp_server_handle_s*)thiz;

    printf("[%s, %d]\n", __FUNCTION__, __LINE__);

    return 0;
}

int get_next_frame(void* thiz, rtsp_stream_identify_s* pidentify, char* buf, int* size)
{
    if(thiz == NULL) {
        return -1;
    }

    test_rtsp_server_handle_s* pthdl = (test_rtsp_server_handle_s*)thiz;

    if(pthdl->offset >= pthdl->size) {
        pthdl->offset = 0;
    }

    if(get_one_nalu(pthdl->buf + pthdl->offset, pthdl->size - pthdl->offset, buf, size) == 0) {
        return -1;
    }

    pthdl->offset += *size;

    printf("[%s, %d] %p, offset: %d, size: %d\n",
            __FUNCTION__, __LINE__, pthdl, pthdl->offset, *size);

    return 0;
}

int main(int argc, char **argv)
{
    int ret = -1;
    unsigned short port = 8554;
    char vfile[128] = {0};

    while((ret = getopt(argc, argv, "?p:v:h")) != -1) {
        switch(ret) {
        case 'p':
            port = atoi(optarg);
            break;

        case 'v':
            strcpy(vfile, optarg);
            break;

        case 'h':
            break;

        default:
            break;
        }
    }

    void* phdl = NULL;
    rtsp_server_param_s param;
    test_rtsp_server_handle_s testhdl;

    memset(&param, 0, sizeof(rtsp_server_param_s));
    memset(&testhdl, 0, sizeof(test_rtsp_server_handle_s));

    param.port = port;
    param.stream_src.priv = &testhdl;
    param.stream_src.max_frame = 200 * 1024;
    param.stream_src.start = start;
    param.stream_src.stop = stop;
    param.stream_src.get_sdp = get_sdp;
    param.stream_src.get_next_frame = get_next_frame;

    struct stat fstat;

    stat(vfile, &fstat);

    testhdl.vfd = open(vfile, O_RDONLY|O_NONBLOCK);
    testhdl.buf = (char *)malloc(fstat.st_size);
    testhdl.size = fstat.st_size;

    read(testhdl.vfd, testhdl.buf, fstat.st_size);

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

    free(testhdl.buf);
    close(testhdl.vfd);

    return 0;
}

// ffplay rtsp://127.0.0.1:8554 -v debug

