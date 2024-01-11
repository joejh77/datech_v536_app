#ifndef __SYSFS_H__
#define __SYSFS_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int sysfs_writeAppend(const char *path, const char *data, size_t size);;

int sysfs_write(const char *path, const char *data, size_t size);

int sysfs_replace(const char *path, const char *data, size_t size, int offset);

int sysfs_putchar(const char *path, int ch);

int sysfs_printf(const char *path, const char *fmt, ...);

int sysfs_read(const char *path, char *buf, size_t len);

int sysfs_read_offset(const char *path, char *buf, size_t len, int offset);

int sysfs_getchar(const char *path);

int sysfs_scanf(const char *path, const char *fmt, ...);

int sysfs_getsize(const char *path);
#ifdef __cplusplus
}
#endif

#endif
