#include "src/pennfat/mkfs.h"

#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


char** whitespace_tokenize(char *string, size_t* n_tokens) {
	size_t length = strlen(string);
	char* copy_of_string = (char*) malloc(sizeof(char) * length);

	if (copy_of_string == NULL) {
		return NULL;
	}


	size_t _n_tokens = 0;
	char *tok;
	char *rest = copy_of_string;
	
	// count number of tokens
	while ((tok = strtok_r(rest, " \t\n\r", &rest))) {
		_n_tokens += 1;
	}
	free(copy_of_string);

	// allocate a char** to store tokens based on the original string
	rest = string;
	char** tokens = (char**) malloc(sizeof(char*) * _n_tokens);
	if (tokens == NULL) {
		return NULL;
	}

	for (size_t i = 0; (tok = strtok_r(rest, " \t\n\r", &rest)) && i < _n_tokens; i++) {
		tokens[i] = tok;
	}

	*n_tokens = _n_tokens;
	return tokens;
}

long int safe_strtol(char *string, char* prefix, bool* ok) {
	char* endptr;

	errno = 0; // reset errno
	long int val = strtol(tokens[2], &endptr, 10);
	
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
	// redirect readline prompt to use stderr
	rl_outstream = stderr;
	while (true) {
		char* line = readline("PENNFAT>");
		if (line == NULL) {
			if (errorno != 0) {
				perror("Failed to read line");
				continue;
			} else {
				// EOF sent by user -- exit the shell
				exit(EXIT_SUCCESS)
			}
		}

		size_t n_tokens;
		char** tokens = whitespace_tokenize(line, &n_tokens); 
		if (tokens == NULL) {
			fprintf(stderr, "Failed to tokenize by whitespace\n");
			goto cleanup_line;
		}

		if (n_tokens == ) {
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
				long int long_blocks_in_fat = safe_strtol(tokens[2], "mkfs BLOCKS_IN_FAT arg", &ok);
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
					goto cleanup_tokens;
				}
				if (long_block_size_config < 0 || long_block_size_config > 4) {
					fprintf(stderr, "mkfs BLOCK_SIZE_CONFIG arg: must be between 0 and 4 inclusive\n");
					goto cleanup_tokens;
				}
				blocks_in_fat = (uint8_t) long_blocks_in_fat;
			}


			int mkfs_err = mkfs(fs_name, blocks_in_fat, block_size_config);
			if (mkfs_err != 0) {
				// TODO: add error prints here
				goto cleanup_tokens;
			}

		}
		

		// Last thing: free line and tokenized version
		cleanup_tokens:
		free(tokens);
		cleanup_line:
		free(line);
	}
}
