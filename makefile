INSTALL_PATH?=/usr/local

all: client server

install: client server
	install neuc_client INSTALL_PATH/bin
	install neuc_server INSTALL_PATH/bin

client: neuc_client.c
	gcc -DTFM_DESC -DUSE_TFM neuc_client.c -o neuc_client -lncurses -ltomcrypt -ltfm

server: neuc_server.c
	gcc neuc_server.c -o neuc_server -lncurses