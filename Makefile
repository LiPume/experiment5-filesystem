CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
SRC = src/main.c src/fs.c src/fat.c src/dir.c src/file.c src/io.c src/utils.c
TARGET = main

all:
	gcc $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o myfsys
