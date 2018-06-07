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

#if 0
static int detect_attr_res(RTSP_RESPONSE *res, char *tok_start, int text_size)
{
    int i;
    int attr = -1;
    int attr_len;
    attr_len = strcspn(tok_start, ":");

    if (attr_len == text_size || attr_len == 0)
        return(0);

    for (i = 0; i < sizeof(ATTR_STR)/sizeof(ATTR_STR[0]); ++i) {
        if (!memcmp(ATTR_STR[i], tok_start, attr_len)) {
            attr = i;
            break;
        }
    }
    tok_start += attr_len;
    text_size -= attr_len - 1;

    /* Ignore spaces after ':' */
    while (*(++tok_start) == ' ') {
        --text_size;
    }

    attr_len = strcspn(tok_start, "\r\n");
    if (attr_len == text_size || attr_len == 0)
        return(0);

    switch (attr) {
    case CSEQ_STR:
        res->CSeq = atoi(tok_start);
        break;

    case SESSION_STR:
        res->Session = atoi(tok_start);
        break;

    case CONTENT_TYPE_STR:
        if (memcmp(tok_start, SDP_STR, attr_len))
            return(0);
        break;

    case CONTENT_LENGTH_STR:
        res->Content_Length = atoi(tok_start);
        if (!res->Content_Length)
            return(0);
        break;

    case TRANSPORT_STR:
        /* The only acceptable transport is RTP */
        if (!strnstr(tok_start, RTP_STR, attr_len))
            return(0);
        /* Check if the transport is unicast or multicast */
        if (strnstr(tok_start, CAST_STR[UNICAST], attr_len))
            res->cast = UNICAST;
        else if (strnstr(tok_start, CAST_STR[MULTICAST], attr_len))
            res->cast = MULTICAST;
        else
            return(0);

        /* Get the client ports */
        if ( (tok_start = strnstr(tok_start, CLIENT_PORT_STR, attr_len)) ) {
            if (!tok_start)
                return(0);
            tok_start += strlen(CLIENT_PORT_STR);
            res->client_port = (unsigned short)atoi(tok_start);
            if (res->client_port == 0)
                return(0);
        }

        /* Get the client ports */
        if ( (tok_start = strnstr(tok_start, SERVER_PORT_STR, attr_len)) ) {
            if (!tok_start)
                return(0);
            tok_start += strlen(SERVER_PORT_STR);
            res->server_port = (unsigned short)atoi(tok_start);
            if (res->server_port == 0) {
                return 0;
            }
        }
        break;

    default:
        return 1;
    }

    return 1;
}

RTSP_REQUEST *construct_rtsp_request(METHOD method, const unsigned char *uri, int Session,
                                    TRANSPORT_CAST cast, unsigned short client_port)
{
    int CSeq = 0;
    RTSP_REQUEST *req = 0;
    int uri_len = strlen((char *)uri);
    req = malloc(sizeof(RTSP_REQUEST));
    if (!req) {
        return NULL;
    }

    req->method = method;
    req->uri = malloc(uri_len + 1);
    if (!req->uri) {
        free(req);
        return NULL;
    }

    strcpy((char *)req->uri, (char *)uri);

    req->uri[uri_len] = 0;
    req->CSeq = ++CSeq;
    req->Session = Session;
    req->cast = cast;
    req->client_port = client_port;

    return req;
}

RTSP_REQUEST *rtsp_describe(const unsigned char *uri)
{
    return construct_rtsp_request(DESCRIBE, uri, -1, UNICAST, 0);
}

RTSP_REQUEST *rtsp_setup(const unsigned char *uri, int Session,
                        TRANSPORT_CAST cast, unsigned short client_port)
{
    return construct_rtsp_request(SETUP, uri, Session, cast, client_port);
}

RTSP_REQUEST *rtsp_play(const unsigned char *uri, int Session)
{
    return construct_rtsp_request(PLAY, uri, Session, UNICAST, 0);
}

RTSP_REQUEST *rtsp_pause(const unsigned char *uri, int Session)
{
    return construct_rtsp_request(PAUSE, uri, Session, UNICAST, 0);
}

RTSP_REQUEST *rtsp_teardown(const unsigned char *uri, int Session)
{
    return construct_rtsp_request(TEARDOWN, uri, Session, UNICAST, 0);
}

int pack_rtsp_req(RTSP_REQUEST *req, char *req_text, int text_size)
{
    int ret;
    int written;

    /* Save space for the last \0 */
    --text_size;
    req_text[text_size] = 0;

    /* Write method and uri */
    if (req->method == -1 || !req->uri)
        return(0);
    ret = written = snprintf(req_text, text_size, "%s %s RTSP/1.0\r\n", METHOD_STR[req->method], req->uri);
    if (ret < 0 || ret >= text_size)
        return(0);

    /* CSeq must have a value always*/
    if (!req->CSeq)
        return(0);
    /* Print to req_text */
    ret = snprintf(req_text + written, text_size - written, "CSeq: %d\r\n", req->CSeq);
    /* Check if the printed text was larger than the space available in req_text */
    if (ret < 0 || ret >= text_size-written)
        return(0);
    /* Add the number of characters written to written */
    written += ret;

    /* Write session number */
    if (req->Session != -1) {
        ret = snprintf(req_text + written, text_size - written, "Session: %d\r\n", req->Session);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    } else {
        /* Session must be present in PLAY, PASE and TEARDOWN */
        if (req->method == PLAY || req->method == PAUSE || req->method == TEARDOWN)
            return(0);
    }

    /* Accept only application/sdp in DESCRIBE */
    if (req->method == DESCRIBE) {
        ret = snprintf(req_text + written, text_size - written, "Accept: application/sdp\r\n");
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    }

    /* Write client port */
    if (req->client_port) {
        ret = snprintf(req_text + written, text_size - written, "Transport: RTP/AVP;%s;client_port=%d-%d\r\n", CAST_STR[req->cast], req->client_port, req->client_port + 1);
        if (ret < 0 || ret >= text_size - written)
            return(0);
        written += ret;
    } else {
        /* client_port must be present in SETUP */
        if (req->method == SETUP)
            return(0);
    }
    ret = snprintf(req_text + written, text_size - written, "\r\n");
    if (ret < 0 || ret >= text_size - written)
        return(0);
    written += ret;

    req_text[written] = 0;

    return(written);
}

RTSP_RESPONSE *rtsp_describe_res(RTSP_REQUEST *req)
{
    SDP sdp;
    int uri_len = strlen(req->uri);
    char sdp_str[1024] = {0};
    int sdp_len = 0;
    RTSP_RESPONSE *ret = NULL;

    if (strstr(req->uri, "audio") || strstr(req->uri, "video")) {
        return NULL;
    }

    sdp.n_medias = 2;
    sdp.uri = (unsigned char *)req->uri;
    sdp.medias = malloc(sizeof(MEDIA) * 2);
    if (!sdp.medias) {
        return NULL;
    }

    sdp.medias[0]->type = AUDIO;
    sdp.medias[0]->port = 0;
    sdp.medias[0]->uri = malloc(uri_len + 8);
    if (!sdp.medias[0]->uri) {
        free(sdp.medias);
        return NULL;
    }
    memcpy(sdp.medias[0]->uri, req->uri, uri_len);
    memcpy(sdp.medias[0]->uri + uri_len, "/audio", 7);
    sdp.medias[0]->uri[uri_len + 7] = 0;

    sdp.medias[1]->type = 96;
    sdp.medias[1]->port = 0;
    sdp.medias[1]->uri = malloc(uri_len + 8);
    if (!sdp.medias[1]->uri) {
        free(sdp.medias[0]->uri);
        free(sdp.medias);
        return NULL;
    }
    memcpy(sdp.medias[1]->uri, req->uri, uri_len);
    memcpy(sdp.medias[1]->uri + uri_len, "/video", 7);
    sdp.medias[1]->uri[uri_len + 7] = 0;

    if (!(sdp_len = pack_sdp(&sdp, (unsigned char *)sdp_str, 1024))) {
        free(sdp.medias[0]->uri);
        free(sdp.medias[1]->uri);
        free(sdp.medias);
        return NULL;
    }

    char* addstr = "a=rtpmap:96 H264/90000\r\n";

    strcat(sdp_str, addstr);
    sdp_len += strlen(addstr);
    ret = construct_rtsp_response(200, -1, 0, 0, 0, sdp_len, sdp_str, 0, req);

    free(sdp.medias[0]->uri);
    free(sdp.medias[1]->uri);
    free(sdp.medias);

    return ret;
}

int unpack_rtsp_res(RTSP_RESPONSE *res, char *res_text, int text_size)
{
    char *tok_start;
    int tok_len;
    int attr;
    tok_start = res_text;

    /* Initialize structure */
    res->code = -1;
    res->CSeq = -1;
    res->Session = -1;
    res->client_port = 0;
    res->server_port = 0;
    res->Content_Length = -1;
    res->content = 0;
    res->options = 0;

    /* Get rtsp token */
    tok_len = strcspn(tok_start, " ");
    if (tok_len == text_size)
        return(0);

    /* Check if the rtsp token is valid */
    if (strncmp(tok_start, RTSP_STR, 8))
        return(0);
    /* Prepare for next token */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;

    /* Check response code */
    if (*tok_start != '2')
        return(0);
    res->code = atoi(tok_start);


    /* Ignore rest of line */
    tok_len = strcspn(tok_start, "\r\n");
    if (tok_len == text_size)
        return(0);
    /* Ignore next \r or \n */
    tok_start += tok_len + 1;
    text_size -= tok_len + 1;
    if (*tok_start == '\r' || *tok_start == '\n') {
        ++tok_start;
        --text_size;
    }

    /* Get Attributes until we find an empty line */
    while ( (tok_len = strcspn(tok_start, "\r\n")) ) {
        if (tok_len == text_size) {
            if (res->content)
                free(res->content);
            res->content = 0;
            return(0);
        }
        /* Get all the attributes */
        attr = detect_attr_res(res, tok_start, text_size);
        if (!attr) {
            if (res->content)
                free(res->content);
            res->content = 0;
            return(0);
        }
        tok_start += tok_len + 1;
        text_size -= tok_len + 1;
        if (*tok_start == '\r' || *tok_start == '\n') {
            ++tok_start;
            --text_size;
        }
    }
    /* If text_size is 0, a last \r\n wasn't received */
    if (text_size == 0)
        return(0);

    /* If there is content */
    if (res->Content_Length != -1) {
        /* Ignore empty line */
        tok_start += 2;
        text_size -= 2;
        if (res->Content_Length > text_size)
            return(0);

        res->content = malloc(res->Content_Length + 1);
        if (!res->content)
            return(0);

        /* Copy content */
        memcpy(res->content, tok_start, res->Content_Length);
        res->content[res->Content_Length] = 0;
    }

    /* Obligatory */
    if (res->code == -1) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }
    /* Obligatory */
    if (res->CSeq == -1) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    /* Is an error having Content-Length but not having content */
    if ((res->content && res->Content_Length == -1) ||
            (!res->content && res->Content_Length != -1)) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    /* Is an error having content and transport */
    if (res->content && res->client_port) {
        if (res->content)
            free(res->content);
        res->content = 0;
        return(0);
    }

    return(1);
}
#endif

