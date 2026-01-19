CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -I.
LDFLAGS = -lm

SOURCES = lexer.c parser.c swf.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = swiftflow

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	./$(TARGET) test.sf

repl: $(TARGET)
	./$(TARGET)

.PHONY: all clean test repl
