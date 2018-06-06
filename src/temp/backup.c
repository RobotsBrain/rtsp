/* Puts the thread to sleep
 * thread: Thread which will be put to sleep
 * sec: amount of seconds it will sleep
 * usec: amount of microseconds it will sleep
 * NOTE: It will sleep seconds + nanoseconds.
 */
void time_sleep(int sec, int usec)
{
    struct timespec t, tr;

    t.tv_sec = sec;
    t.tv_nsec = usec * 1000;

    nanosleep(&t, &tr);

    return;
}

int rtp_send_create_unicast_connection(RTP_TO_RTSP *data_from_rtp, char *uri,
                                        int Session, struct sockaddr_storage *client_addr)
{
    RTSP_TO_RTP setup_msg;
    int st;
    int sockfd;
    int rcv_sockfd;
    int rtp_sockfd;
    char *host, *path;
    struct addrinfo hints, *res;
    struct sockaddr rtp_addr;
    unsigned short port;
    char port_str[6];
    int rtp_addr_len = sizeof(struct sockaddr);

    setup_msg.order = SETUP_RTP_UNICAST;
    strcpy(setup_msg.uri, uri);
    setup_msg.Session = Session;
    setup_msg.client_ip = ((struct sockaddr_in *)client_addr)->sin_addr.s_addr;
    setup_msg.client_port = ((struct sockaddr_in *)client_addr)->sin_port;

    /* TODO: Send to rtp server */
    st = extract_uri(setup_msg.uri, &host, &path);
    if (!st || !host || !path)
        return(0);
    fprintf(stderr, "%s\n", host);

    /* Open receive socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    do {
        port = rand();
        if (!snprintf(port_str, 5, "%d", port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        rcv_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (rcv_sockfd == -1)
            return(0);

        st = bind(rcv_sockfd, res->ai_addr, res->ai_addrlen);

        port = ((struct sockaddr_in*)(res->ai_addr))->sin_port;
        freeaddrinfo(res);
    } while (st == -1);
    /* Listen in rcv_sockfd */
    st = listen(rcv_sockfd, 1);
    if (st == -1)
        return(0);

    setup_msg.response_port = port;
    fprintf(stderr, "Listening on port %d\n", port);

    sockfd = TCP_connect(host, rtp_comm_port);
    if (host)
        free(host);
    if (path)
        free(path);
    if(!sockfd)
        return(0);
    st = send(sockfd, &setup_msg, sizeof(RTSP_TO_RTP), 0);
    if (!st)
        return(0);
    close(sockfd);

    /* Open rtp_socket */
    rtp_sockfd = accept(rcv_sockfd, &rtp_addr, &rtp_addr_len);
    st = recv(rtp_sockfd, data_from_rtp, sizeof(RTP_TO_RTSP), 0);

    close(rcv_sockfd);
    close(rtp_sockfd);
    if (!st) {
        return(0);
    }
    fprintf(stderr, "order: %d\n", data_from_rtp->order);
    fprintf(stderr, "ssrc recibido: %d\n", data_from_rtp->ssrc);
    if (data_from_rtp->order == ERR_RTP) {
        return(0);
    }

    return(1);
}

int send_to_rtp(RTSP_TO_RTP *message)
{
    int st;
    int sockfd;
    int rcv_sockfd;
    int rtp_sockfd;
    char *host, *path;
    RTP_TO_RTSP response;
    struct addrinfo hints, *res;
    unsigned short port;
    char port_str[6];
    struct sockaddr rtp_addr;
    int rtp_addr_len = sizeof(struct sockaddr);

    fprintf(stderr, "extrayendo uri: %s\n", message->uri);
    st = extract_uri(message->uri, &host, &path);
    fprintf(stderr, "Status: %d\n", st);
    if (!st)
        return(0);

    /* Open receive socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    do {
        port = rand();
        if (!snprintf(port_str, 5, "%d", port))
            return(0);
        if (getaddrinfo(0, port_str, &hints, &res))
            return(0);

        rcv_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (rcv_sockfd == -1)
            return(0);

        st = bind(rcv_sockfd, res->ai_addr, res->ai_addrlen);

        port = ((struct sockaddr_in*)(res->ai_addr))->sin_port;
        freeaddrinfo(res);
    } while (st == -1);
    /* Listen in rcv_sockfd */
    st = listen(rcv_sockfd, 1);
    if (st == -1)
        return(0);

    message->response_port = port;

    sockfd = TCP_connect(host, rtp_comm_port);
    if (host)
        free(host);
    if (path)
        free(path);
    if(!sockfd)
        return(0);
    st = send(sockfd, message, sizeof(RTSP_TO_RTP), 0);
    if (!st)
        return(0);
    close(sockfd);

    rtp_sockfd = accept(rcv_sockfd, &rtp_addr, &rtp_addr_len);
    fprintf(stderr, "Valor de rcv_sockfd: %d\n", rcv_sockfd);
    fprintf(stderr, "Valor de rtp_sockfd: %d\n", rtp_sockfd);
    if (rtp_sockfd == -1) {
        perror("error: ");
        return(0);
    }
    st = recv(rtp_sockfd, &response, sizeof(RTP_TO_RTSP), 0);
    close(rcv_sockfd);
    close(rtp_sockfd);
    fprintf(stderr, "Status de receive: %d\n", st);
    if (st == -1) {
        perror("error: ");
        return(0);
    }
    fprintf(stderr, "Response order: %d\n", response.order);
    if (response.order == ERR_RTP)
        return(0);

    return(1);
}

int rtp_send_play(char *uri, unsigned int ssrc)
{
    RTSP_TO_RTP play_msg;

    play_msg.order = PLAY_RTP;
    play_msg.ssrc = ssrc;
    strcpy(play_msg.uri, uri);

    fprintf(stderr, "rtp_send_play\n");

    return send_to_rtp(&play_msg);
}

int rtp_send_pause(char *uri, unsigned int ssrc)
{
    RTSP_TO_RTP pause_msg;

    pause_msg.order = PAUSE_RTP;
    pause_msg.ssrc = ssrc;
    strcpy(pause_msg.uri, uri);

    return send_to_rtp(&pause_msg);
}

int rtp_send_teardown(char *uri, unsigned int ssrc)
{
    RTSP_TO_RTP teardown_msg;

    teardown_msg.order = TEARDOWN_RTP;
    teardown_msg.ssrc = ssrc;
    strcpy(teardown_msg.uri, uri);

    return send_to_rtp(&teardown_msg);
}


static int receive_message(int sockfd, char* buf, int buf_size)
{
    int ret = 0;
    int read = 0;
    int content_length = 0;
    char* tmp = NULL;
    size_t tmp_size = 0;
    FILE* f = NULL;

    memset(buf, 0, buf_size);

    f = fdopen(sockfd, "r");

    do {
        read = getline(&tmp, &tmp_size, f);
        if(read <= 0) {
            fclose(f);
            free(tmp);
            return -1;
        }

        if(read + ret >= buf_size) {
            fclose(f);
            free(tmp);
            return 0;
        }

        if(!content_length && !strncmp(tmp, "Content-Length:", 15)) {
            if(sscanf(tmp, "Content-Length: %d", &content_length) < 1) {
                content_length = 0;
            }
        }

        strncpy(buf + ret, tmp, read);
        ret += read;
    } while(tmp[0] != '\r');

    if(content_length) {
        content_length += ret;

        do {
            read = getline(&tmp, &tmp_size, f);
            if(read <= 0) {
                fclose(f);
                free(tmp);
                return -1;
            }

            if(read + ret >= buf_size) {
                fclose(f);
                free(tmp);
                return 0;
            }

            memcpy(buf + ret, tmp, read);
            ret += read;
        } while(ret < content_length);
    }

    if(tmp != NULL) {
        fprintf(stderr, "\n########################## RECEIVED ##########################\n%s", buf);
        free(tmp);
        tmp = NULL;
    }

    return ret;
}

void free_rtsp_req(RTSP_REQUEST **req)
{
    if ((*req)->uri) {
        free((*req)->uri);
    }
    free(*req);
    *req = 0;

    return;
}


do {
            server_error = 0;

            memset(buf, 0, REQ_BUFFER);

            st = read(sockfd, buf, REQ_BUFFER);
            if (st == -1) {
                return -1;
            }
            
fprintf(stderr, "\n########################## RECEIVED ##########################\n%s", buf);
            st = unpack_rtsp_req(req, buf, st);
            if (server_error && st) {
                fprintf(stderr, "caca1\n");
                res = rtsp_server_error(req);
                if (res) {
                    st = rtsp_pack_response(res, buf, REQ_BUFFER);
                    if (st) {
                        buf[st] = 0;
                        send(sockfd, buf, st, 0);
                    }
                    rtsp_free_response(&res);
                }
            } else if (server_error || !st) {
                memcpy(buf, "RTSP/1.0 500 Internal server error\r\n\r\n", 38);
                buf[38] = 0;
                send(sockfd, buf, strlen(buf), 0);
            }
        } while(server_error || !st);

#if 0
int abstr_nalu_indic(unsigned char *buf, int buf_size, int *be_found)
{
    int offset = 0;
    int frame_size = 4;
    unsigned char *p_tmp = buf + 4;

    *be_found = 0;
    
    while(frame_size < buf_size - 4) {
        if(p_tmp[2]) {
            offset = 3;
        } else if(p_tmp[1]) {
            offset = 2;
        } else if(p_tmp[0]) {
            offset = 1;
        } else {
            if(p_tmp[3] != 1) {
                if(p_tmp[3]) {
                    offset = 4;
                } else {
                    offset = 1;
                }
            } else {
                *be_found = 1;
                break;
            }
        }

        frame_size += offset;
        p_tmp += offset;
    }

    if(!*be_found) {
        frame_size = buf_size;
    }

    return frame_size;
}

long GetFileSize(FILE *infile)
{
    long size_of_file;

    fseek(infile, 0L, SEEK_END);

    size_of_file = ftell(infile);

    fseek(infile, 0L, SEEK_SET);

    return size_of_file;
}

    FILE *infile = NULL;
    int total_size = 0, bytes_consumed = 0, frame_size = 0, bytes_left;
    unsigned char inbufs[READ_LEN] = {0}, outbufs[READ_LEN] = {0};
    unsigned char *p_tmp = NULL;
    int found_nalu = 0;
    int reach_last_nalu = 0;

    infile = fopen("1.h264", "rb");
    if(infile == NULL) {
        printf("please check media file\n");
        return NULL;
    }
    
    total_size = GetFileSize(infile);
    if(total_size <= 4) {
        fclose(infile);
        return NULL;    
    }

    while(1) {
        bytes_left = fread(inbufs, 1, READ_LEN, infile);
        p_tmp = inbufs;

        while(bytes_left > 0) {
            frame_size = abstr_nalu_indic(p_tmp, bytes_left, &found_nalu);
            reach_last_nalu = (bytes_consumed + frame_size >= total_size); 

            if(found_nalu || reach_last_nalu) {       
                memcpy(outbufs, p_tmp, frame_size);

                rtp_build_nalu(prphdl, outbufs, frame_size);
                p_tmp += frame_size;
                bytes_consumed += frame_size;

                // if(reach_last_nalu) {
                //      rtsp[cur_conn_num]->is_runing = 0;
                // }
            }

            bytes_left -= frame_size;
        }
     
        fseek(infile, bytes_consumed, SEEK_SET);
    }

    fclose(infile);
#endif