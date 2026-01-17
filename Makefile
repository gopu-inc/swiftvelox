CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I./src -D_POSIX_C_SOURCE=200809L
LIBS = -lm -lpthread -lcurl -lsqlite3
SRC_DIR = src
OBJ_DIR = obj

# Liste tous les fichiers source
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/value.c \
       $(SRC_DIR)/interpreter.c \
       $(SRC_DIR)/native.c \
       $(SRC_DIR)/parser.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TARGET = swft

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
