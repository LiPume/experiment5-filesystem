#ifndef FILE_H
#define FILE_H

#include "fs.h"

int my_create(char *filename);
void my_rm(char *filename);
int my_open(char *filename);
void my_close(int fd);

#endif
