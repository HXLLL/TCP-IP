CFLAGS 	:= -g3
CC 		:= gcc
LDFLAGS := -lpcap -lpthread

all: build/main build/a build/b

build/main: main.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/a: a.c device.c packetio.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/b: b.c device.c packetio.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)
