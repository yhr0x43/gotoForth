CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -ggdb

.PHONY: all

all: forth

forth: main.c Makefile
	$(CC) $(CFLAGS) -o $@ $<

forth.s: main.c Makefile
	$(CC) $(CFLAGS) -S -o $@ $<
