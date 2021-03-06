CFLAGS 	:= -g3 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
CC 		:= gcc
LDFLAGS := -lpcap -lpthread -lffcall
LIB_FILE:= device.c packetio.c ip.c routing_table.c link_state.c arp.c
# TODO separate socket.c and other lib files
HIJACK_ARG := -Wl,--wrap,socket,--wrap,connect,--wrap,bind,--wrap,listen,--wrap,accept,--wrap,read,--wrap,write,--wrap,close

all: build/a build/b build/tcp_daemon build/echo_client build/echo_server

build/echo_client: checkpoints/unp.c checkpoints/echo_client.c socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(HIJACK_ARG)

build/echo_server: checkpoints/unp.c checkpoints/echo_server.c socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(HIJACK_ARG)

build/a: a.c $(LIB_FILE) socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(HIJACK_ARG)

build/b: b.c $(LIB_FILE) socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(HIJACK_ARG)

build/tcp_daemon: tcp_daemon.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/router: router.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)
