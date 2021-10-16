CFLAGS 	:= -g3
CC 		:= gcc
LDFLAGS := -lpcap -lpthread -lffcall
LIB_FILE:=device.c packetio.c ip.c routing_table.c

all: build/main build/a build/b

build/main: main.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/a: a.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/b: b.c $(LIB_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)
