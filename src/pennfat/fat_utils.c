#include "src/pennfat/fat_utils.h"

#include <stdint.h>

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

int parse_first_fat_entry(uint16_t first_entry, uint16_t* block_size_ptr, uint8_t* blocks_in_fat_ptr) {
	*blocks_in_fat_ptr = first_entry >> 8;
	*block_size_ptr = block_size_of_config((uint8_t) first_entry);

	if (block_size_ptr == 0) {
		return -1;
	}
	return 0;
}
