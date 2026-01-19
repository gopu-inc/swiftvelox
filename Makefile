# SwiftFlow Compiler - Makefile
# GoPU.inc © 2026

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -I. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lpthread

# Liste des fichiers sources
SOURCES = lexer.c parser.c swf.c snippet.c str.c types.c async.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = swift

# Cibles principales
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Règles de compilation
%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Installation système
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo mkdir -p /usr/local/lib/swiftflow
	sudo cp -r lib/* /usr/local/lib/swiftflow/
	@echo "SwiftFlow installé avec succès!"

# Nettoyage
clean:
	rm -f $(OBJECTS) $(TARGET)

# Tests
test: $(TARGET)
	./$(TARGET) tests/test_basic.sfl

test-async: $(TARGET)
	./$(TARGET) tests/test_async.sfl

# Mode REPL
repl: $(TARGET)
	./$(TARGET)

# Développement
dev: CFLAGS += -DDEBUG -O0
dev: $(TARGET)

# Production
release: CFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

.PHONY: all clean test repl install dev release
