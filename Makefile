# SwiftFlow Interpreter Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm -lbsd  # Ajouter -lbsd pour dirname

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
       $(SRC_DIR)/interpreter/interpreter.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Binary
TARGET = $(BIN_DIR)/swiftflow
# Dans votre Makefile, ajoutez :
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
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Test
test: all
	@echo "Testing..."
	@./$(BIN_DIR)/swiftflow

# Install
install: all
	cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin"

# Phony targets
.PHONY: all clean test install directories
