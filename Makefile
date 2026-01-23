# SwiftFlow Interpreter Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm  # Ajouter -lbsd pour dirname si nécessaire

# Détection du système d'exploitation
ifeq ($(OS),Windows_NT)
    CFLAGS += -D_WIN32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CFLAGS += -D_POSIX_C_SOURCE=199309L
        LDLIBS += -lrt
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS += -D_DARWIN_C_SOURCE
    endif
endif

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/frontend/lexer.c \
       $(SRC_DIR)/frontend/parser.c \
       $(SRC_DIR)/frontend/ast.c \
       $(SRC_DIR)/runtime/swf.c \
       $(SRC_DIR)/runtime/jsonlib.c \
       $(SRC_DIR)/interpreter/interpreter.c

# Object files
OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/frontend/lexer.o \
       $(OBJ_DIR)/frontend/parser.o \
       $(OBJ_DIR)/frontend/ast.o \
       $(OBJ_DIR)/runtime/swf.o \
       $(OBJ_DIR)/runtime/jsonlib.o \
       $(OBJ_DIR)/interpreter/interpreter.o

# Binary
TARGET = $(BIN_DIR)/swift

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR)/frontend
	@mkdir -p $(OBJ_DIR)/interpreter
	@mkdir -p $(OBJ_DIR)/runtime
	@mkdir -p $(BIN_DIR)

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# Règle spécifique pour jsonlib.o
obj/runtime/jsonlib.o: src/runtime/jsonlib.c include/jsonlib.h
	$(CC) $(CFLAGS) -c $< -o $@

# Règles génériques pour les fichiers objets
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/frontend/%.o: $(SRC_DIR)/frontend/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/runtime/%.o: $(SRC_DIR)/runtime/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/interpreter/%.o: $(SRC_DIR)/interpreter/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Test
test: all
	@echo "Testing..."
	@./$(BIN_DIR)/swift

# Install
install: all
	cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin"

# Phony targets
.PHONY: all clean test install directories
