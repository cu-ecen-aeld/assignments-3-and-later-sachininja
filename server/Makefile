# Makefile for Assignment 6
CC = gcc
CROSS_COMPILE =
CFLAGS := -g -Wall -Werror -pthread
LDFLAGS := -pthread -lrt
ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror -pthread
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

SRC := aesdsocket.c
TARGET = aesdsocket
TARGET ?= aesdsocket

all: $(TARGET)

$(TARGET) : $(SRC)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map