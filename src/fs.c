#include "fs.h"

unsigned char *myvhard = NULL;
useropen openfilelist[MAXOPENFILE];
int curdirid = 0;
unsigned char *blockaddr[100000];
block0 initblock;
fat fat1[100000], fat2[100000];
int BLOCKSIZE = 1024;
int BLOCKNUM = 1000;

void startsys(void) {
    /* TODO: 启动文件系统，加载镜像或格式化 */
}

void my_format(void) {
    /* TODO: 格式化虚拟磁盘，初始化FAT和根目录 */
}

void my_exitsys(void) {
    /* TODO: 退出系统并保存虚拟磁盘 */
}
