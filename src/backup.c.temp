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
