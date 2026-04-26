#include <stdio.h>
#include "fat.h"
#include "fs.h"

// ============================
// 获取 FAT 表指针
// ============================
static unsigned short *getFat1() {
    return (unsigned short *)(myvhard + BLOCKSIZE * 1);
}

static unsigned short *getFat2() {
    return (unsigned short *)(myvhard + BLOCKSIZE * 3);
}

// ============================
// 1. 查找空闲块
// ============================
int getFreeFatid() {
    unsigned short *fat1 = getFat1();

    // 从数据区开始找（前面是系统区）
    for (int i = 6; i < SIZE / BLOCKSIZE; i++) {
        if (fat1[i] == FREE) {
	    printf("[FAT] find tree block: %d\n", i);
            return i;
        }
    }
    return -1;  // 没有空闲块
}

// ============================
// 2. 设置 FAT 表项（同步 FAT1 和 FAT2）
// ============================
void setFat(int id, int value) {
    unsigned short *fat1 = getFat1();
    unsigned short *fat2 = getFat2();

    fat1[id] = value;
    fat2[id] = value;
    printf("[FAT] set fat[%d] = %d\n", id, value);
}

// ============================
// 3. 获取下一个块号
// ============================
int getNextFat(int id) {
    unsigned short *fat1 = getFat1();
    return fat1[id];
}

// ============================
// 4. 分配一个新块（用于创建或写文件）
// ============================
int allocBlock() {
    int id = getFreeFatid();
    if (id == -1) return -1;

    setFat(id, END);  // 新块默认结束
    return id;
}

// ============================
// 5. 释放整个 FAT 链（删除文件用）
// ============================
void fatFree(int first) {
    unsigned short *fat1 = getFat1();
    int cur = first;
    int next;

    while (cur != END && cur != FREE) {
        next = fat1[cur];
        setFat(cur, FREE);
        printf("[FAT] free block: %d\n", cur);
        cur = next;
    }
}

// ============================
// 6. 调试函数（可选）
// ============================
void printFatChain(int first) {
    unsigned short *fat1 = getFat1();

    int cur = first;
    printf("FAT chain: ");

    while (cur != END) {
        printf("%d -> ", cur);
        cur = fat1[cur];
    }
    printf("END\n");
}
