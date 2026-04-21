#ifndef IO_H
#define IO_H

#include "fs.h"

int my_write(int fd, int pos);
int do_write(int fd, unsigned char *text, int len, char op);
int my_read(int fd, int pos);
int do_read(int fd, unsigned char *text, int len);

#endif
