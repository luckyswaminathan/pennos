#include "catch_amalgamated.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

extern "C"
{
    #include "src/pennfat/fat.h"
    #include "src/pennfat/mkfs.h"
}

TEST_CASE("k_open create file", "[k_open]") {
    std::FILE* tmpf = std::tmpfile();
    
    const char* fs_file_const = fs::read_symlink(
        fs::path("/proc/self/fd") / std::to_string(fileno(tmpf))
    ).c_str();
    char fs_file[1024];
    strcpy(fs_file, fs_file_const);

    REQUIRE(mkfs(fs_file, 1, 0) == 0); 
    
    fat16_fs fs;
    REQUIRE(mount(fs_file, &fs) == 0);

    SECTION("write/read once to empty file") {
        int fd = k_open(&fs, "a", F_WRITE);
        REQUIRE(fd >= 0);

        char* str = "hello world";
        int len = strlen(str);
        REQUIRE(k_write(&fs, fd, str, strlen(str)) == len);

        // reading from the same position should yield nothing
        char out[32] = "";
        REQUIRE(k_read(&fs, fd, 32, out) == 0);
        REQUIRE(out[0] == '\0');

        // seeking then reading should yield the string
        REQUIRE(k_lseek(fd, 0, F_SEEK_SET) == 0);
        REQUIRE(k_read(&fs, fd, 32, out) == len);
        REQUIRE(strcmp(out, str) == 0);
    }
}

