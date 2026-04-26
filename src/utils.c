#include "fs.h"

/*
 * 初始化 FCB
 * attribute: 0 表示目录, 1 表示普通文件
 * free: 0 表示有效项, 1 表示空/已删除
 */
void fcb_init(fcb *file, const char *name, unsigned short first, unsigned char attribute) {
    const char *dot = NULL;
    size_t len = 0;

    if (file == NULL || name == NULL) {
        return;
    }

    memset(file, 0, sizeof(fcb));

    file->attribute = attribute;
    file->first = first;
    file->free = 0;

    if (attribute == 0) {
        /* 目录：不考虑扩展名 */
        strncpy(file->filename, name, sizeof(file->filename) - 1);
        file->length = 2 * sizeof(fcb);   /* 默认预留 "." 和 ".." */
        return;
    }

    /* 普通文件：尝试分离文件名和扩展名 */
    dot = strrchr(name, '.');
    if (dot != NULL) {
        len = (size_t)(dot - name);
        if (len >= sizeof(file->filename)) {
            len = sizeof(file->filename) - 1;
        }
        memcpy(file->filename, name, len);
        strncpy(file->exname, dot + 1, sizeof(file->exname) - 1);
        file->exname[sizeof(file->exname) - 1] = '\0';
    } else {
        strncpy(file->filename, name, sizeof(file->filename) - 1);
    }

    file->length = 0;
}

/*
 * 初始化用户打开文件表项
 */
void useropen_init(useropen *item, unsigned short first, int dirno, const char *dir) {
    if (item == NULL) {
        return;
    }

    memset(item, 0, sizeof(useropen));
    item->first = first;
    item->dirno = dirno;
    item->diroff = 0;
    item->count = 0;
    item->fcbstate = 0;
    item->topenfile = 1;

    if (dir != NULL) {
        strncpy(item->dir, dir, DIRLEN - 1);
    }
}

/*
 * 检查 fd 是否有效且已打开
 */
int check_fd(int fd) {
    if (fd < 0 || fd >= MAXOPENFILE) {
        return 0;
    }
    if (openfilelist[fd].topenfile == 0) {
        return 0;
    }
    return 1;
}

/*
 * 找一个空闲的打开文件表项
 * 0 一般留给根目录当前目录项，所以从 1 开始找
 */
int getFreeOpenlist(void) {
    int i;
    for (i = 1; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile == 0) {
            return i;
        }
    }
    return -1;
}