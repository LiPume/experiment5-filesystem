#include "fs.h"

int main(void) {
    char cmd[32];
    char arg1[DIRLEN];
    int fd, pos;

    startsys();

    while (1) {
        memset(cmd, 0, sizeof(cmd));
        memset(arg1, 0, sizeof(arg1));
        fd = -1;
        pos = -1;

        printf("%s> ", openfilelist[curdirid].dir);

        if (scanf("%31s", cmd) != 1) {
            break;
        }

        if (strcmp(cmd, "ls") == 0) {
            my_ls();
        } else if (strcmp(cmd, "cd") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: cd <dirname>\n");
                continue;
            }
            my_cd(arg1);
        } else if (strcmp(cmd, "mkdir") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: mkdir <dirname>\n");
                continue;
            }
            my_mkdir(arg1);
        } else if (strcmp(cmd, "rmdir") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: rmdir <dirname>\n");
                continue;
            }
            my_rmdir(arg1);
        } else if (strcmp(cmd, "create") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: create <filename>\n");
                continue;
            }
            fd = my_create(arg1);
            if (fd >= 0) {
                printf("create success, fd = %d\n", fd);
            }
        } else if (strcmp(cmd, "rm") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: rm <filename>\n");
                continue;
            }
            my_rm(arg1);
        } else if (strcmp(cmd, "open") == 0) {
            if (scanf("%79s", arg1) != 1) {
                printf("usage: open <filename>\n");
                continue;
            }
            fd = my_open(arg1);
            if (fd >= 0) {
                printf("open success, fd = %d\n", fd);
            }
        } else if (strcmp(cmd, "close") == 0) {
            if (scanf("%d", &fd) != 1) {
                printf("usage: close <fd>\n");
                continue;
            }
            my_close(fd);
        } else if (strcmp(cmd, "write") == 0) {
            /*
             * 用法:
             * write <fd> [pos]
             * 如果不写 pos，可以默认传 -1 表示从当前策略决定位置
             */
            if (scanf("%d", &fd) != 1) {
                printf("usage: write <fd> [pos]\n");
                continue;
            }

            /* 尝试读取可选参数 pos；读不到就用 -1 */
            {
                int c = getchar();
                if (c == '\n') {
                    pos = -1;
                } else {
                    ungetc(c, stdin);
                    if (scanf("%d", &pos) != 1) {
                        pos = -1;
                    }
                }
            }

            my_write(fd, pos);
        } else if (strcmp(cmd, "read") == 0) {
            /*
             * 用法:
             * read <fd> [pos]
             */
            if (scanf("%d", &fd) != 1) {
                printf("usage: read <fd> [pos]\n");
                continue;
            }

            {
                int c = getchar();
                if (c == '\n') {
                    pos = -1;
                } else {
                    ungetc(c, stdin);
                    if (scanf("%d", &pos) != 1) {
                        pos = -1;
                    }
                }
            }

            my_read(fd, pos);
        } else if (strcmp(cmd, "help") == 0) {
            printf("Supported commands:\n");
            printf("  ls\n");
            printf("  cd <dirname>\n");
            printf("  mkdir <dirname>\n");
            printf("  rmdir <dirname>\n");
            printf("  create <filename>\n");
            printf("  rm <filename>\n");
            printf("  open <filename>\n");
            printf("  close <fd>\n");
            printf("  write <fd> [pos]\n");
            printf("  read <fd> [pos]\n");
            printf("  exit\n");
        } else if (strcmp(cmd, "exit") == 0) {
            my_exitsys();
            break;
        } else {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'help' for available commands.\n");
        }
    }

    return 0;
}