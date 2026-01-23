[file name]: Makefile
[file content begin]
# SwiftFlow Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
LDFLAGS = -lm -lpcre -ltommath -lreadline
TARGET = swift
SOURCES = main.c lexer.c parser.c ast.c interpreter.c jsonlib.c \
          mathlib.c regexlib.c repl.c swf.c llvm_backend.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

test: $(TARGET)
	./$(TARGET) test.swf

.PHONY: all clean install uninstall test
[file content end]
