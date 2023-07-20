#include <arpa/inet.h>
#include <assert.h>
#include <curses.h>
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

#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>

#include <wchar.h>
#include <locale.h>

#define SERVER_PORT 5544
#define INCOMING_CLR_PAIR 1
#define OUTGOING_CLR_PAIR 2
#define MAX_LEN 256

struct recv_data {
    WINDOW * output_win;
    WINDOW * status_win;
    WINDOW * input_win;
    bool exit_state;
    int socket_fd;
    unsigned char aes_key[32];
	unsigned char aes_IV[32];
};


/*  | FUNCTION PROTOTYPES |  */
/* \/                    \/  */

/* clear and redraw specified window */
void win_reset (WINDOW * win);

/* print formatted message */
void print_msg (WINDOW * win, char message[], bool sender);

/* print dingbats in the little status window*/
void status_change (WINDOW * win, int state);

/* random string generator */
void gen_rand_str (unsigned char * rand_str, int len);

/* recieve messages from other client */
void * recv_msg (void * data);


/*  | MAIN |  */
/* \/     \/  */
int main (int argc, char * argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            printf("usage: neuc_client [server IP] [connection key] \n");
            return 0;
        }

        if (strcmp(argv[1], "--help") == 0) {
            printf("usage: neuc_client [server IP] [connection key] \n");
            return 0;
        }
    }

    /* ncurses initialization */
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    refresh();

    /* init color pairs */
    init_pair(INCOMING_CLR_PAIR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(OUTGOING_CLR_PAIR, COLOR_GREEN, COLOR_BLACK);

    /* init window vars */
    WINDOW * input_win, * output_win, * status_win;
    
    /* input options */
    keypad(stdscr, TRUE);
    cbreak();
    
    /* make window borders */
    input_win = newwin(3, COLS - 5, (LINES - 3), 0);
    output_win = newwin((LINES - 3), COLS, 0, 0);
    status_win = newwin(3, 5, (LINES - 3), (COLS - 5));
    
    /* screen options */
    scrollok(output_win, TRUE);
    
    /* reset boxes */
    win_reset(input_win);
    win_reset(output_win);
    win_reset(status_win);
    
    /* seed rand() */
    srand(time(NULL));
    
    /* init encryption vars */
    rsa_key rsa_key;
    symmetric_CTR ctr;
    hash_state sha512;
    int hash_idx, prng_idx, res;
    
    unsigned char rsa_key_der[270];
    unsigned long rsa_key_der_len = sizeof(rsa_key_der);
    
    unsigned char aes_key[32], aes_key_enc[256], aes_IV[32], aes_IV_enc[256];
    unsigned long aes_key_len = sizeof(aes_key), aes_key_enc_len = sizeof(aes_key_enc);

    /* init TFM variables */
    register_prng(&sprng_desc);
    
    /* register a math library (TomsFastMath)*/
    ltc_mp = tfm_desc;
    
    register_hash(&sha512_desc);
    hash_idx = find_hash("sha512");
    prng_idx = find_prng("sprng");
	
    register_cipher(&aes_desc);

    sha512_init(&sha512);

    /* generate port between 49152 and 65535 */
    int local_host_port = rand() % 16383 + 49152;

    /* variables to hold client data from server */
    uint32_t * last_addr;
    uint16_t * last_port;
    char target_addr_buf[sizeof(*last_addr) + sizeof(*last_port)];
    memset(target_addr_buf, 0, sizeof(target_addr_buf));
    last_addr   = (uint32_t *) (target_addr_buf          );
    last_port   = (uint16_t *) (last_addr + 1);

    /* initialize server info and connection key*/
    char connection_key[64];
    char server_ip[16];
    char role[1];
	
    struct sockaddr_in serv_addr, local_host_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&serv_addr, 0, sizeof(local_host_addr));

    status_change(status_win, 6);

    if (argc > 1) {
        strcpy(server_ip, argv[1]);
    } else {
        /* get the server address and the connection key */
        mvwprintw(input_win, 1, 1, "server ip: ");
        wscanw(input_win, "%s", server_ip);
        win_reset(input_win);
    }

    if (argc > 2) {
        strcpy(connection_key, argv[2]);
    } else {
        mvwprintw(input_win, 1, 1, "connection key: ");
        wscanw(input_win, "%s", connection_key);
        win_reset(input_win);
    }

    curs_set(0);

    sha512_process(&sha512, (unsigned char *) connection_key, strlen(connection_key));
    sha512_done(&sha512, (unsigned char *) connection_key);

    /* make socket */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        endwin();
        printf("error on socket creation");
        return 1;
    }

    /* bind socket and check to see if port is avaliable */
    while (bind(socket_fd, (struct sockaddr *) &local_host_addr, sizeof(local_host_addr)) < 0) {
        local_host_port = rand() % 16383 + 49152;
        local_host_addr.sin_port = htons(local_host_port);
    };

    /* filling local_host info */
    local_host_addr.sin_family = AF_INET;
    local_host_addr.sin_port = htons(local_host_port);

    /* filling server info */
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);
    inet_aton(server_ip, &serv_addr.sin_addr);

    int counter = 0;
    int status_counter = 0;
    for (;;) {
        if (!(counter % 90000))
            /* send server connection key */
            sendto(socket_fd, (const char *) connection_key, sizeof(connection_key), 
                0, (struct sockaddr *) &serv_addr, 
                    sizeof(serv_addr));

        if (!(counter % 7500)) {
            status_change(status_win, ((status_counter % 4)+ 1));
            status_counter++;
        }

        /* wait for target information from server */
        recvfrom(socket_fd, target_addr_buf, sizeof(target_addr_buf), 
            MSG_DONTWAIT, NULL, 
                NULL);
        
        if (target_addr_buf[0] != '\0') {
            break;
        }
        counter++;
        sleep((unsigned int) 0.001);
    }
    
    /* filling peer client information */
    struct sockaddr_in remote_host;
    remote_host.sin_family       = AF_INET;
    remote_host.sin_addr.s_addr  = *last_addr;
    remote_host.sin_port         = *last_port;

    /* recieve a role from the server */
    recvfrom(socket_fd, role, sizeof(role),
        MSG_WAITALL, NULL,
            NULL);

    /* role 'a' makes a RSA key, then sends the public key to the other 
    client with the 'b' role that makes an AES key, then encrypts it and sends it back*/
    if (role[0] == 'a') {
        /* make a 2048 bit RSA key */
        rsa_make_key(NULL, prng_idx, 2048/8, 65537, &rsa_key); 

        /* export key */
        rsa_export(rsa_key_der, &rsa_key_der_len, PK_PUBLIC, &rsa_key);

        /* send exported key */
        sendto (socket_fd, rsa_key_der, sizeof(rsa_key_der), 
            0, (struct sockaddr *) &remote_host, 
                sizeof(remote_host));
        
        /* recieve encrypted AES key */
        recvfrom(socket_fd, aes_key_enc, sizeof(aes_key_enc),
            MSG_WAITALL, NULL,
                NULL);
		
		/* recieve encrypted IV */
		recvfrom(socket_fd, aes_IV_enc, sizeof(aes_IV_enc),
            MSG_WAITALL, NULL,
                NULL);

        /* decrypt AES key */
        if (rsa_decrypt_key (aes_key_enc, sizeof(aes_key_enc), aes_key, &aes_key_len, 
            (const unsigned char *) "neuc", 4, hash_idx, &res, &rsa_key) != CRYPT_OK)
            print_msg(output_win, "error decrypting AES key", false);
        
		/* decrypt IV */
        if (rsa_decrypt_key (aes_IV_enc, sizeof(aes_IV_enc), aes_IV, &aes_key_len, 
            (const unsigned char *)"neuc", 4, hash_idx, &res, &rsa_key) != CRYPT_OK)
            print_msg(output_win, "error decrypting AES IV", false);
    }

    if (role[0] == 'b') {
        /* recieve RSA public key */
        recvfrom(socket_fd, rsa_key_der, sizeof(rsa_key_der),
            MSG_WAITALL, NULL,
                NULL);
        
        /* import RSA key */
        if (rsa_import(rsa_key_der, sizeof(rsa_key_der), &rsa_key) != CRYPT_OK)
            print_msg(output_win, "error importing RSA key", false);
        
        /* generate random AES key and IV*/
        gen_rand_str(aes_key, 32);
		gen_rand_str(aes_IV, 32);
        
        /* encrypt AES key */
        if (rsa_encrypt_key(aes_key, sizeof(aes_key), aes_key_enc, &aes_key_enc_len, 
            (const unsigned char *) "neuc", 4, NULL, prng_idx, hash_idx, &rsa_key) != CRYPT_OK)
            print_msg(output_win, "error encrypting AES key", false);

		if (rsa_encrypt_key(aes_IV, sizeof(aes_IV), aes_IV_enc, &aes_key_enc_len,
			(const unsigned char *) "neuc", 4, NULL, prng_idx, hash_idx, &rsa_key) != CRYPT_OK)
			print_msg(output_win, "error encrypting AES IV", false);

        /* send encrypted AES key back */
        sendto (socket_fd, aes_key_enc, sizeof(aes_key_enc), 
            MSG_CONFIRM, (struct sockaddr *) &remote_host, 
                sizeof(remote_host));
        
		/* send encrypt IV back */
		sendto (socket_fd, aes_IV_enc, sizeof(aes_IV_enc), 
            MSG_CONFIRM, (struct sockaddr *) &remote_host, 
                sizeof(remote_host));
		
    }

    /* initialize AES key */
	ctr_start(find_cipher("aes"), aes_IV, aes_key, 32, 
        0, CTR_COUNTER_LITTLE_ENDIAN, &ctr);

    /* fill data for recv thread*/
    struct recv_data data;
    data.output_win = output_win;
    data.status_win = status_win;
    data.input_win = input_win;
    
    data.socket_fd = socket_fd;
    data.exit_state = false;
    for (int i = 0; i < 32; i++) {
        data.aes_key[i] = aes_key[i];
		data.aes_IV[i] = aes_IV[i];
    }

    /* open second thread to receive data */
    pthread_t recv_msg_id;
    pthread_create(&recv_msg_id, NULL, recv_msg, &data);

    /* intialize message buffers*/
    unsigned char input_buf[MAX_LEN];
    unsigned char enc_msg[MAX_LEN];

    status_change(status_win, 0);

    /* loop to take input, check for commands, then send AES encrypted text*/
    for (;;) {
        /* get input */
        mvwscanw(input_win, 1, 1, "%[^\n]", input_buf);
        win_reset(input_win);

        /* check for commands. all commands are predicated with a "!". not case sensitive */
        /* clears all messages on the screen */
        if (strcasecmp((const char *) input_buf, "!clear") == 0) {
            win_reset(output_win);
            memset(&input_buf, 0, sizeof(input_buf));
        }

        /* exits program */
        if (strcasecmp((const char *) input_buf, "!exit") == 0) {
            sendto(socket_fd, "DISCONNECT", 10, 
                0, (struct sockaddr *) &remote_host, 
                    sizeof(remote_host));
            goto end;
        }

        if (strlen((const char *) input_buf) != 0) {            
            ctr_encrypt(input_buf, enc_msg, sizeof(input_buf), &ctr);

            /* send encrypted text */
            sendto(socket_fd, enc_msg, sizeof(enc_msg), 
                0, (struct sockaddr *) &remote_host, 
                    sizeof(remote_host));
            
            /* display outgoing message on the client */
            print_msg(output_win, (char *) input_buf, false);

            /* reset buffers */
            memset(&input_buf, 0, sizeof(input_buf));
            memset(&enc_msg, 0, sizeof(enc_msg));
        }
    }
    
    /* clean up memory, end recieving thread, and close the ncurses window */
    end:
        rsa_free(&rsa_key);
        ctr_done(&ctr);
        pthread_cancel(recv_msg_id);
        endwin();
        return 0;
}

/*  | FUNCTIONS | */
/* \/          \/ */

/* print message in specified window */
void print_msg (WINDOW * win, char message[], bool sender) {
        curs_set(0);

        if (sender) {
            wattron(win, COLOR_PAIR(INCOMING_CLR_PAIR));
            mvwprintw(win, (LINES - 4), 2, "%lc%lc%lc %s\n", (wint_t)9548, (wint_t)9548, (wint_t)9658, message);
            wattroff(win, COLOR_PAIR(INCOMING_CLR_PAIR));
        } else {
            wattron(win, COLOR_PAIR(OUTGOING_CLR_PAIR));
            mvwprintw(win, (LINES - 4), 2, "%lc %s\n", (wint_t)9658, message);
            wattroff(win, COLOR_PAIR(OUTGOING_CLR_PAIR));
        }
        
        wscrl(win, 1);
        box(win, 0, 0);

        move(LINES - 2, 1);
        curs_set(1);

        wrefresh(win);
        refresh();
}

/* clear and redraw window */
void win_reset (WINDOW * win) {
    werase(win);
    box(win, 0, 0);
    wrefresh(win);
}

void status_change (WINDOW * win, int state) {
    curs_set(0);
    switch (state) {
        case 0:
            wattron(win, COLOR_PAIR(OUTGOING_CLR_PAIR));
            mvwprintw(win, 1, 2, "%lc", (wint_t)10004);
            wattroff(win, COLOR_PAIR(OUTGOING_CLR_PAIR));
            break;

        case 1:
            mvwprintw(win, 1, 2, "%lc", (wint_t)9680);
            break;

        case 2:
            mvwprintw(win, 1, 2, "%lc", (wint_t)9682);
            break;

        case 3:
            mvwprintw(win, 1, 2, "%lc", (wint_t)9681);
            break;

        case 4:
            mvwprintw(win, 1, 2, "%lc", (wint_t)9683);
            break;
    
        case 5:
            wattron(win, COLOR_PAIR(INCOMING_CLR_PAIR));
            mvwprintw(win, 1, 2, "%lc", (wint_t)10006);
            wattroff(win, COLOR_PAIR(INCOMING_CLR_PAIR));
            break;
            
        case 6:
            mvwprintw(win, 1, 2, "%lc", (wint_t)9673);
            break;
    }

    wrefresh(win);
    move(LINES - 2, 1);
    curs_set(1);
    refresh();
    return;
}

/* random string generator */
void gen_rand_str(unsigned char * rand_str, int len) {
    unsigned char char_array[84] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()_+-=,.<>/?;:";
    for (int i = 0; i < len; i++) {
        rand_str[i] = char_array[(rand() % 84)];
    }
    
    memset(rand_str, 0, sizeof(*rand_str));
    memset(&len, 0, sizeof(len));
    return;
}

/* recieve messages from other client */
void * recv_msg (void * data) {    
    struct recv_data * m_data = data;
    symmetric_CTR ctr;
    unsigned char msg[MAX_LEN], enc_msg[MAX_LEN];
	unsigned long msg_len = sizeof(msg);

	ctr_start(find_cipher("aes"), m_data->aes_IV, m_data->aes_key, 
    32, 0, CTR_COUNTER_LITTLE_ENDIAN, &ctr);
	ctr_setiv(m_data->aes_IV, 32, &ctr);

    for (;;) {
        recvfrom (m_data->socket_fd, enc_msg, sizeof(enc_msg),
            MSG_WAITALL, NULL,
                NULL);

        if (strcasecmp((const char *) enc_msg, "DISCONNECT") == 0) {
            status_change(m_data->status_win, 5);
        } else {
            ctr_decrypt(enc_msg, msg, sizeof(msg), &ctr);

            print_msg(m_data->output_win, (char *) msg, true);
        }

        memset(&msg, 0, sizeof(msg));
        memset(&enc_msg, 0, sizeof(enc_msg));
    }
    return 0;
}