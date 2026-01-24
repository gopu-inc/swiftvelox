CC = gcc
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-format-truncation
LDFLAGS = -lm

SRCS = swf.c lexer.c parser.c io.c
OBJS = $(SRCS:.c=.o)
TARGET = swift

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c common.h io.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
