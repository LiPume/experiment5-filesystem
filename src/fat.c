#include "fat.h"

int getFreeFatid(void) {
    /* TODO: 返回一个空闲盘块号 */
    return -1;
}

void fatFree(unsigned short id) {
    /* TODO: 释放FAT链 */
    (void)id;
}

unsigned short getNextFat(unsigned short id) {
    /* TODO: 获取下一块盘块号 */
    return id;
}
