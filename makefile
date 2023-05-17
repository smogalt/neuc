all: client server

client: client.c
	gcc -c draw_funcs.c -lncurses
	gcc -c msg_funcs.c -lncurses
	gcc -DTFM_DESC -DUSE_TFM -c client.c -lncurses -ltomcrypt -ltfm

	gcc -DTFM_DESC -DUSE_TFM draw_funcs.o msg_funcs.o client.o -o client -lncurses -ltomcrypt -ltfm
	
	rm msg_funcs.o
	rm draw_funcs.o

server: server.c
	gcc server.c -o server -lncurses
