#include "io.h"
#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* * 内部辅助函数：将文件的逻辑偏移(offset)映射为物理盘块号
 * 核心逻辑：利用队友提供的 getNextFat 沿着 FAT 链向后找
 */
static int get_actual_block(int first_block, int offset, int *block_offset) {
    int cur_block = first_block;
    int target_index = offset / BLOCKSIZE; // 需要跳过的块数
    *block_offset = offset % BLOCKSIZE;    // 块内偏移

    for (int i = 0; i < target_index; i++) {
        if (cur_block == END || cur_block == FREE) {
            return END;
        }
        cur_block = getNextFat(cur_block);
    }
    return cur_block;
}

// ==================== 读文件模块 ====================

int my_read(int fd, int pos) {
    if (!check_fd(fd)) {
        printf("read: 错误，无效的文件描述符。\n");
        return -1;
    }

    if (pos >= 0) {
        openfilelist[fd].count = pos;
    }

    int len;
    printf("请输入读取长度: ");
    if (scanf("%d", &len) != 1) return -1;

    // 申请临时缓冲区
    unsigned char *text = (unsigned char *)malloc(len + 1);
    if (!text) return -1;

    int real_read = do_read(fd, text, len);
    if (real_read > 0) {
        text[real_read] = '\0';
        printf("--- 读取内容 ---\n%s\n----------------\n", text);
    } else {
        printf("未读取到任何内容。\n");
    }

    free(text);
    return real_read;
}

int do_read(int fd, unsigned char *text, int len) {
    useropen *ptr = &openfilelist[fd];
    int read_count = 0;

    // 边界检查，防止越界读
    if ((unsigned long)(ptr->count + len) > ptr->open_fcb.length) {
        len = (int)(ptr->open_fcb.length - ptr->count);
    }

    while (len > 0) {
        int off;
        int cur_b = get_actual_block(ptr->first, ptr->count, &off);
        if (cur_b == END) break;

        int can_read = BLOCKSIZE - off;
        int chunk = (len < can_read) ? len : can_read;

        memcpy(text + read_count, blockaddr[cur_b] + off, chunk);

        ptr->count += chunk;
        read_count += chunk;
        len -= chunk;
    }
    return read_count;
}

// ==================== 写文件模块 ====================

int my_write(int fd, int pos) {
    if (!check_fd(fd)) {
        printf("write: 错误，无效的文件描述符。\n");
        return -1;
    }

    int op;
    printf("请选择写方式 (1:截断写, 2:覆盖写, 3:追加写): ");
    if (scanf("%d", &op) != 1) return -1;
    getchar(); // 吸收回车，防止被接下来的 fgets 读走

    if (op == 1) { 
        // 截断写：利用队友的 fatFree 释放除第一块外的所有后续块
        unsigned short next = getNextFat(openfilelist[fd].first);
        while (next != (unsigned short)END) {
            unsigned short temp = getNextFat(next);
            fatFree(next);
            next = temp;
        }
        fat1[openfilelist[fd].first].id = END; // 保持首块
        fat2[openfilelist[fd].first].id = END;
        openfilelist[fd].open_fcb.length = 0;
        openfilelist[fd].count = 0;
    } else if (op == 3) {
        // 追加写：指针移动到文件末尾
        openfilelist[fd].count = (int)openfilelist[fd].open_fcb.length;
    } else if (pos >= 0) {
        openfilelist[fd].count = pos;
    }

    unsigned char buffer[SIZE];
    printf("请输入内容 (以回车结束):\n");
    if (fgets((char *)buffer, SIZE, stdin) == NULL) return -1;
    
    int len = strlen((char *)buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[--len] = '\0';

    return do_write(fd, buffer, len, (char)op);
}

int do_write(int fd, unsigned char *text, int len, char op) {
    useropen *ptr = &openfilelist[fd];
    int write_count = 0;
    (void)op; // 消除变量未使用警告

    while (len > 0) {
        int off;
        int cur_b = get_actual_block(ptr->first, ptr->count, &off);

        // 如果需要新块
        if (cur_b == END) {
            int new_b = getFreeFatid(); // 调用队友函数
            if (new_b == -1) {
                printf("错误：磁盘空间已满。\n");
                break;
            }
            
            // 找到链表当前末尾并挂载
            int last = ptr->first;
            while (getNextFat(last) != (unsigned short)END) {
                last = getNextFat(last);
            }
            fat1[last].id = (unsigned short)new_b;
            fat2[last].id = (unsigned short)new_b;
            
            // 初始化新块
            cur_b = new_b;
            off = 0;
            memset(blockaddr[cur_b], 0, BLOCKSIZE);
        }

        int can_write = BLOCKSIZE - off;
        int chunk = (len < can_write) ? len : can_write;

        memcpy(blockaddr[cur_b] + off, text + write_count, chunk);

        ptr->count += chunk;
        write_count += chunk;
        len -= chunk;

        // 更新文件实际长度
        if ((unsigned long)ptr->count > ptr->open_fcb.length) {
            ptr->open_fcb.length = (unsigned long)ptr->count;
        }
    }

    ptr->fcbstate = 1; // 标记修改，提醒系统写回磁盘
    return write_count;
}