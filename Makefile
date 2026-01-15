CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = swiftvelox

.PHONY: all clean test

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

clean:
	rm -f $(TARGET) *.o *.out *.c examples/*.out examples/*.c

test: $(TARGET)
	./$(TARGET) run examples/test.svx
	./$(TARGET) run examples/calc.svx

quick:
	gcc -O2 -o swiftvelox src/main.c
