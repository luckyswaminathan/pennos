#include "src/pennfat/mkfs.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * Maps 0,1,2,3,4 to 256,512,1024,2048,4096 bytes
 *
 * 0 return indicates an error
 */
uint16_t block_size_of_config(uint8_t block_size_config) {
	switch (block_size_config) {
		case 0: return 256;
		case 1: return 512;
		case 2: return 1024;
		case 3: return 2048;
		case 4: return 4096;
		default: return 0;
	}
}

int mkfs(char* fs_name, uint8_t blocks_in_fat, uint8_t block_size_config) {
	// the size of the filesystem is equal to the size of the fat plus the size of the data region
	// The size of the fat is just blocks_in_fat * block_size_of_config(block_size_config)
	// The size of the data region is block size * (number of FAT entries - 1)
	
	if (blocks_in_fat < 1 || blocks_in_fat > 32) {
		return BAD_BLOCKS_IN_FAT_VAL;
	}

	uint16_t block_size = block_size_of_config(block_size_config);

	if (block_size == 0) {
		return BAD_BLOCK_SIZE_CONFIG_VAL;
	}

	uint32_t fat_size = (uint32_t)block_size * blocks_in_fat;
	uint32_t blocks_in_data_region = (fat_size / 2) - 1;

	if (blocks_in_data_region >= (1 << 16)) {
		// should be impossible because the the max fat_size should be 4096 bytes * 32
		// but if we do reach this case, return an UNKNOWN_ERROR
		return MKFS_UNKNOWN_ERROR;
	}

	// Create a file with that size that represents ourfilesystem
	int fs_fd = open(fs_name, O_CREAT | O_EXCL | O_WRONLY);
	if (fs_fd < 0) {
		return EMKFS_OPEN_FAILED;	
	}

	uint8_t* empty_block = (uint8_t*) calloc(block_size, sizeof(uint8_t));
	if (empty_block == NULL) {
		return EMKFS_CALLOC_FAILED;
	}
	
	for (uint32_t i = 0; i < (blocks_in_data_region + blocks_in_fat); i++) {
		ssize_t written_bytes = write(fs_fd, empty_block, block_size);
		// error writing
		if (written_bytes < 0) {
			return EMKFS_WRITE_FAILED;
		} else if (written_bytes < block_size) {
			return EMKFS_WRITE_LESS;
		}
	}

	// Go back to the start and write the first FAT entrey
	if (lseek(fs_fd, 0, SEEK_SET) < 0) {
		return EMKFS_LSEEK_FAILED;	
	}
	uint16_t zeroeth_fat_entry = (((uint16_t)blocks_in_fat) << 8) | block_size_config;
	ssize_t written_bytes = write(fs_fd, &zeroeth_fat_entry, 2);
	if (written_bytes < 0) {
		return EMKFS_WRITE_FAILED;
	} else if (written_bytes < 2) {
		return EMKFS_WRITE_LESS;
	}

	// now have the fat, so close the file
	if (close(fs_fd) < 0) {
		return EMKFS_CLOSE_FAILED;
	}

	return 0;
}
