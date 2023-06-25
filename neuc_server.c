#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 5544

int main () {
    printf("starting...\n");
    
    /* initialize variables */
    uint32_t * last_addr;
    uint16_t * last_port;
    char buf[sizeof(*last_addr) + sizeof(*last_port)];
    memset(buf, 0, sizeof(buf));

    last_addr   = (uint32_t *) (buf          );
    last_port   = (uint16_t *) (last_addr + 1);

    /* open socket */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        printf("socket not avaliable");
        return -1;
    }

    /* bind socket */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    
    int bind_ret = bind(socket_fd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (bind_ret < 0) {
        printf("error on bind. check to see if port is avaliable");
        return -1;
    }

    /* initialize client variables */
    struct sockaddr_in client_a, client_b;
    socklen_t client_a_len;
    socklen_t client_b_len;
    client_a_len = sizeof(struct sockaddr_in);
    client_b_len = sizeof(struct sockaddr_in);

    char client_connection_key_a[256];
    char client_connection_key_b[256];

    printf("ready\n");

    for(;;) {
        /* reset client information */
        memset(&client_a, 0, sizeof(client_a));
        memset(&client_b, 0, sizeof(client_a));

        memset(&buf, 0, sizeof(buf));
        
        memset(&client_connection_key_a, 0, sizeof(client_connection_key_a));
        memset(&client_connection_key_b, 0, sizeof(client_connection_key_b));

        /* receive packets*/
        recvfrom(socket_fd, (char *) client_connection_key_a, 4097, 
            MSG_WAITALL, (struct sockaddr *) &client_a, 
                &client_a_len);

        /* fill client information */
        *last_addr = client_a.sin_addr.s_addr;
        *last_port = client_a.sin_port;

        /* wait for a packet that matches. will loop for 10 seconds */
        for(int iter = 0; iter < 1000000; iter++) {
            /* receive potential matching packet */
            recvfrom(socket_fd, (char *) client_connection_key_b, 4097,
                MSG_DONTWAIT, (struct sockaddr *) &client_b,
                    &client_b_len);

            /* compare their connection keys */
            if (strcmp(client_connection_key_a, client_connection_key_b) == 0) {
                /* if it matches send the client information */
                sendto(socket_fd, buf, sizeof(buf),
                    0, (const struct sockaddr *) &client_b, 
                        client_b_len);

                *last_addr = client_b.sin_addr.s_addr;
                *last_port = client_b.sin_port;

                sendto(socket_fd, buf, sizeof(buf),
                    0, (const struct sockaddr *) &client_a, 
                        client_a_len);

                /* send roles for the client that will make the AES key and the one who will make the RSA key */
                sendto(socket_fd, "a", 1,
                    0, (const struct sockaddr *) &client_a, 
                        client_a_len);

                sendto(socket_fd, "b", 1,
                    0, (const struct sockaddr *) &client_b, 
                        client_b_len);

                goto end;
            }
            sleep(0.00001);
        }
        end:
    }
    return 0;
}