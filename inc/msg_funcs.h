#ifndef MSG_FUNCS_H
#define MSG_FUNCS_H

struct msg_data {
    WINDOW * output_win;
    int socket_fd;
};

void * recv_msg (void * data);

#endif
