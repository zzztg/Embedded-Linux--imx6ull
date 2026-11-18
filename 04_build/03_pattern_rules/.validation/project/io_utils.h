#ifndef IO_UTILS_H
#define IO_UTILS_H

#include <stddef.h>

/* 声明接口：调用者提供 fd、数据起始地址和长度。 */
int write_all(int fd, const char *data, size_t length);

#endif
