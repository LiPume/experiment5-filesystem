#include "io.h"
#include "fat.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== 内部辅助函数 ===================== */

/**
 * 逻辑偏移转物理块号
 * 核心逻辑：沿着队友维护的 FAT 链表跳转
 */
static int get_actual_block(int first_block, int offset, int *block_offset) {
    int cur_block = first_block;
    int target_index = offset / BLOCKSIZE; 
    *block_offset = offset % BLOCKSIZE;

    for (int i = 0; i < target_index; i++) {
        // 调用队友的 getNextFat 获取下一跳
        int next = getNextFat(cur_block);
        if (next == END || next == FREE) {
            return END;
        }
        
        cur_block = next;
    }
    return cur_block;
}

/* ===================== 1. 读文件模块 ===================== */

int my_read(int fd, int pos) {
    if (!check_fd(fd)) {
        printf("read: 错误，无效的文件描述符。\n");
        return -1;
    }

    if (pos >= 0) {
        openfilelist[fd].count = pos;
    }

    int len;
    printf("请输入欲读取的字节数: ");
    if (scanf("%d", &len) != 1) return -1;

    unsigned char *text = (unsigned char *)malloc(len + 1);
    if (!text) return -1;

    int real_read = do_read(fd, text, len);
    if (real_read > 0) {
        text[real_read] = '\0';
        printf("--- 文件内容 ---\n%s\n----------------\n", text);
    } else {
        printf("读取结束或无内容。\n");
    }

    free(text);
    return real_read;
}

int do_read(int fd, unsigned char *text, int len) {
    useropen *ptr = &openfilelist[fd];
    int read_count = 0;

    // 适配：使用队友在 my_open 中同步的 length
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

/* ===================== 2. 写文件模块 ===================== */

int my_write(int fd, int pos) {
    if (!check_fd(fd)) {
        printf("write: 错误，无效的文件描述符。\n");
        return -1;
    }

    int op;
    printf("请选择写方式 (1:截断写, 2:覆盖写, 3:追加写): ");
    if (scanf("%d", &op) != 1) return -1;
    getchar(); // 吸收回车

    if (op == 1) { 
        // 截断写：保留首块，释放后续块
        int next = getNextFat(openfilelist[fd].first);
        if (next != END) {
            fatFree(next); // 调用队友的链式释放函数
            setFat(openfilelist[fd].first, END); // 重新封口
        }
        openfilelist[fd].open_fcb.length = 0;
        openfilelist[fd].length = 0;
        openfilelist[fd].count = 0;
    } else if (op == 3) {
        openfilelist[fd].count = (int)openfilelist[fd].open_fcb.length;
    } else if (pos >= 0) {
        openfilelist[fd].count = pos;
    }

    unsigned char buffer[SIZE];
    printf("请输入待写入内容 (直接回车结束):\n");
    if (fgets((char *)buffer, SIZE, stdin) == NULL) return -1;
    
    int len = strlen((char *)buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[--len] = '\0';

    return do_write(fd, buffer, len, (char)op);
}

int do_write(int fd, unsigned char *text, int len, char op) {
    useropen *ptr = &openfilelist[fd];
    int write_count = 0;
    (void)op;

    while (len > 0) {
        int off;
        int cur_b = get_actual_block(ptr->first, ptr->count, &off);

        // 扩容逻辑
        if (cur_b == END) {
            // 调用队友的 allocBlock，它会自动找空位并标记 END
            int new_b = allocBlock(); 
            if (new_b == -1) {
                printf("[IO] 错误：磁盘空间不足。\n");
                break;
            }
            
            // 查找到当前文件的链表末尾，并挂载新块
            int last = ptr->first;
            while (getNextFat(last) != END) {
                last = getNextFat(last);
            }
            setFat(last, new_b); // 关联新块

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

        // 同步长度：确保 my_close 写回磁盘时大小正确
        if ((unsigned long)ptr->count > ptr->open_fcb.length) {
            ptr->open_fcb.length = (unsigned long)ptr->count;
            ptr->length = ptr->open_fcb.length; 
        }
    }

    ptr->fcbstate = 1; // 激活修改标志
    return write_count;
}