#include "acutest.h"
#include "src/pennfat/fat.h"
#include "src/pennfat/mkfs.h"
#include <stdio.h>

// this will be a min sized fs, so it will have
// 1 block and 256 byte blocks
char *test_fs_name = "testfs999";

void test_k_write_read(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    char *str = "hello world";
    int len = strlen(str);
    TEST_CHECK(k_write(&fs, fd, str, strlen(str)) == len);

    // reading from the same position should yield nothing
    char out[32] = "";
    int bytes_read = k_read(&fs, fd, 32, out);
    TEST_CHECK(bytes_read == 0);
    TEST_MSG("Expected %d", 0);
    TEST_MSG("Produced %d", bytes_read);
    TEST_MSG("Produced %s", out);
    TEST_CHECK(out[0] == '\0');
    TEST_MSG("Expected %c", '\0');
    TEST_MSG("Produced %c", out[0]);

    // seeking then reading should yield the string
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_SET) == 0);

    bytes_read = k_read(&fs, fd, 32, out);
    TEST_CHECK(bytes_read == len);
    TEST_MSG("Expected %d", len);
    TEST_MSG("Produced %d", bytes_read);
    TEST_MSG("Produced %s", out);
    TEST_CHECK(strcmp(out, str) == 0);

    TEST_CHECK(k_close(&fs, fd) == 0);
    TEST_CHECK(unmount(&fs) == 0);
}

void test_k_lseek_past_end(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    TEST_CHECK(k_lseek(fd, 100, F_SEEK_SET) == 100);

    char *str = "hello world";
    int len = strlen(str);
    TEST_CHECK(k_write(&fs, fd, str, strlen(str)) == len);

    TEST_CHECK(k_lseek(fd, 0, F_SEEK_SET) == 0);

    char out[200] = "";
    int bytes_read = k_read(&fs, fd, 200, out);
    TEST_CHECK(bytes_read == 100 + len);
    TEST_MSG("Expected %d", 100 + len);

    for (int i = 0; i < 100; i++)
    {
        TEST_CHECK(out[i] == '\0');
    }
    TEST_CHECK(strcmp(out + 100, str) == 0);

    TEST_CHECK(k_close(&fs, fd) == 0);
    TEST_CHECK(unmount(&fs) == 0);
}

void test_k_lseek_various(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    TEST_CHECK(k_lseek(fd, -1, F_SEEK_CUR) < 0);

    char str[] = "hello";
    TEST_CHECK(k_write(&fs, fd, str, 5) == 5);

    // seek to the second char
    TEST_CHECK(k_lseek(fd, 2, F_SEEK_SET) == 2);

    char out[5] = "";
    TEST_CHECK(k_read(&fs, fd, 5, out) == 3);
    TEST_CHECK(strcmp(out, "llo") == 0);

    // Check the cursor is at the end of the file
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_CUR) == 5);

    // Seek to 2 again, but this time only read 1 byte
    TEST_CHECK(k_lseek(fd, 2, F_SEEK_SET) == 2);
    TEST_CHECK(k_read(&fs, fd, 1, out) == 1);
    TEST_CHECK(out[0] == 'l');
    TEST_MSG("Expected %c", 'l');
    TEST_MSG("Produced %c", out[0]);
    TEST_MSG("Produced out %s", out);

    // Check we're at the 3rd byte
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_CUR) == 3);

    // Seek to the end of the file
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_END) == 5);

    // Try to read 1 byte
    TEST_CHECK(k_read(&fs, fd, 1, out) == 0);
    TEST_CHECK(out[0] == 'l');

    // Try to read 100 bytes
    TEST_CHECK(k_read(&fs, fd, 100, out) == 0);
    TEST_CHECK(out[0] == 'l');

    TEST_CHECK(k_close(&fs, fd) == 0);
    TEST_CHECK(unmount(&fs) == 0);
}

void test_k_many_opens(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fds[1000];
    for (int i = 0; i < 1000; i++)
    {
        fds[i] = k_open(&fs, "a", F_WRITE);
        TEST_CHECK(fds[i] >= 0);
    }

    for (int i = 0; i < 1000; i++)
    {
        TEST_CHECK(k_close(&fs, fds[i]) == 0);
    }

    TEST_CHECK(unmount(&fs) == 0);
}

void test_big_write_and_read(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    char str[1301] = "";
    for (int i = 0; i < 26; i++)
    {
        for (int j = 0; j < 50; j++)
        {
            str[i * 50 + j] = 'a' + i;
        }
    }
    str[1300] = '\0';

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    TEST_CHECK(k_write(&fs, fd, str, 1300) == 1300);
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_CUR) == 1300);
    TEST_CHECK(k_lseek(fd, 0, F_SEEK_SET) == 0);

    char out[1301] = "";
    out[1300] = '\0';
    TEST_CHECK(k_read(&fs, fd, 1300, out) == 1300);
    TEST_CHECK(strcmp(out, str) == 0);
    TEST_MSG("Expected str of length %lu", strlen(str));
    TEST_MSG("Produced str of length %lu", strlen(out));
    TEST_MSG("Expected str: %s", str);
    TEST_MSG("Produced str: %s", out);

    TEST_CHECK(k_close(&fs, fd) == 0);
    TEST_CHECK(unmount(&fs) == 0);
}

void test_stdin_stdout_stderr(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int bytes_written;
    bytes_written = k_write(&fs, STDIN_FD, "hello", 5);
    TEST_CHECK(bytes_written == EK_WRITE_WRITE_FAILED || bytes_written == 5); // write to stdin may or may not fail
    TEST_MSG("Expected %d", EK_WRITE_WRITE_FAILED);
    TEST_MSG("Produced %d", bytes_written);

    bytes_written = k_write(&fs, STDOUT_FD, "hello\n", 6);
    TEST_CHECK(bytes_written == 6);
    bytes_written = k_write(&fs, STDERR_FD, "hello\n", 6);
    TEST_CHECK(bytes_written == 6);

    printf("This is more of a visual test -- verify that the output is correct and input a value for the reads. you'll need to enter hello<nl> three times to continue\n");

    char out[100];
    int bytes_read = k_read(&fs, STDIN_FD, 100, out);
    TEST_CHECK(bytes_read == 6);
    TEST_MSG("Expected %d", 6);
    TEST_MSG("Produced %d", bytes_read);
    TEST_MSG("Expected %s", "hello");
    TEST_MSG("Produced %s", out);
    TEST_CHECK(strcmp(out, "hello\n") == 0);

    bytes_read = k_read(&fs, STDOUT_FD, 100, out);
    TEST_CHECK(bytes_read == 6);
    TEST_CHECK(strcmp(out, "hello\n") == 0);

    bytes_read = k_read(&fs, STDERR_FD, 100, out);
    TEST_CHECK(bytes_read == 6);
    TEST_CHECK(strcmp(out, "hello\n") == 0);

    TEST_CHECK(unmount(&fs) == 0);
}

void test_operating_on_special_fds(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    TEST_CHECK(k_close(&fs, STDIN_FD) == EK_CLOSE_SPECIAL_FD);
    TEST_CHECK(k_close(&fs, STDOUT_FD) == EK_CLOSE_SPECIAL_FD);
    TEST_CHECK(k_close(&fs, STDERR_FD) == EK_CLOSE_SPECIAL_FD);

    TEST_CHECK(k_lseek(STDIN_FD, 0, F_SEEK_SET) == EK_LSEEK_SPECIAL_FD);
    TEST_CHECK(k_lseek(STDOUT_FD, 0, F_SEEK_SET) == EK_LSEEK_SPECIAL_FD);
    TEST_CHECK(k_lseek(STDERR_FD, 0, F_SEEK_SET) == EK_LSEEK_SPECIAL_FD);

    TEST_CHECK(unmount(&fs) == 0);
}

void test_k_ls_on_one_file(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    // 31 char filename
    int fd = k_open(&fs, "this-is-aagam-s-first-ever-file", F_WRITE);
    TEST_CHECK(fd >= 0);
    TEST_MSG("Produced fd: %d", fd);

    TEST_CHECK(k_write(&fs, fd, "hello", 5) == 5);
    TEST_CHECK(k_close(&fs, fd) == 0);

    TEST_CHECK(k_ls(&fs, "this-is-aagam-s-first-ever-file") == 0);

    // get input that the output is correct (using k_write and k_read for fun)
    char out[2];
    out[1] = '\0';
    k_write(&fs, STDOUT_FD, "Does k_ls work? [y/N] ", 22);
    int bytes_read = k_read(&fs, STDIN_FD, 1, out);
    TEST_CHECK(bytes_read == 1);
    TEST_MSG("Expected %d", 1);
    TEST_MSG("Produced %d", bytes_read);

    TEST_CHECK(strcmp(out, "y") == 0);
}

void test_k_ls_after_unlink(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    // 31 char filename
    int fd = k_open(&fs, "this-is-aagam-s-first-ever-file", F_WRITE);
    TEST_CHECK(fd >= 0);
    TEST_MSG("Produced fd: %d", fd);

    TEST_CHECK(k_ls(&fs, NULL) == 0);

    TEST_CHECK(k_unlink(&fs, "this-is-aagam-s-first-ever-file") == 0);

    // get input that the output is correct (using k_write and k_read for fun)
    char out[2];
    out[1] = '\0';
    k_write(&fs, STDOUT_FD, "Does k_ls work? [y/N] ", 22);
    int bytes_read = k_read(&fs, STDIN_FD, 1, out);
    TEST_CHECK(bytes_read == 1);
    TEST_MSG("Expected %d", 1);
    TEST_MSG("Produced %d", bytes_read);

    TEST_CHECK(strcmp(out, "y") == 0);
}

void test_k_ls_on_non_existent_file(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int status = k_ls(&fs, "this-is-aagam-s-first-ever-file");
    TEST_CHECK(status == EK_LS_FIND_FILE_IN_ROOT_DIR_FAILED);
    TEST_MSG("Expected %d", EK_LS_FIND_FILE_IN_ROOT_DIR_FAILED);
    TEST_MSG("Produced %d", status);

    TEST_CHECK(unmount(&fs) == 0);
}

void test_k_ls_multiple_files(void)
{
    remove(test_fs_name); // assume this succeeded

    TEST_CHECK(mkfs(test_fs_name, 1, 0) == 0);

    fat16_fs fs;
    TEST_CHECK(mount(test_fs_name, &fs) == 0);

    int fd = k_open(&fs, "a", F_WRITE);
    TEST_CHECK(fd >= 0);

    TEST_CHECK(k_write(&fs, fd, "hello", 5) == 5);
    TEST_CHECK(k_close(&fs, fd) == 0);

    fd = k_open(&fs, "b", F_WRITE);
    TEST_CHECK(fd >= 0);

    TEST_CHECK(k_write(&fs, fd, "0", 1) == 1);
    TEST_CHECK(k_close(&fs, fd) == 0);

    TEST_CHECK(k_ls(&fs, NULL) == 0);

    // get input that the output is correct (using k_write and k_read for fun)
    char out[2];
    out[1] = '\0';
    k_write(&fs, STDOUT_FD, "Does k_ls work? [y/N] ", 22);
    int bytes_read = k_read(&fs, STDIN_FD, 1, out);
    TEST_CHECK(bytes_read == 1);
    TEST_CHECK(strcmp(out, "y") == 0);

    TEST_CHECK(unmount(&fs) == 0);
}

TEST_LIST = {
    {"test_k_write_read", test_k_write_read},
    {"test_k_lseek_past_end", test_k_lseek_past_end},
    {"test_k_lseek_various", test_k_lseek_various},
    //    { "test_k_many_opens", test_k_many_opens }, // TODO: fix this
    {"test_big_write_and_read", test_big_write_and_read},
    // {"test_stdin_stdout_stderr", test_stdin_stdout_stderr}, // This one requires input and is annoying to run every time
    {"test_operating_on_special_fds", test_operating_on_special_fds},
    {"test_k_ls_on_one_file", test_k_ls_on_one_file},
    {"test_k_ls_after_unlink", test_k_ls_after_unlink},
    {"test_k_ls_on_non_existent_file", test_k_ls_on_non_existent_file},
    {"test_k_ls_multiple_files", test_k_ls_multiple_files},
    {NULL, NULL} // important: need to have this
};
