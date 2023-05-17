#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/random.h>
#include <tomcrypt.h>
#include <tfm.h>

#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>

#include <locale.h>

#include "inc/msg_funcs.h"
#include "inc/draw_funcs.h"

#define SERVER_PORT 5544
#define MAX_LEN 4097


int main () {
    /* ncurses initialization */
    setlocale(LC_ALL, "");
    initscr();
    refresh();

    WINDOW *text_box;
    WINDOW *messages;
    
    keypad(stdscr, TRUE);
    cbreak();

    text_box = newwin(3, COLS, (LINES - 3), 0);
    messages = newwin((LINES - 3), COLS, 0, 0);
    
    scrollok(messages, TRUE);

    win_reset(text_box);
    win_reset(messages);
    
    /* message var */
    char input_buf[MAX_LEN];

    /* seed rand() */
    srand(time(NULL));

    /* generate port*/
    int my_port = rand() % 16383 + 49152;

    /* variables to hold client data from server */
    uint32_t * last_addr;
    uint16_t * last_port;
    char target_addr_buf[sizeof(*last_addr) + sizeof(*last_port)];
    memset(target_addr_buf, 0, sizeof(target_addr_buf));
    last_addr   = (uint32_t *) (target_addr_buf          );
    last_port   = (uint16_t *) (last_addr + 1);

    /* initialize server info and connection key*/
    char connection_key[MAX_LEN];
    char server_ip[16];

    /* get server address, port, and the connection key */
    mvwprintw(text_box, 1, 1, "server ip: ");
    wscanw(text_box, "%s", server_ip);
    win_reset(text_box);

    mvwprintw(text_box, 1, 1, "connection key: ");
    wscanw(text_box, "%s", connection_key);
    win_reset(text_box);

    /* make socket */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	
    if (socket_fd < 0) {
        endwin();
        printf("error on socket creation");
        return 1;
    }

    /* filling server information */
	struct sockaddr_in serv_addr, my_addr;
	
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&serv_addr, 0, sizeof(my_addr));

    /* localhost */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);

    /* target server */
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);
    inet_aton(server_ip, &serv_addr.sin_addr);

    /* bind socket and check to see if port is avaliable */
    while (bind(socket_fd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
        my_port = rand() % 16383 + 49152;
        my_addr.sin_port = htons(my_port);
    };

    /* send server connection key */
    sendto(socket_fd, (const char *) connection_key, strlen(connection_key), 
        MSG_CONFIRM, (struct sockaddr *) &serv_addr, 
            sizeof(serv_addr));
    
    /* wait for contact from server */
    recvfrom(socket_fd, target_addr_buf, sizeof(target_addr_buf), 
        MSG_WAITALL, NULL, 
            NULL);

    /* filling peer client information */
    struct sockaddr_in remote;
    remote.sin_family       = AF_INET;
    remote.sin_addr.s_addr  = *last_addr;
    remote.sin_port         = *last_port;

    struct msg_data data;
    data.output_win = messages;
    data.socket_fd = socket_fd;

    pthread_t recv_msg_thr_id;
    pthread_create(&recv_msg_thr_id, NULL, recv_msg, &data);

    sleep(1);
    win_reset(messages);

    for (;;) {
        mvwscanw(text_box, 1, 1, "%[^\n]", input_buf);
        win_reset(text_box);

        if (strcasecmp(input_buf, "!clear") == 0) {
            win_reset(messages);
            memset(&input_buf, 0, sizeof(input_buf));
        }

        if (strcasecmp(input_buf, "!exit") == 0) {
            goto end;
        }

        if (strlen(input_buf) != 0) {
            sendto(socket_fd, input_buf, strlen(input_buf), 
                MSG_CONFIRM, (struct sockaddr *) &remote, 
                    sizeof(remote));
            
            print_msg(messages, input_buf, false);
            
            memset(&input_buf, 0, sizeof(input_buf));
        }
    }
    
    end:
        pthread_cancel(recv_msg_thr_id);
        endwin();
        return 0;
}
