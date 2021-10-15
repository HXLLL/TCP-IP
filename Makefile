CFLAGS 	:= -g3
CC 		:= gcc
LDFLAGS := -lpcap

all: build/main build/a

build/main: main.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/a: a.c device.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean: 
	rm -f ./build/*

$(shell mkdir -p build)
