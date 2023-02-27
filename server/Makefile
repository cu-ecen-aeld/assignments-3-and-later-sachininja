# Makefile for cross compilation and native compilation 
# Author : Sachin Mathad


all: aesdsocket

CC=gcc

aesdsocket: aesdsocket.c
	@($(CROSS_COMPILE)$(CC) -o aesdsocket aesdsocket.c -Wall -Werror)

clean:
	@rm -f aesdsocket
