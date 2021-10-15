CFLAGS 	:= -g3
CC 		:= gcc
LDFLAGS := -lpcap

all: main

main: main.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)