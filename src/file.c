#include "file.h"
#include "fat.h"
#include <stdio.h>
#include <string.h>

/* ===================== 内部辅助函数 ===================== */

/* 目录中查找文件 */
static int find_file(unsigned short dir_first, unsigned long length,
                     const char *name, fcb *out, int *index) {

    int total = length / sizeof(fcb);
    fcb temp;
    char fullname[16];

    for (int i = 0; i < total; i++) {
        memcpy(&temp, blockaddr[dir_first] + i * sizeof(fcb), sizeof(fcb));

        if (temp.free == 1) continue;

        if (temp.exname[0] != '\0')
            sprintf(fullname, "%s.%s", temp.filename, temp.exname);
        else
            sprintf(fullname, "%s", temp.filename);

        if (strcmp(fullname, name) == 0) {
            if (out) memcpy(out, &temp, sizeof(fcb));
            if (index) *index = i;
            return 0;
        }
    }
    return -1;
}

/* ===================== 1. 创建文件 ===================== */

int my_create(char *filename) {
    useropen *cur = &openfilelist[curdirid];
    fcb newfile;
    int blk;
    int index;

    if (find_file(cur->first, cur->length, filename, NULL, NULL) == 0) {
        printf("文件已存在\n");
        return -1;
    }

    blk = allocBlock();
    if (blk < 0) {
        printf("磁盘已满\n");
        return -1;
    }

    fcb_init(&newfile, filename, blk, 1);

    /* 先简单按目录尾追加 */
    index = cur->length / sizeof(fcb);
    memcpy(blockaddr[cur->first] + index * sizeof(fcb), &newfile, sizeof(fcb));

    /* 更新当前目录长度 */
    cur->length += sizeof(fcb);
    cur->open_fcb.length = cur->length;
    cur->fcbstate = 1;

    /* 把当前目录自己的最新 FCB 同步到 '.' 目录项 */
    {
        fcb self_fcb = cur->open_fcb;
        strcpy(self_fcb.filename, ".");
        memcpy(blockaddr[cur->first], &self_fcb, sizeof(fcb));

        /* 根目录的 '..' 也指向自己，顺手同步 */
        if (cur->first == 5) {
            fcb parent_fcb = cur->open_fcb;
            strcpy(parent_fcb.filename, "..");
            memcpy(blockaddr[cur->first] + sizeof(fcb), &parent_fcb, sizeof(fcb));
        }
    }

    printf("创建成功\n");
    return my_open(filename);  /* 创建后直接打开 */
}
/* ===================== 2. 删除文件 ===================== */

void my_rm(char *filename) {
    useropen *cur = &openfilelist[curdirid];
    fcb target;
    int index;

    if (find_file(cur->first, cur->open_fcb.length, filename, &target, &index) != 0) {
        printf("文件不存在\n");
        return;
    }

    fatFree(target.first);

    fcb empty;
    memset(&empty, 0, sizeof(fcb));
    empty.free = 1;

    memcpy(blockaddr[cur->first] + index * sizeof(fcb), &empty, sizeof(fcb));

    cur->fcbstate = 1;

    printf("删除成功\n");
}

/* ===================== 3. 打开文件 ===================== */
int my_open(char *filename) {
    useropen *cur = &openfilelist[curdirid];
    fcb target;
    int index;

    if (find_file(cur->first, cur->open_fcb.length, filename, &target, &index) != 0) {
        printf("文件不存在\n");
        return -1;
    }

    int fd = getFreeOpenlist();
    if (fd < 0) {
        printf("打开文件表已满\n");
        return -1;
    }

    useropen_init(&openfilelist[fd], target.first, cur->first, cur->dir);

    memcpy(&openfilelist[fd].open_fcb, &target, sizeof(fcb));
    openfilelist[fd].length = target.length;
    openfilelist[fd].attribute = target.attribute;
    openfilelist[fd].diroff = index * sizeof(fcb);

    printf("打开成功\n");
    return fd;
}

/* ===================== 4. 关闭文件 ===================== */

void my_close(int fd) {
    if (!check_fd(fd)) {
        printf("fd非法\n");
        return;
    }

    useropen *file = &openfilelist[fd];

    if (file->fcbstate == 1) {
        file->open_fcb.length = file->length;

        memcpy(blockaddr[file->dirno] + file->diroff,
               &file->open_fcb, sizeof(fcb));
    }

    file->topenfile = 0;

    printf("关闭成功\n");
}