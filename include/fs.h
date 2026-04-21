#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 1024000
#define END 65535
#define FREE 0
#define ROOTBLOCKNUM 2
#define MAXOPENFILE 10
#define DIRLEN 80

typedef struct FCB {
    char filename[8];
    char exname[3];
    unsigned char attribute;   // 0:目录文件 1:数据文件
    unsigned short time;
    unsigned short date;
    unsigned short first;      // 起始盘块号
    unsigned long length;      // 文件长度
    char free;                 // 0:已分配 1:空/删除
} fcb;

typedef struct FAT {
    unsigned short id;         // 下一块盘块号，END表示结束
} fat;

typedef struct USEROPEN {
    char filename[8];
    char exname[3];
    unsigned char attribute;
    unsigned short time;
    unsigned short date;
    unsigned short first;
    unsigned long length;
    char dir[DIRLEN];
    int count;                 // 读写指针
    char fcbstate;             // FCB是否修改
    char topenfile;            // 打开表项是否被占用
    int dirno;                 // 父目录起始块号
    int diroff;                // 在父目录中的偏移
    fcb open_fcb;              // 当前打开文件对应的FCB副本
} useropen;

typedef struct BLOCK0 {
    char information[200];
    unsigned short root;
    unsigned char *startblock;
    int blocksize;
    int blocknum;
} block0;

/* 全局变量 */
extern unsigned char *myvhard;
extern useropen openfilelist[MAXOPENFILE];
extern int curdirid;
extern unsigned char *blockaddr[100000];
extern block0 initblock;
extern fat fat1[100000], fat2[100000];
extern int BLOCKSIZE;
extern int BLOCKNUM;

/* 系统相关 */
void startsys(void);
void my_format(void);
void my_exitsys(void);

/* 目录相关 */
void my_ls(void);
void my_cd(char *dirname);
void my_mkdir(char *dirname);
void my_rmdir(char *dirname);

/* 文件相关 */
int my_create(char *filename);
void my_rm(char *filename);
int my_open(char *filename);
void my_close(int fd);

/* 读写相关 */
int my_write(int fd, int pos);
int do_write(int fd, unsigned char *text, int len, char op);
int my_read(int fd, int pos);
int do_read(int fd, unsigned char *text, int len);

#endif
