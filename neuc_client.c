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

#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>

#include <wchar.h>
#include <locale.h>

#define SERVER_PORT 5544
#define MAX_LEN 256

struct print_data {
    WINDOW * output_win;
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

/* random string generator */
void rand_string(unsigned char * srng, int len);

/* recieve messages from other client */
void * recv_msg (void * data);


/*  | MAIN |  */
/* \/     \/  */
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
    
    /* seed rand() */
    srand(time(NULL));
    
    /* init encryption */
    rsa_key r_key;
    symmetric_CTR ctr;
    unsigned char key_der[270], aes_key[32], aes_IV[32], aes_key_enc[256], aes_IV_enc[256];
    unsigned long key_der_len = sizeof(key_der), aes_key_len = sizeof(aes_key), aes_key_enc_len = sizeof(aes_key_enc);
    int hash_idx, prng_idx, res;

    /* register prng/hash */
    register_prng(&sprng_desc);
    
    /* register a math library (in this case TomsFastMath) */
    ltc_mp = tfm_desc;
    register_hash(&sha1_desc);
    hash_idx = find_hash("sha1");
    prng_idx = find_prng("sprng");
   
	register_cipher(&aes_desc);

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
    char role[1];
	
    struct sockaddr_in serv_addr, my_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&serv_addr, 0, sizeof(my_addr));

    /* get the server address and the connection key */
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

    /* bind socket and check to see if port is avaliable */
    while (bind(socket_fd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
        my_port = rand() % 16383 + 49152;
        my_addr.sin_port = htons(my_port);
    };

    /* filling localhost info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);

    /* filling server info */
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);
    inet_aton(server_ip, &serv_addr.sin_addr);

    /* send server connection key */
    sendto(socket_fd, (const char *) connection_key, strlen(connection_key), 
        MSG_CONFIRM, (struct sockaddr *) &serv_addr, 
            sizeof(serv_addr));
    
    /* wait for target information from server */
    recvfrom(socket_fd, target_addr_buf, sizeof(target_addr_buf), 
        MSG_WAITALL, NULL, 
            NULL);
    
    /* filling peer client information */
    struct sockaddr_in remote;
    remote.sin_family       = AF_INET;
    remote.sin_addr.s_addr  = *last_addr;
    remote.sin_port         = *last_port;

    /* recieve a role from the server */
    recvfrom(socket_fd, role, sizeof(role),
        MSG_WAITALL, NULL,
            NULL);

    /* role 'a' makes a RSA key, then sends the public key to the other 
    client with the 'b' role that makes an AES key, then encrypts it and sends it back*/
    if (role[0] == 'a') {
        /* make a 2048 bit RSA key */
        rsa_make_key(NULL, prng_idx, 2048/8, 65537, &r_key); 

        /* export key */
        rsa_export(key_der, &key_der_len, PK_PUBLIC, &r_key);

        /* send exported key */
        sendto (socket_fd, key_der, sizeof(key_der), 
            MSG_CONFIRM, (struct sockaddr *) &remote, 
                sizeof(remote));
        
        /* recieve encrypted AES key */
        if (recvfrom(socket_fd, aes_key_enc, sizeof(aes_key_enc),
            MSG_WAITALL, NULL,
                NULL) < 0)
            print_msg(messages, "ERROR ON RECIEVING AES KEY", false);
		
		/* recieve encrypted IV */
		if (recvfrom(socket_fd, aes_IV_enc, sizeof(aes_IV_enc),
            MSG_WAITALL, NULL,
                NULL) < 0)
            print_msg(messages, "ERROR ON RECIEVING IV KEY", false);

        /* decrypt AES key */
        if (rsa_decrypt_key (aes_key_enc, sizeof(aes_key_enc), aes_key, &aes_key_len, 
            "neuc", 4, hash_idx, &res, &r_key) != CRYPT_OK)
            print_msg(messages, "ERROR ON DECRYPTING AES KEY", false);
        
		/* decrypt IV */
        if (rsa_decrypt_key (aes_IV_enc, sizeof(aes_IV_enc), aes_IV, &aes_key_len, 
            "neuc", 4, hash_idx, &res, &r_key) != CRYPT_OK)
            print_msg(messages, "ERROR ON DECRYPTING AES KEY", false);
    }

    if (role[0] == 'b') {
        /* recieve RSA public key */
        recvfrom(socket_fd, key_der, sizeof(key_der),
            MSG_WAITALL, NULL,
                NULL);
        
        /* import RSA key */
        if (rsa_import (key_der, sizeof(key_der), &r_key) != CRYPT_OK)
            print_msg(messages, "ERROR ON RSA IMPORT", false);
        
        /* generate random AES key and IV*/
        rand_string(aes_key, 32);
		rand_string(aes_IV, 32);
        
        /* encrypt AES key */
        if (rsa_encrypt_key(aes_key, sizeof(aes_key), aes_key_enc, &aes_key_enc_len, 
            "neuc", 4, NULL, prng_idx, hash_idx, &r_key) != CRYPT_OK)
            print_msg(messages, "ERROR ON ENCRYPTING AES KEY", false);

		if (rsa_encrypt_key(aes_IV, sizeof(aes_IV), aes_IV_enc, &aes_key_enc_len,
			"neuc", 4, NULL, prng_idx, hash_idx, &r_key) != CRYPT_OK)
			print_msg(messages, "ERROR ON ENCRYPTING IV", false);

        /* send encrypted AES key back */
        sendto (socket_fd, aes_key_enc, sizeof(aes_key_enc), 
            MSG_CONFIRM, (struct sockaddr *) &remote, 
                sizeof(remote));
        
		/* send encrypt IV back */
		sendto (socket_fd, aes_IV_enc, sizeof(aes_IV_enc), 
            MSG_CONFIRM, (struct sockaddr *) &remote, 
                sizeof(remote));
		
    }

    /* initialize AES key */
	ctr_start(find_cipher("aes"), aes_IV, aes_key, 32, 0, CTR_COUNTER_LITTLE_ENDIAN, &ctr);

    /* fill data for recv thread*/
    struct print_data data;
    data.output_win = messages;
    data.socket_fd = socket_fd;
    for (int i = 0; i < 32; i++) {
        data.aes_key[i] = aes_key[i];
		data.aes_IV[i] = aes_IV[i];
    }

    /* open second thread to receive data */
    pthread_t recv_msg_thr_id;
    pthread_create(&recv_msg_thr_id, NULL, recv_msg, &data);

    /* intialize message buffers*/
    unsigned char input_buf[MAX_LEN];
    unsigned char enc_msg[MAX_LEN];

    /* loop to take input, check for commands, then send AES encrypted text*/
    for (;;) {
        /* get input */
        mvwscanw(text_box, 1, 1, "%[^\n]", input_buf);
        win_reset(text_box);

        /* check for commands. all commands are predicated with a "!". not case sensitive */
        /* clears all messages on the screen */
        if (strcasecmp(input_buf, "!clear") == 0) {
            win_reset(messages);
            memset(&input_buf, 0, sizeof(input_buf));
        }

        /* exits program */
        if (strcasecmp(input_buf, "!exit") == 0) {
            goto end;
        }

        if (strlen(input_buf) != 0) {            
            ctr_encrypt(input_buf, enc_msg, sizeof(input_buf), &ctr);

            /* send encrypted text */
            sendto(socket_fd, enc_msg, sizeof(enc_msg), 
                MSG_CONFIRM, (struct sockaddr *) &remote, 
                    sizeof(remote));
            
            /* display outgoing message on the client */
            print_msg(messages, input_buf, false);
            
            /* reset buffers */
            memset(&input_buf, 0, sizeof(input_buf));
            memset(&enc_msg, 0, sizeof(enc_msg));
        }
    }
    
    /* clean up memory, end recieving thread, and close the ncurses window */
    end:
        rsa_free(&r_key);
        ctr_done(&ctr);
        pthread_cancel(recv_msg_thr_id);
        endwin();
        return 0;
}

/*  | FUNCTIONS | */
/* \/          \/ */

/* print message in specified window */
void print_msg (WINDOW * win, char message[], bool sender) {
        switch (sender) {
            case true:
                mvwprintw(win, 39, 2, "%lc%lc%lc %s\n", (wint_t)9472, (wint_t)9472, (wint_t)10148, message);
                break;
            
            case false:
                mvwprintw(win, 39, 2, "%lc %s\n", (wint_t)10148, message);
                break;
        }
        
        wscrl(win, 1);
        box(win, 0, 0);
        wrefresh(win);
}

/* clear and redraw window */
void win_reset (WINDOW * win) {
    werase(win);
    box(win, 0, 0);
    wrefresh(win);
}

/* random string generator */
void rand_string(unsigned char * srng, int len) {
    unsigned char char_array[84] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()_+-=,.<>/?;:";
    for (int i = 0; i < len; i++) {
        srng[i] = char_array[(rand() % 84)];
    }
}

/* recieve messages from other client */
void * recv_msg (void * data) {    
    struct print_data * m_data = data;
    symmetric_CTR ctr;
    unsigned char msg[MAX_LEN], enc_msg[MAX_LEN];
	unsigned long msg_len = sizeof(msg);

	ctr_start(find_cipher("aes"), m_data->aes_IV, m_data->aes_key, 32, 0, CTR_COUNTER_LITTLE_ENDIAN, &ctr);
	ctr_setiv(m_data->aes_IV, 32, &ctr);

    for (;;) {
        recvfrom (m_data->socket_fd, enc_msg, sizeof(enc_msg),
            MSG_WAITALL, NULL,
                NULL);

        ctr_decrypt(enc_msg, msg, sizeof(msg), &ctr);

        print_msg(m_data->output_win, msg, true);
        
        memset(&msg, 0, sizeof(msg));
        memset(&enc_msg, 0, sizeof(enc_msg));
    }
}