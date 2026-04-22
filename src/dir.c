#include "dir.h"

/*
 * 这些辅助函数只服务目录模块，因此放在 dir.c 内部即可，
 * 不修改公共头文件，不暴露给其他模块。
 */

/* 判断一个目录项是否为有效项 */
static int is_valid_fcb(const fcb *entry) {
    if (entry == NULL) {
        return 0;
    }

    /* 显式空项/删除项 */
    if (entry->free == 1) {
        return 0;
    }

    /*
     * 兼容当前工程中目录块剩余区域被 memset 为 0 的情况：
     * 这些全 0 槽位虽然 free 不是 1，但也不能视为有效目录项
     */
    if (entry->filename[0] == '\0' &&
        entry->exname[0] == '\0' &&
        entry->first == 0 &&
        entry->length == 0) {
        return 0;
    }

    return 1;
}

/* 将 FCB 转成可显示名字 */
static void make_entry_name(const fcb *entry, char *buf, size_t buflen) {
    if (entry == NULL || buf == NULL || buflen == 0) {
        return;
    }

    memset(buf, 0, buflen);

    if (entry->attribute == 0 || entry->exname[0] == '\0') {
        snprintf(buf, buflen, "%s", entry->filename);
    } else {
        snprintf(buf, buflen, "%s.%s", entry->filename, entry->exname);
    }
}

/*
 * 从目录文件中按逻辑序号读取一个目录项
 * dir_first : 目录起始块号
 * index     : 该目录中的第 index 个 fcb（从 0 开始）
 */
static int read_dir_entry_by_index(unsigned short dir_first, int index, fcb *out) {
    int entries_per_block;
    int block_skip;
    int inner_index;
    unsigned short blk;

    if (out == NULL || index < 0 || dir_first >= BLOCKNUM || BLOCKSIZE <= 0) {
        return -1;
    }

    entries_per_block = BLOCKSIZE / (int)sizeof(fcb);
    if (entries_per_block <= 0) {
        return -1;
    }

    block_skip = index / entries_per_block;
    inner_index = index % entries_per_block;
    blk = dir_first;

    while (block_skip > 0) {
        if (blk >= BLOCKNUM || fat1[blk].id == FREE || fat1[blk].id == END) {
            return -1;
        }
        blk = fat1[blk].id;
        block_skip--;
    }

    if (blk >= BLOCKNUM) {
        return -1;
    }

    memcpy(out, blockaddr[blk] + inner_index * sizeof(fcb), sizeof(fcb));
    return 0;
}

/*
 * 在目录中查找指定名字的“子目录”
 * 这里只匹配目录 attribute == 0，避免 cd 进普通文件
 */
static int find_subdir_in_dir(unsigned short dir_first,
                              unsigned long dir_length,
                              const char *dirname,
                              fcb *out_entry,
                              int *out_off) {
    int total_entries;
    int i;
    fcb temp;
    char namebuf[16];

    if (dirname == NULL) {
        return -1;
    }

    total_entries = (int)(dir_length / sizeof(fcb));
    for (i = 0; i < total_entries; i++) {
        if (read_dir_entry_by_index(dir_first, i, &temp) != 0) {
            continue;
        }

        if (!is_valid_fcb(&temp)) {
            continue;
        }

        if (temp.attribute != 0) {
            continue;
        }

        make_entry_name(&temp, namebuf, sizeof(namebuf));
        if (strcmp(namebuf, dirname) == 0) {
            if (out_entry != NULL) {
                memcpy(out_entry, &temp, sizeof(fcb));
            }
            if (out_off != NULL) {
                *out_off = i * (int)sizeof(fcb);
            }
            return 0;
        }
    }

    return -1;
}

/* 根据当前路径和目标目录名，构造切换后的路径 */
static void build_next_dir_path(const char *current,
                                const char *dirname,
                                char *out,
                                size_t outlen) {
    size_t len;

    if (out == NULL || outlen == 0) {
        return;
    }

    memset(out, 0, outlen);

    if (current == NULL || dirname == NULL) {
        return;
    }

    if (strcmp(dirname, ".") == 0) {
        snprintf(out, outlen, "%s", current);
        return;
    }

    if (strcmp(dirname, "..") == 0) {
        snprintf(out, outlen, "%s", current);
        len = strlen(out);

        if (strcmp(out, "~/") == 0) {
            return;
        }

        /* 去掉最后的 '/' */
        if (len > 0 && out[len - 1] == '/') {
            out[len - 1] = '\0';
            len--;
        }

        /* 删除最后一级目录名 */
        while (len > 0 && out[len - 1] != '/') {
            out[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            snprintf(out, outlen, "~/");
        }
        return;
    }

    if (strcmp(current, "~/") == 0) {
        snprintf(out, outlen, "~/%s/", dirname);
    } else {
        snprintf(out, outlen, "%s%s/", current, dirname);
    }
}

void my_ls(void) {
    useropen *curdir;
    int total_entries;
    int i;
    int has_output = 0;
    fcb entry;
    char namebuf[16];

    if (!check_fd(curdirid)) {
        printf("当前目录状态非法。\n");
        return;
    }

    curdir = &openfilelist[curdirid];

    total_entries = (int)(curdir->length / sizeof(fcb));

    printf("当前目录: %s\n", curdir->dir);

    for (i = 0; i < total_entries; i++) {
        if (read_dir_entry_by_index(curdir->first, i, &entry) != 0) {
            continue;
        }

        if (!is_valid_fcb(&entry)) {
            continue;
        }

        make_entry_name(&entry, namebuf, sizeof(namebuf));
        printf("[%s] %-12s first=%u length=%lu\n",
               entry.attribute == 0 ? "DIR " : "FILE",
               namebuf,
               entry.first,
               entry.length);
        has_output = 1;
    }

    if (!has_output) {
        printf("(empty)\n");
    }
}

void my_cd(char *dirname) {
    useropen *curdir;
    useropen olddir;
    fcb target;
    fcb parent_dotdot;
    int diroff = 0;
    char next_path[DIRLEN];

    if (dirname == NULL || dirname[0] == '\0') {
        printf("usage: cd <dirname>\n");
        return;
    }

    if (!check_fd(curdirid)) {
        printf("当前目录状态非法。\n");
        return;
    }

    curdir = &openfilelist[curdirid];
    memcpy(&olddir, curdir, sizeof(useropen));

    /* cd . 直接不动 */
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    /* 根目录下 cd ..，保持不动 */
    if (strcmp(dirname, "..") == 0 && strcmp(curdir->dir, "~/") == 0) {
        return;
    }

    /* 当前目录中查找目标目录，兼容 ".." */
    if (find_subdir_in_dir(olddir.first, olddir.length, dirname, &target, &diroff) != 0) {
        printf("目录不存在: %s\n", dirname);
        return;
    }

    build_next_dir_path(olddir.dir, dirname, next_path, sizeof(next_path));

    /*
     * 直接原地覆写当前目录打开项，避免影响其他尚未完成模块。
     * first 取目标目录起始块号
     * dirno 表示“当前目录的父目录起始块号”
     */
    useropen_init(curdir, target.first, olddir.first, next_path);

    strncpy(curdir->filename, target.filename, sizeof(curdir->filename) - 1);
    curdir->filename[sizeof(curdir->filename) - 1] = '\0';

    strncpy(curdir->exname, target.exname, sizeof(curdir->exname) - 1);
    curdir->exname[sizeof(curdir->exname) - 1] = '\0';

    curdir->attribute = target.attribute;
    curdir->time = target.time;
    curdir->date = target.date;
    curdir->first = target.first;
    curdir->length = target.length;
    curdir->diroff = diroff;
    curdir->count = 0;
    curdir->fcbstate = 0;
    curdir->topenfile = 1;
    memcpy(&curdir->open_fcb, &target, sizeof(fcb));

    /*
     * 处理父目录块号：
     * 1. 进入普通子目录时，新目录的父目录就是 olddir.first
     * 2. cd .. 时，结果目录的父目录应是“祖父目录”
     */
    if (strcmp(dirname, "..") == 0) {
        if (read_dir_entry_by_index(target.first, 1, &parent_dotdot) == 0 &&
            is_valid_fcb(&parent_dotdot)) {
            curdir->dirno = parent_dotdot.first;
        } else {
            /* 保底：如果读取失败，至少让它指向自己 */
            curdir->dirno = target.first;
        }
    } else {
        curdir->dirno = olddir.first;
    }
}

void my_mkdir(char *dirname) {
    /* TODO: 创建子目录 */
    (void)dirname;
}

void my_rmdir(char *dirname) {
    /* TODO: 删除空目录 */
    (void)dirname;
}
