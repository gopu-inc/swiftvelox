# SwiftFlow Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm

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
       $(SRC_DIR)/backend/llvm_back.c \
       $(SRC_DIR)/backend/nasm_back.c \
       $(SRC_DIR)/runtime/swf.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Binary
TARGET = $(BIN_DIR)/swiftflow

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR)/frontend
	@mkdir -p $(OBJ_DIR)/backend
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

# Run tests
test: all
	@echo "Running tests..."
	@./$(BIN_DIR)/swiftflow examples/hello.swf

# Install
install: all
	cp $(TARGET) /usr/local/bin/swiftflow
	@echo "SwiftFlow installed to /usr/local/bin"

# Phony targets
.PHONY: all clean test install directories

# Dependencies
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/frontend/lexer.o: $(SRC_DIR)/frontend/lexer.c $(INCLUDE_DIR)/lexer.h
$(OBJ_DIR)/frontend/parser.o: $(SRC_DIR)/frontend/parser.c $(INCLUDE_DIR)/parser.h
$(OBJ_DIR)/frontend/ast.o: $(SRC_DIR)/frontend/ast.c $(INCLUDE_DIR)/ast.h
$(OBJ_DIR)/backend/llvm_back.o: $(SRC_DIR)/backend/llvm_back.c $(INCLUDE_DIR)/backend.h
$(OBJ_DIR)/backend/nasm_back.o: $(SRC_DIR)/backend/nasm_back.c $(INCLUDE_DIR)/backend.h
$(OBJ_DIR)/runtime/swf.o: $(SRC_DIR)/runtime/swf.c
