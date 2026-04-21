#include "io.h"

int my_write(int fd, int pos) {
    /* TODO: 写文件 */
    (void)fd;
    (void)pos;
    return -1;
}

int do_write(int fd, unsigned char *text, int len, char op) {
    /* TODO: 实际写文件 */
    (void)fd;
    (void)text;
    (void)len;
    (void)op;
    return -1;
}

int my_read(int fd, int pos) {
    /* TODO: 读文件 */
    (void)fd;
    (void)pos;
    return -1;
}

int do_read(int fd, unsigned char *text, int len) {
    /* TODO: 实际读文件 */
    (void)fd;
    (void)text;
    (void)len;
    return -1;
}
