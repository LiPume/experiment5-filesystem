#include "fs.h"

unsigned char *myvhard = NULL;
useropen openfilelist[MAXOPENFILE];
int curdirid = 0;
unsigned char *blockaddr[100000];
block0 initblock;
fat fat1[100000], fat2[100000];
int BLOCKSIZE = 1024;
int BLOCKNUM = 1000;

/*
 * 启动文件系统：
 * 1. 申请虚拟磁盘内存
 * 2. 尝试从 myfsys 读取旧镜像
 * 3. 如果没有镜像，则重新格式化
 * 4. 初始化根目录打开项
 */
void startsys(void) {
    FILE *fp = NULL;
    int i;

    myvhard = (unsigned char *)malloc(SIZE);
    if (myvhard == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(myvhard, 0, SIZE);

    for (i = 0; i < MAXOPENFILE; i++) {
        memset(&openfilelist[i], 0, sizeof(useropen));
        openfilelist[i].topenfile = 0;
    }

    fp = fopen("myfsys", "rb");
    if (fp != NULL) {
        size_t nread = fread(myvhard, 1, SIZE, fp);
        fclose(fp);

        if (nread > 0) {
            memcpy(&initblock, myvhard, sizeof(block0));

            /* 基本合法性检查，防止坏镜像把程序带崩 */
            if (initblock.blocksize > 0 &&
                initblock.blocknum > 0 &&
                initblock.blocksize * initblock.blocknum <= SIZE) {

                BLOCKSIZE = initblock.blocksize;
                BLOCKNUM = initblock.blocknum;

                for (i = 0; i < BLOCKNUM; i++) {
                    blockaddr[i] = myvhard + i * BLOCKSIZE;
                }

                memcpy(fat1, blockaddr[1], BLOCKNUM * sizeof(fat));
                memcpy(fat2, blockaddr[3], BLOCKNUM * sizeof(fat));

                /* 根目录放入打开文件表第 0 项 */
                curdirid = 0;
		useropen_init(&openfilelist[curdirid], initblock.root, initblock.root, "~/");
		memcpy(&openfilelist[curdirid].open_fcb, blockaddr[initblock.root], sizeof(fcb));
		openfilelist[curdirid].attribute = 0;
		openfilelist[curdirid].length = 2 * sizeof(fcb);
		openfilelist[curdirid].count = 0;
		openfilelist[curdirid].topenfile = 1;

                printf("文件系统加载成功。\n");
                return;
            }
        }
    }

    /* 没有旧镜像或镜像非法，则重新格式化 */

    printf("创建文件系统...\n");
    my_format();
    printf("已创建新的文件系统。\n");
}

/*
 * 格式化文件系统：
 * 约定磁盘布局
 * 0: 引导块
 * 1-2: FAT1
 * 3-4: FAT2
 * 5: 根目录
 * 6...: 数据区
 */
void my_format(void) {
    int i;
    fcb root_dot, root_dotdot;

    printf("请输入磁盘块大小，和数量：\n");
    scanf("%d %d", &BLOCKSIZE, &BLOCKNUM);

    if (BLOCKSIZE <= 0 || BLOCKNUM <= 0 || BLOCKSIZE * BLOCKNUM > SIZE) {
        printf("输入非法，使用默认值 1024 1000。\n");
        BLOCKSIZE = 1024;
        BLOCKNUM = 1000;
    }

    if (myvhard == NULL) {
        myvhard = (unsigned char *)malloc(SIZE);
        if (myvhard == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    memset(myvhard, 0, SIZE);

    for (i = 0; i < BLOCKNUM; i++) {
        blockaddr[i] = myvhard + i * BLOCKSIZE;
    }

    for (i = 0; i < MAXOPENFILE; i++) {
        memset(&openfilelist[i], 0, sizeof(useropen));
        openfilelist[i].topenfile = 0;
    }

    memset(&initblock, 0, sizeof(block0));
    strcpy(initblock.information, "Simple File System");
    initblock.root = 5;
    initblock.startblock = blockaddr[6];
    initblock.blocksize = BLOCKSIZE;
    initblock.blocknum = BLOCKNUM;

    for (i = 0; i < BLOCKNUM; i++) {
        fat1[i].id = FREE;
        fat2[i].id = FREE;
    }

    for (i = 0; i <= 5; i++) {
        fat1[i].id = END;
        fat2[i].id = END;
    }

    fat1[5].id = END;
    fat2[5].id = END;

    fcb_init(&root_dot, ".", 5, 0);
    fcb_init(&root_dotdot, "..", 5, 0);

    memcpy(blockaddr[5], &root_dot, sizeof(fcb));
    memcpy(blockaddr[5] + sizeof(fcb), &root_dotdot, sizeof(fcb));

    memcpy(blockaddr[0], &initblock, sizeof(block0));
    memcpy(blockaddr[1], fat1, BLOCKNUM * sizeof(fat));
    memcpy(blockaddr[3], fat2, BLOCKNUM * sizeof(fat));

    curdirid = 0;
    useropen_init(&openfilelist[curdirid], 5, 5,  "~/");
    memcpy(&openfilelist[curdirid].open_fcb, &root_dot, sizeof(fcb));
    openfilelist[curdirid].attribute = 0;
    openfilelist[curdirid].length = 2 * sizeof(fcb);
    openfilelist[curdirid].count = 0;
    openfilelist[curdirid].topenfile = 1;

    printf("格式化完成。\n");
}

/*
 * 退出系统：
 * 1. 把引导块和 FAT 写回虚拟磁盘
 * 2. 把整个虚拟磁盘保存到 myfsys
 */
void my_exitsys(void) {
    FILE *fp = NULL;

    if (myvhard == NULL) {
        return;
    }

    memcpy(blockaddr[0], &initblock, sizeof(block0));
    memcpy(blockaddr[1], fat1, BLOCKNUM * sizeof(fat));
    memcpy(blockaddr[3], fat2, BLOCKNUM * sizeof(fat));

    fp = fopen("myfsys", "wb");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    fwrite(myvhard, 1, SIZE, fp);
    fclose(fp);

    free(myvhard);
    myvhard = NULL;

    printf("文件系统已保存并退出。\n");
}
