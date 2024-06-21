IDIR = ./inc
CC = gcc
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
CURRENT_DATE := '"$(shell date +"%Y%m%dT%H%M%SZ")"'
CURRENT_NAME := '"rfefifo"'

rfefifo: rfefifo.c
	$(CC) rfefifo.c -o rfefifo -DCURRENT_HASH=$(CURRENT_HASH) \
	-DCURRENT_DATE=$(CURRENT_DATE) -DCURRENT_NAME=$(CURRENT_NAME) \
	-lftd2xx

clean:
	rm rfefifo
