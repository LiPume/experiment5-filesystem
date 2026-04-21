#ifndef FAT_H
#define FAT_H

#include "fs.h"

int getFreeFatid(void);
void fatFree(unsigned short id);
unsigned short getNextFat(unsigned short id);

#endif
