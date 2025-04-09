#include "acutest.h"
#include "src/pennfat/fat.h"
#include "src/pennfat/mkfs.h"
#include <stdio.h>

char* test_fs_name = "testfs999";

void test_k_write_read(void) {
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0); 
    
    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    char* str = "hello world";
    int len = strlen(str);
    TEST_CHECK(k_write(&fs, fd, str, strlen(str)) == len);

    // reading from the same position should yield nothing
    char out[32] = "";
    int bytes_read = k_read(&fs, fd, 32, out);
    TEST_CHECK(bytes_read == 0);
    TEST_MSG("Expected %d", 0);
    TEST_MSG("Produced %d", bytes_read);
    TEST_CHECK(out[0] == '\0');
    TEST_MSG("Expected %c", '\0');
    TEST_MSG("Produced %c", out[0]);

    // seeking then reading should yield the string
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_SET) == 0);
    TEST_CHECK(k_read(&fs, fd, 32, out) == len);
    TEST_CHECK(strcmp(out, str) == 0);

    remove(test_fs_name);
}

TEST_LIST = {
   { "test_k_write_read", test_k_write_read },
   { NULL, NULL } // important: need to have this
};
