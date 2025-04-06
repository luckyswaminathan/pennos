#include "src/pennfat/fat.h"
#include "src/pennfat/fat_utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILENAME_SIZE 30

int mount(char* fs_name, fat16_fs* ptr_to_fs) {
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        return EMOUNT_OPEN_FAILED;
    }

    // compute the fat size
    uint16_t first_entry;
    if (read(fs_fd, &first_entry, 2) == -1) {
        return EMOUNT_READ_FAILED; 
    }

    uint8_t blocks_in_fat;
    uint16_t block_size;
    if (parse_first_fat_entry(first_entry, &block_size, &blocks_in_fat) == -1) {
        return EMOUNT_BAD_FAT_FIRST_ENTRY;
    }

    size_t fat_size = (size_t)blocks_in_fat * block_size;
    uint16_t* fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0); 
    if (fat == MAP_FAILED) {
        return EMOUNT_MMAP_FAILED;
    }
    if (close(fs_fd) != 0) {
        // try to unmount
        if (munmap(fat, fat_size) == -1) {
            return EMOUNT_CLOSE_FAILED_MUNMAP_FAILED;
        }
        return EMOUNT_CLOSE_FAILED;
    }

    *ptr_to_fs = (fat16_fs) {
        .fat = fat,
        .fat_size = fat_size,
        .block_size = block_size,
        .blocks_in_fat = blocks_in_fat
    };
    return 0;
}

int unmount(fat16_fs* ptr_to_fs) {
    if (munmap(ptr_to_fs->fat, ptr_to_fs->fat_size) == -1) {
        return EUNMOUNT_MUNMAP_FAILED;
    }
    *ptr_to_fs = (fat16_fs){0};
    ptr_to_fs->fat = NULL; // just to be safe, set the ptr for the FAT to NULL
    return 0;
}

#define EK_OPEN_FILENAME_TOO_LONG 1
#define EK_OPEN_INVALID_FILENAME_CHARSET 2

/**
 * Whether the provided string fits thei POSIX filename charset
 */
bool check_filename_charset(const char* str, uint8_t strlen) {
    for (uint8_t i = 0; i < strlen; i++) {
        if (
            !(
                ('a' <= str[i] && str[i] <= 'z') ||
                ('A' <= str[i] && str[i] <= 'Z') ||
                ('0' <= str[i] && str[i] <= '9') ||
                str[i] == '.' || 
                str[i] == '_' || 
                str[i] == '-'
            )
        ) {
           return false; 
        }
    }
    return true;
}

uint32_t get_blocks_in_data_region(fat16_fs* ptr_to_fs) {
    return ((ptr_to_fs->fat_size) / 2) - 1;
}

#define EGET_BLOCK_BLOCK_NUM_0 1
#define EGET_BLOCK_BLOCK_NUM_TOO_HIGH 2
#define EGET_BLOCK_OPEN_FAILED 3
#define EGET_BLOCK_LSEEK_FAILED 4
#define EGET_BLOCK_READ_FAILED 5
#define EGET_BLOCK_CLOSE_FAILED 6

int get_block(fat16_fs* ptr_to_fs, uint16_t block_num, void* data) {
    uint32_t blocks_in_data_region = get_blocks_in_data_region(ptr_to_fs);
    if (block_num < 1) {
        return EGET_BLOCK_BLOCK_NUM_0;
    }

    if (block_num >= blocks_in_data_region) {
        return EGET_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    // todo: it's not efficient to keep opening the same file
    int fs_fd = open(ptr_to_fs->fs_name, O_RDONLY);
    if (fs_fd == -1) {
        return EGET_BLOCK_OPEN_FAILED;
    }

    if (lseek(fs_fd, ptr_to_fs->fat_size + (size_t)block_num * ptr_to_fs->block_size, SEEK_SET) < 0) {
        return EGET_BLOCK_LSEEK_FAILED;
    }

    if (read(fs_fd, data, ptr_to_fs->block_size) == -1) {
        return EGET_BLOCK_READ_FAILED;
    }

    if (close(fs_fd) == -1) {
        return EGET_BLOCK_CLOSE_FAILED;
    }

    return 0;
}

#define ENEXT_BLOCK_BLOCK_NUM_0 1
#define ENEXT_BLOCK_BLOCK_NUM_TOO_HIGH 2

uint16_t next_block(fat16_fs* ptr_to_fs, uint16_t block_num) {
    uint32_t blocks_in_data_region = get_blocks_in_data_region(ptr_to_fs);
    if (block_num < 1) {
        return ENEXT_BLOCK_BLOCK_NUM_0;
    }

    if (block_num >= blocks_in_data_region) {
        return ENEXT_BLOCK_BLOCK_NUM_TOO_HIGH;
    }

    return ptr_to_fs->fat[2 * block_num];
}

/**
 * TODO
 */
int k_open(fat16_fs* ptr_to_fs, const char* fname, int mode) {
    if (mode != F_WRITE) {
        // for now, only support F_WRITE
        exit(EXIT_FAILURE);
    }

    uint8_t strlen = strnlen(fname, MAX_FILENAME_SIZE);

    if (fname[strlen] != '\0') {
        // the string must be longer than MAX_FILENAME_SIZE
        return EK_OPEN_FILENAME_TOO_LONG;
    }

    if (!check_filename_charset(fname, strlen)) {
        return EK_OPEN_INVALID_FILENAME_CHARSET; 
    }

    return 0;
}
