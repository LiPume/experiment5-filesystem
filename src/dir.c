#include "dir.h"
#include "fat.h"

/* 判断一个目录项是否为有效项 */
static int is_valid_fcb(const fcb *entry) {
    if (entry == NULL) {
        return 0;
    }

    if (entry->free == 1) {
        return 0;
    }

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

/* 从目录文件中按逻辑序号读取一个目录项 */
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

/* 在目录中查找指定名字的子目录 */
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

        if (len > 0 && out[len - 1] == '/') {
            out[len - 1] = '\0';
            len--;
        }

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

/* 目录项总数 = length / sizeof(fcb) */
static int get_dir_entry_count(unsigned long dir_length) {
    return (int)(dir_length / sizeof(fcb));
}

/* 每个盘块最多能放多少个 fcb */
static int get_entries_per_block(void) {
    if (BLOCKSIZE <= 0) {
        return 0;
    }
    return BLOCKSIZE / (int)sizeof(fcb);
}

/* 释放从 first 开始的一条 FAT 链 */
static void free_block_chain(unsigned short first) {
    unsigned short cur;
    unsigned short next;

    cur = first;

    while (cur != END && cur != FREE && cur < BLOCKNUM) {
        next = fat1[cur].id;

        fat1[cur].id = FREE;
        fat2[cur].id = FREE;
        memset(blockaddr[cur], 0, BLOCKSIZE);

        if (next == END || next == FREE || next >= BLOCKNUM) {
            break;
        }

        cur = next;
    }
}

/* 向目录文件中按逻辑序号写入一个目录项，allow_expand=1 时允许目录扩容 */
static int write_dir_entry_by_index(unsigned short dir_first,
                                    int index,
                                    const fcb *in,
                                    int allow_expand) {
    int entries_per_block;
    int block_skip;
    int inner_index;
    unsigned short blk;

    if (in == NULL || index < 0 || dir_first >= BLOCKNUM || BLOCKSIZE <= 0) {
        return -1;
    }

    entries_per_block = get_entries_per_block();
    if (entries_per_block <= 0) {
        return -1;
    }

    block_skip = index / entries_per_block;
    inner_index = index % entries_per_block;
    blk = dir_first;

    while (block_skip > 0) {
        if (blk >= BLOCKNUM || fat1[blk].id == FREE) {
            return -1;
        }

        if (fat1[blk].id == END) {
            int newblk;

            if (!allow_expand) {
                return -1;
            }

            newblk = allocBlock();
            if (newblk < 0) {
                return -1;
            }

            fat1[blk].id = (unsigned short)newblk;
            fat2[blk].id = (unsigned short)newblk;
            fat1[newblk].id = END;
            fat2[newblk].id = END;
        }

        blk = fat1[blk].id;
        block_skip--;
    }

    if (blk >= BLOCKNUM) {
        return -1;
    }

    memcpy(blockaddr[blk] + inner_index * (int)sizeof(fcb), in, sizeof(fcb));
    return 0;
}

/* 在目录中按名字查找任意目录项 */
static int find_entry_in_dir(unsigned short dir_first,
                             unsigned long dir_length,
                             const char *name,
                             fcb *out_entry,
                             int *out_index) {
    int total_entries;
    int i;
    fcb temp;
    char namebuf[16];

    if (name == NULL) {
        return -1;
    }

    total_entries = get_dir_entry_count(dir_length);

    for (i = 0; i < total_entries; i++) {
        if (read_dir_entry_by_index(dir_first, i, &temp) != 0) {
            continue;
        }

        if (!is_valid_fcb(&temp)) {
            continue;
        }

        make_entry_name(&temp, namebuf, sizeof(namebuf));

        if (strcmp(namebuf, name) == 0) {
            if (out_entry != NULL) {
                memcpy(out_entry, &temp, sizeof(fcb));
            }
            if (out_index != NULL) {
                *out_index = i;
            }
            return 0;
        }
    }

    return -1;
}

/* 找一个可写入的空槽位；如果没有空槽位，就返回末尾追加位置 */
static int find_free_slot_or_append(unsigned short dir_first,
                                    unsigned long dir_length,
                                    int *out_index,
                                    int *need_append) {
    int total_entries;
    int i;
    fcb temp;

    if (out_index == NULL || need_append == NULL) {
        return -1;
    }

    total_entries = get_dir_entry_count(dir_length);

    for (i = 0; i < total_entries; i++) {
        if (read_dir_entry_by_index(dir_first, i, &temp) != 0) {
            continue;
        }

        if (!is_valid_fcb(&temp)) {
            *out_index = i;
            *need_append = 0;
            return 0;
        }
    }

    *out_index = total_entries;
    *need_append = 1;
    return 0;
}

/* 判断目录是否为空：只允许存在 "." 和 ".." */
static int dir_is_empty(unsigned short dir_first, unsigned long dir_length) {
    int total_entries;
    int i;
    fcb temp;
    char namebuf[16];

    total_entries = get_dir_entry_count(dir_length);

    for (i = 0; i < total_entries; i++) {
        if (read_dir_entry_by_index(dir_first, i, &temp) != 0) {
            continue;
        }

        if (!is_valid_fcb(&temp)) {
            continue;
        }

        make_entry_name(&temp, namebuf, sizeof(namebuf));

        if (strcmp(namebuf, ".") == 0 || strcmp(namebuf, "..") == 0) {
            continue;
        }

        return 0;
    }

    return 1;
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

    total_entries = (int)(curdir->open_fcb.length / sizeof(fcb));

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
    fcb target_dot;
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

    /* cd . 不切换 */
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    /* 根目录下 cd .. 不切换 */
    if (strcmp(dirname, "..") == 0 && strcmp(curdir->dir, "~/") == 0) {
        return;
    }

    /* 在当前目录中查找目标目录，包括普通子目录和 .. */
    if (find_subdir_in_dir(olddir.first,
                           olddir.open_fcb.length,
                           dirname,
                           &target,
                           &diroff) != 0) {
        printf("目录不存在: %s\n", dirname);
        return;
    }

    build_next_dir_path(olddir.dir, dirname, next_path, sizeof(next_path));

    /*
     * 关键修复：
     * 不管是 cd 子目录，还是 cd ..，
     * 都不要直接信任当前目录项 target.length。
     *
     * 因为父目录中保存的子目录 FCB 可能是旧的，
     * 例如 testdir 中创建 inner 后，
     * 根目录里的 testdir.length 可能仍然是 80。
     *
     * 所以这里统一读取目标目录自己的 "." 项，
     * 用 "." 项中的 length 作为目标目录最新长度。
     */
    if (read_dir_entry_by_index(target.first, 0, &target_dot) != 0 ||
        !is_valid_fcb(&target_dot)) {
        printf("目标目录状态异常。\n");
        return;
    }

    target.length = target_dot.length;
    target.time = target_dot.time;
    target.date = target_dot.date;

    /*
     * 原地覆写当前目录打开项
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
     * 设置当前目录的父目录块号 dirno
     */
    if (strcmp(dirname, "..") == 0) {
        if (read_dir_entry_by_index(target.first, 1, &parent_dotdot) == 0 &&
            is_valid_fcb(&parent_dotdot)) {
            curdir->dirno = parent_dotdot.first;
        } else {
            curdir->dirno = target.first;
        }
    } else {
        curdir->dirno = olddir.first;
    }
}

void my_mkdir(char *dirname) {
    useropen *curdir;
    fcb exist;
    fcb newdir;
    fcb dot;
    fcb dotdot;
    int newblk;
    int slot_index;
    int need_append;

    if (dirname == NULL || dirname[0] == '\0') {
        printf("usage: mkdir <dirname>\n");
        return;
    }

    if (!check_fd(curdirid)) {
        printf("当前目录状态非法。\n");
        return;
    }

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        printf("不能创建特殊目录 %s\n", dirname);
        return;
    }

    if (strlen(dirname) >= sizeof(newdir.filename)) {
        printf("目录名过长，最多 7 个字符。\n");
        return;
    }

    curdir = &openfilelist[curdirid];

    if (find_entry_in_dir(curdir->first,
                          curdir->open_fcb.length,
                          dirname,
                          &exist,
                          NULL) == 0) {
        printf("已存在同名文件或目录: %s\n", dirname);
        return;
    }

    newblk = allocBlock();
    if (newblk < 0) {
        printf("磁盘空间不足，无法创建目录。\n");
        return;
    }

    /* 新目录自身的 FCB */
    fcb_init(&newdir, dirname, (unsigned short)newblk, 0);

    /* 初始化新目录中的 "." */
    fcb_init(&dot, ".", (unsigned short)newblk, 0);

    /*
     * 初始化新目录中的 ".."。
     * 这里复制当前目录最新 open_fcb，保证父目录 first 和 length 尽量保持最新。
     */
    dotdot = curdir->open_fcb;
    strcpy(dotdot.filename, "..");
    memset(dotdot.exname, 0, sizeof(dotdot.exname));
    dotdot.attribute = 0;

    memcpy(blockaddr[newblk], &dot, sizeof(fcb));
    memcpy(blockaddr[newblk] + sizeof(fcb), &dotdot, sizeof(fcb));

    if (find_free_slot_or_append(curdir->first,
                                 curdir->open_fcb.length,
                                 &slot_index,
                                 &need_append) != 0) {
        free_block_chain((unsigned short)newblk);
        printf("父目录写入失败。\n");
        return;
    }

    if (write_dir_entry_by_index(curdir->first, slot_index, &newdir, 1) != 0) {
        free_block_chain((unsigned short)newblk);
        printf("父目录空间不足，创建失败。\n");
        return;
    }

    if (need_append) {
        curdir->length += sizeof(fcb);
        curdir->open_fcb.length = curdir->length;
    }

    curdir->fcbstate = 1;

    /*
     * 同步当前目录自己的 "." 项。
     * 这样之后 cd .. 或 rmdir 判空时，可以拿到目录最新 length。
     */
    {
        fcb self_fcb = curdir->open_fcb;
        strcpy(self_fcb.filename, ".");
        memset(self_fcb.exname, 0, sizeof(self_fcb.exname));
        memcpy(blockaddr[curdir->first], &self_fcb, sizeof(fcb));

        /*
         * 根目录的 "." 和 ".." 都指向自己。
         * 根目录变化时，把 ".." 也同步一下。
         */
        if (curdir->first == 5) {
            fcb parent_fcb = curdir->open_fcb;
            strcpy(parent_fcb.filename, "..");
            memset(parent_fcb.exname, 0, sizeof(parent_fcb.exname));
            memcpy(blockaddr[curdir->first] + sizeof(fcb), &parent_fcb, sizeof(fcb));
        }
    }

    printf("目录创建成功: %s\n", dirname);
}

void my_rmdir(char *dirname) {
    useropen *curdir;
    fcb target;
    fcb target_dot;
    fcb empty_entry;
    int index;

    if (dirname == NULL || dirname[0] == '\0') {
        printf("usage: rmdir <dirname>\n");
        return;
    }

    if (!check_fd(curdirid)) {
        printf("当前目录状态非法。\n");
        return;
    }

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        printf("不能删除特殊目录 %s\n", dirname);
        return;
    }

    curdir = &openfilelist[curdirid];

    if (find_entry_in_dir(curdir->first,
                          curdir->open_fcb.length,
                          dirname,
                          &target,
                          &index) != 0) {
        printf("目录不存在: %s\n", dirname);
        return;
    }

    if (target.attribute != 0) {
        printf("%s 不是目录，不能用 rmdir 删除。\n", dirname);
        return;
    }

    if (target.first == curdir->first) {
        printf("不能删除当前目录。\n");
        return;
    }

    /*
     * 关键修复：
     * 不能直接使用 target.length 判断目录是否为空。
     * 因为 target 是父目录中保存的目录项，它的 length 可能是旧值。
     * 应读取目标目录自己的 "." 项，使用其中最新的 length。
     */
    if (read_dir_entry_by_index(target.first, 0, &target_dot) != 0 ||
        !is_valid_fcb(&target_dot)) {
        printf("目录状态异常，不能删除: %s\n", dirname);
        return;
    }

    if (!dir_is_empty(target.first, target_dot.length)) {
        printf("目录非空，不能删除: %s\n", dirname);
        return;
    }

    free_block_chain(target.first);

    memset(&empty_entry, 0, sizeof(fcb));
    empty_entry.free = 1;

    if (write_dir_entry_by_index(curdir->first, index, &empty_entry, 0) != 0) {
        printf("删除目录项失败，但盘块已释放，请检查。\n");
        return;
    }

    curdir->fcbstate = 1;

    printf("目录删除成功: %s\n", dirname);
}
