#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <ncurses.h>

#include "inc/draw_funcs.h"
#include "inc/msg_funcs.h"

#define MAX_LEN 4097
#define INCOMING_CLR_PAIR 2


/* send message to other client */
void * recv_msg (void * data) {
    init_pair(INCOMING_CLR_PAIR, COLOR_YELLOW, COLOR_BLACK);
    
    struct msg_data * m_data = data;
    char msg_buf[MAX_LEN];

    for (;;) {
        if (recvfrom(m_data->socket_fd, (char *) msg_buf, MAX_LEN,
            MSG_WAITALL, NULL,
                NULL) < 0);
        
        attron(COLOR_PAIR(INCOMING_CLR_PAIR));
        print_msg(m_data->output_win, msg_buf, true);
        attroff(COLOR_PAIR(INCOMING_CLR_PAIR));

        memset(&msg_buf, 0, sizeof(msg_buf));
    }
}

;
