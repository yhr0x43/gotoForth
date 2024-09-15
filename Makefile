CC ?= gcc
CFLAGS ?= -std=c2x -Wall -Wextra -pedantic -ggdb

.PHONY: all

all: gotoforth

gotoforth: main.c Makefile
	$(CC) $(CFLAGS) -o $@ $<

forth.s: main.c Makefile
	$(CC) $(CFLAGS) -S -o $@ $<
