#ifndef PENNFAT_FAT_CONSTANTS_H
#define PENNFAT_FAT_CONSTANTS_H

#define F_WRITE 1
#define F_READ 0
#define F_APPEND 2

#define F_SEEK_SET 1
#define F_SEEK_CUR 2
#define F_SEEK_END 3

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

#define F_CHMOD_SET 0
#define F_CHMOD_ADD 1
#define F_CHMOD_REMOVE 2

#define F_CHMOD_R 4
#define F_CHMOD_W 2
#define F_CHMOD_X 1 // TODO: should only be set if F_CHMOD_R is also set (enforce in chmod)

#endif // PENNFAT_FAT_CONSTANTS_H
