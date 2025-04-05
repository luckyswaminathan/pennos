#include "src/pennfat/fat.h"
#include "src/pennfat/fat_utils.h"

#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

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
