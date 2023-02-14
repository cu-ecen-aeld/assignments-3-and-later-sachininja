# Makefile for cross compilation and native compilation 
# Author : Sachin Mathad


all: writer

CC=gcc

writer: writer.c
	@($(CROSS_COMPILE)$(CC) -o writer writer.c -Wall -Werror)

clean:
	@rm -f writer
	@echo Clean