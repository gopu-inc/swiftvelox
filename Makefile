# Makefile for SwiftFlow
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
LDFLAGS = -lm

SRCS = lexer.c parser.c swf.c
OBJS = $(SRCS:.c=.o)
TARGET = swiftflow

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	./$(TARGET) test.swf

repl: $(TARGET)
	./$(TARGET)
