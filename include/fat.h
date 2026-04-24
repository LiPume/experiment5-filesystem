#ifndef _FAT_H_
#define _FAT_H_

int getFreeFatid();
void setFat(int id, int value);
int getNextFat(int id);
int allocBlock();
void freeFatChain(int first);
void fatFree(int first);

// 可选调试
void printFatChain(int first);

#endif
