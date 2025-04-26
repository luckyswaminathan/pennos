#ifndef PENNFAT_FAT_UTILS_H
#define PENNFAT_FAT_UTILS_H

#include <stdint.h>

/**
 * Maps 0,1,2,3,4 to 256,512,1024,2048,4096 bytes
 *
 * 0 return indicates an invalid block_size_config was passed
 */
uint16_t block_size_of_config(uint8_t block_size_config);

/**
 * Parse the first entry of the fat into block_size and blocks_in_fat 
 */
int parse_first_fat_entry(uint16_t first_entry, uint16_t* block_size_ptr, uint8_t* blocks_in_fat_ptr);

#endif // PENNFAT_FAT_UTILS_H
