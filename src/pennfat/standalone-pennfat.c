#include "src/pennfat/mkfs.h"
#include "src/pennfat/fat.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>


char** whitespace_tokenize(char *string, size_t* n_tokens) {
	size_t length = strlen(string) + 1; // +1 for the null terminator
	char* copy_of_string = (char*) malloc(sizeof(char) * length);
	if (copy_of_string == NULL) {
		return NULL;
	}
	strcpy(copy_of_string, string);


	// count number of tokens
	size_t _n_tokens = 0;
	char* tok = strtok(copy_of_string, " \t\n\r");
	while (tok != NULL) {
		_n_tokens += 1;
		tok = strtok(NULL, " \t\n\r");
	}
	free(copy_of_string);

	// allocate a char** to store tokens based on the original string
	char** tokens = (char**) malloc(sizeof(char*) * _n_tokens);
	if (tokens == NULL) {
		return NULL;
	}

	// actually tokenize the string
	tok = strtok(string, " \t\n\r");
	for (size_t i = 0; tok != NULL && i < _n_tokens; i++) {
		tokens[i] = tok;
		tok = strtok(NULL, " \t\n\r");
	}

	*n_tokens = _n_tokens;
	return tokens;
}

long int safe_strtol(char *string, char* prefix, bool* ok) {
	char* endptr;

	errno = 0; // reset errno
	long int val = strtol(string, &endptr, 10);
	
	*ok = true;
	if (*endptr != '\0') {
		fprintf(stderr, "%s: expected string to be entirely numeric\n", prefix);
		*ok = false;
		return 0;
	}
	if (errno == ERANGE) {
		fprintf(stderr, "%s: underflow or overflow occured while parsing string to number\n", prefix);
		*ok = false;
		return 0;
	}
	if (errno != 0) {
		fprintf(stderr, "%s: unspecified error parsing string to number\n", prefix);
		*ok = false;
		return 0;
	}

	return val;
}

int main(void) {
	fat16_fs fs;
	bool mounted = false;

	while (true) {
		fprintf(stderr, "PENNFAT> ");
		char* line = NULL;
		size_t line_size;

		errno = 0; // clear errno
		ssize_t nread = getline(&line, &line_size, stdin);

		if (nread == -1) {
			if (errno != 0) {
				perror("Failed to read line");
				goto cleanup_line;
			} else {
				// EOF sent by user -- exit the shell
				// Note an EOF sent anywhere is treated as a signal to kill the shell (even if there is a full command before the EOF is sent
				fprintf(stderr, "\n");
				// NOTE: we can exit safely here because fat16_fs and the mmap in it etc will be cleaned up on exit!
				exit(EXIT_SUCCESS);
			}
		}

		size_t n_tokens;
		char** tokens = whitespace_tokenize(line, &n_tokens); 
		if (tokens == NULL) {
			fprintf(stderr, "Failed to tokenize by whitespace\n");
			goto cleanup_line;
		}

		if (n_tokens == 0) {
			goto cleanup_tokens;
		}

		if (strcmp(tokens[0], "mkfs") == 0) {
			if (n_tokens != 4) {
				fprintf(stderr, "mkfs got an incorrect number of arguments\n");
				goto cleanup_tokens;
			}

			char* fs_name = tokens[1];
			
			uint8_t blocks_in_fat;
			{
				bool ok;
				long long_blocks_in_fat = safe_strtol(tokens[2], "mkfs BLOCKS_IN_FAT arg", &ok);
				if (!ok) {
					goto cleanup_tokens;
				}
				if (long_blocks_in_fat < 1 || long_blocks_in_fat > 32) {
					fprintf(stderr, "mkfs BLOCKS_IN_FAT arg: must be between 1 and 32 inclusive\n");
					goto cleanup_tokens;
				}
				blocks_in_fat = (uint8_t) long_blocks_in_fat;
			}

			uint8_t block_size_config;
			{
				bool ok;
				long int long_block_size_config = safe_strtol(tokens[3], "mkfs BLOCK_SIZE_CONFIG arg", &ok);
				if (!ok) {
					// safe_strol already handles the arg
					goto cleanup_tokens;
				}
				if (long_block_size_config < 0 || long_block_size_config > 4) {
					fprintf(stderr, "mkfs BLOCK_SIZE_CONFIG arg: must be between 0 and 4 inclusive\n");
					goto cleanup_tokens;
				}
				block_size_config = (uint8_t) long_block_size_config;
			}


			int mkfs_err = mkfs(fs_name, blocks_in_fat, block_size_config);
			if (mkfs_err != 0) {
				// TODO: add error prints here
				fprintf(stderr, "Failed to mkfs with error code %d\n", mkfs_err);
				goto cleanup_tokens;
			}
		} else if (strcmp(tokens[0], "mount") == 0) {
			if (n_tokens != 2) {
				fprintf(stderr, "mount got wrong number of arguments\n");
				goto cleanup_tokens;
			}

			int mount_err = mount(tokens[1], &fs);
			if (mount_err != 0) {
				fprintf(stderr, "Failed to mount with error code %d\n", mount_err);
				goto cleanup_tokens;
			}
			mounted = true;
		} else if (strcmp(tokens[0], "unmount") == 0) {
			if (n_tokens != 1) {
				fprintf(stderr, "unmount got wrong number of arguments (expected no arguments)\n");
				goto cleanup_tokens;
			}
			if (!mounted) {
				fprintf(stderr, "unmount: there is no filesystem mounted\n");
				goto cleanup_tokens;
			}

			int unmount_err = unmount(&fs);
			if (unmount_err != 0) {
				fprintf(stderr, "Failed to unmount with error code %d\n", unmount_err);
				goto cleanup_tokens;
			}
			mounted = false;

		} else {
			fprintf(stderr, "Unrecognized command\n");
			goto cleanup_tokens;
		}
		

		// Last thing: free line and tokenized version
		cleanup_tokens:
		free(tokens);
		cleanup_line:
		free(line);
	}
}
