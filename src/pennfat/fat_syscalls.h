#define ES_PROCESS_FILE_TABLE_FULL -100
int s_open(const char *fname, int mode);

#define ES_READ_UNKNOWN_FD -101
int s_read(int fd, int n, char *buf);


#define ES_WRITE_UNKNOWN_FD -102
#define ES_SEEK_ERROR -103
int s_write(int fd, const char *str, int n);

#define ES_CLOSE_UNKNOWN_FD -103
int s_close(int fd);

int s_unlink(const char *fname);

int s_ls(const char *filename);

int s_chmod(const char *fname, uint8_t perm);

int s_mv(const char *src, const char *dest);
