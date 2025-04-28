#include "src/pennfat/fat_constants.h"

int s_open(const char *fname, int mode);

int s_read(int fd, int n, char *buf);

int s_write(int fd, const char *str, int n);

int s_close(int fd);

int s_unlink(const char *fname);

int s_ls(const char *filename);

int s_chmod(const char *fname, uint8_t perm, int mode);

int s_mv(const char *src, const char *dest);

int s_fprintf_short(int fd, const char *format, ...);
