CFLAGS 	:= -g3 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
CC 		:= gcc
LDFLAGS := -lpcap -lpthread -lffcall
LIB_FILE:= device.c packetio.c ip.c routing_table.c link_state.c arp.c
# TODO separate socket.c and other lib files

all: build/a build/tcp_daemon

build/a: a.c $(LIB_FILE) socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/b: b.c $(LIB_FILE) socket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tcp_daemon: tcp_daemon.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/router: router.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)
