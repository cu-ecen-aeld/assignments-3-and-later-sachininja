# Makefile for cross compilation and native compilation 
# Author : Sachin Mathad

cc=gcc

writer: writer.c
	@($(CROSS_COMPILE)$(cc) -o writer writer.c -Wall -Werror)

clean:
	@rm -f writer
	@echo Clean