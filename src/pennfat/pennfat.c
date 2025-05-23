#include "src/pennfat/mkfs.h"
#include "src/pennfat/fat.h"
#include "src/pennfat/fat_constants.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

char **whitespace_tokenize(char *string, size_t *n_tokens)
{
	size_t length = strlen(string) + 1; // +1 for the null terminator
	char *copy_of_string = (char *)malloc(sizeof(char) * length);
	if (copy_of_string == NULL)
	{
		return NULL;
	}
	strcpy(copy_of_string, string);

	// count number of tokens
	size_t _n_tokens = 0;
	char *tok = strtok(copy_of_string, " \t\n\r");
	while (tok != NULL)
	{
		_n_tokens += 1;
		tok = strtok(NULL, " \t\n\r");
	}
	free(copy_of_string);

	// allocate a char** to store tokens based on the original string
	char **tokens = (char **)malloc(sizeof(char *) * _n_tokens);
	if (tokens == NULL)
	{
		return NULL;
	}

	// actually tokenize the string
	tok = strtok(string, " \t\n\r");
	for (size_t i = 0; tok != NULL && i < _n_tokens; i++)
	{
		tokens[i] = tok;
		tok = strtok(NULL, " \t\n\r");
	}

	*n_tokens = _n_tokens;
	return tokens;
}

long int safe_strtol(char *string, char *prefix, bool *ok)
{
	char *endptr;

	errno = 0; // reset errno
	long int val = strtol(string, &endptr, 10);

	*ok = true;
	if (*endptr != '\0')
	{
		char* err_msg = "%s: expected string to be entirely numeric\n";
		k_write(STDERR_FILENO, err_msg, strlen(err_msg));
		*ok = false;
		return 0;
	}
	if (errno == ERANGE)
	{
		char* err_msg = "%s: underflow or overflow occured while parsing string to number\n";
		k_write(STDERR_FILENO, err_msg, strlen(err_msg));
		*ok = false;
		return 0;
	}
	if (errno != 0)
	{
		char* err_msg = "%s: unspecified error parsing string to number\n";
		k_write(STDERR_FILENO, err_msg, strlen(err_msg));
		*ok = false;
		return 0;
	}

	return val;
}

#define MAX_LINE_SIZE 1024
char line[MAX_LINE_SIZE];

int main(void)
{
	struct sigaction sa = {0};
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

	while (true)
	{
		char* prompt = "PENNFAT> ";
		k_write(STDERR_FILENO, prompt, strlen(prompt));
		
		ssize_t nread = k_read(STDIN_FD, MAX_LINE_SIZE-1, line);
		line[nread] = '\0';

		
		if (nread == 0)
		{
			// EOF sent by user -- exit the shell
			// Note an EOF sent anywhere is treated as a signal to kill the shell (even if there is a full command before the EOF is sent)
			char* eof_msg = "\n";
			k_write(STDERR_FILENO, eof_msg, strlen(eof_msg));
			// NOTE: we can exit safely here because fat16_fs and the mmap in it etc will be cleaned up on exit!
			exit(EXIT_SUCCESS);
		}

		size_t n_tokens;
		char **tokens = whitespace_tokenize(line, &n_tokens);
		if (tokens == NULL)
		{
			char* err_msg = "Failed to tokenize by whitespace\n";
			k_write(STDERR_FILENO, err_msg, strlen(err_msg));
			continue;
		}

		if (n_tokens == 0)
		{
			goto cleanup_tokens;
		}

		if (strcmp(tokens[0], "mkfs") == 0)
		{
			if (n_tokens != 4)
			{
				char* err_msg = "mkfs got an incorrect number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			char *fs_name = tokens[1];

			uint8_t blocks_in_fat;
			{
				bool ok;
				long long_blocks_in_fat = safe_strtol(tokens[2], "mkfs BLOCKS_IN_FAT arg", &ok);
				if (!ok)
				{
					goto cleanup_tokens;
				}
				if (long_blocks_in_fat < 1 || long_blocks_in_fat > 32)
				{
					char* err_msg = "mkfs BLOCKS_IN_FAT arg: must be between 1 and 32 inclusive\n";
					k_write(STDERR_FILENO, err_msg, strlen(err_msg));
					goto cleanup_tokens;
				}
				blocks_in_fat = (uint8_t)long_blocks_in_fat;
			}

			uint8_t block_size_config;
			{
				bool ok;
				long int long_block_size_config = safe_strtol(tokens[3], "mkfs BLOCK_SIZE_CONFIG arg", &ok);
				if (!ok)
				{
					// safe_strol already handles the arg
					goto cleanup_tokens;
				}
				if (long_block_size_config < 0 || long_block_size_config > 4)
				{
					char* err_msg = "mkfs BLOCK_SIZE_CONFIG arg: must be between 0 and 4 inclusive\n";
					k_write(STDERR_FILENO, err_msg, strlen(err_msg));
					goto cleanup_tokens;
				}
				block_size_config = (uint8_t)long_block_size_config;
			}

			int mkfs_err = mkfs(fs_name, blocks_in_fat, block_size_config);
			if (mkfs_err != 0)
			{
				k_fprintf_short(STDERR_FILENO, "Failed to mkfs with error code %d\n", mkfs_err);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "mount") == 0)
		{
			if (n_tokens != 2)
			{
				char* err_msg = "mount got wrong number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			int mount_err = mount(tokens[1]);
			if (mount_err != 0)
			{
				char* err_msg = "Failed to mount with error code %d\n";
				k_fprintf_short(STDERR_FILENO, err_msg, mount_err);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "umount") == 0 || strcmp(tokens[0], "unmount") == 0)
		{
			if (n_tokens != 1)
			{
				char* err_msg = "unmount got wrong number of arguments (expected no arguments)\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
			if (!is_mounted())
			{
				char* err_msg = "unmount: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			int unmount_err = unmount();
			if (unmount_err != 0)
			{
				char* err_msg = "Failed to unmount with error code %d\n";
				k_fprintf_short(STDERR_FILENO, err_msg, unmount_err);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "touch") == 0)
		{
			if (n_tokens < 2)
			{
				char* err_msg = "touch got wrong number of arguments (expected at least 1 argument)\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
			if (!is_mounted())
			{
				char* err_msg = "touch: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			for (size_t i = 1; i < n_tokens; i++)
			{
				int fd = k_open(tokens[i], F_APPEND);
				if (fd < 0)
				{
					char* err_msg = "touch: failed to open file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, fd);
					goto cleanup_tokens;
				}
				if (k_write(fd, NULL, 0) < 0)
				{
					char* err_msg = "touch: failed to write to file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, fd);
					goto cleanup_tokens;
				}
				k_close(fd);
			}
		}
		else if (strcmp(tokens[0], "mv") == 0)
		{
			if (n_tokens != 3)
			{
				char* err_msg = "mv got wrong number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
			if (!is_mounted())
			{
				char* err_msg = "mv: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			int mv_status = k_mv(tokens[1], tokens[2]);
			if (mv_status != 0)
			{
				char* err_msg = "mv: failed with error code %d\n";
				k_fprintf_short(STDERR_FILENO, err_msg, mv_status);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "rm") == 0)
		{
			if (n_tokens != 2)
			{
				char* err_msg = "rm got wrong number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
			if (!is_mounted())
			{
				char* err_msg = "rm: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			// Remove the file
			int unlink_status = k_unlink(tokens[1]);
			if (unlink_status < 0)
			{
				char* err_msg = "rm: Error - failed to remove file with error code %d\n";
				k_fprintf_short(STDERR_FILENO, err_msg, unlink_status);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "cat") == 0)
		{
			if (!is_mounted())
			{
				char* err_msg = "cat: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			// Check for -w or -a flags
			bool write_mode = false;
			bool append_mode = false;
			char *output_file = NULL;

			// Parse arguments to find flags
			for (size_t i = 1; i < n_tokens; i++)
			{
				if (strcmp(tokens[i], "-w") == 0 && i + 1 < n_tokens)
				{
					write_mode = true;
					output_file = tokens[i + 1];
					i++; // Skip the next token
				}
				else if (strcmp(tokens[i], "-a") == 0 && i + 1 < n_tokens)
				{
					append_mode = true;
					output_file = tokens[i + 1];
					i++; // Skip the next token
				}
			}

			// Case 1: cat -w FILE or cat -a FILE (read from terminal, write to file)
			if ((write_mode || append_mode) && n_tokens == 3)
			{
				int mode = write_mode ? F_WRITE : F_APPEND;
				int fd = k_open(output_file, mode);
				if (fd < 0)
				{
					char* err_msg = "cat: Error - failed to open output file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, fd);
					goto cleanup_tokens;
				}

				// Read from stdin and write to the file
				char buffer[1024];
				int bytes_read;

				while ((bytes_read = k_read(STDIN_FD, 1024, buffer)) > 0)
				{
					if (k_write(fd, buffer, bytes_read) < 0)
					{
						char* err_msg = "cat: Error - failed to write to file\n";
						k_fprintf_short(STDERR_FILENO, err_msg, fd);
						k_close(fd);
						goto cleanup_tokens;
					}
					if (buffer[bytes_read - 1] == '\n')
					{
						break;
					}
				}

				k_close(fd);
			}
			// Case 2: cat FILE1 FILE2 ... [-w|-a OUTPUT_FILE]
			else if (n_tokens >= 2)
			{
				// Open output file if needed
				int out_fd = -1;
				if (output_file != NULL)
				{
					int mode = append_mode ? F_APPEND : F_WRITE;
					out_fd = k_open(output_file, mode);
					if (out_fd < 0)
					{
						k_fprintf_short(STDERR_FILENO, "cat: Error - failed to open output file with error code %d\n", out_fd);
						goto cleanup_tokens;
					}
				}

				// Process each input file
				for (size_t i = 1; i < n_tokens; i++)
				{
					// Skip flag and output file
					if ((strcmp(tokens[i], "-w") == 0 || strcmp(tokens[i], "-a") == 0) && i + 1 < n_tokens)
					{
						i++; // Skip the flag and the output file
						continue;
					}

					// Open current file
					int in_fd = k_open(tokens[i], F_READ);
					if (in_fd < 0)
					{
						k_fprintf_short(STDERR_FILENO, "cat: Error - failed to open input file %s with error code %d\n",
								tokens[i], in_fd);
						if (out_fd >= 0)
						{
							k_close(out_fd);
						}
						goto cleanup_tokens;
					}

					// Read and write
					char buffer[1024];
					int bytes_read;
					while ((bytes_read = k_read(in_fd, sizeof(buffer), buffer)) > 0)
					{
						// Write to output file if specified, otherwise to stdout
						int write_fd = (out_fd >= 0) ? out_fd : STDOUT_FD;
						int k_write_status = k_write(write_fd, buffer, bytes_read);
						if (k_write_status < 0)
						{
							k_fprintf_short(STDERR_FILENO, "cat: Error - write failed with error code %d\n", k_write_status);
							k_close(in_fd);
							if (out_fd >= 0)
							{
								k_close(out_fd);
							}
							goto cleanup_tokens;
						}
					}

					k_close(in_fd);
				}

				if (out_fd >= 0)
				{
					k_close(out_fd);
				}
			}
			else
			{
				char* err_msg = "cat: invalid arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "cp") == 0)
		{
			if (!is_mounted())
			{
				char* err_msg = "cp: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			// cp -h HOST_FILE PENNFAT_FILE (from host to pennfat)
			if (n_tokens == 4 && strcmp(tokens[1], "-h") == 0)
			{
				// Open host file
				int host_fd = open(tokens[2], O_RDONLY);
				if (host_fd < 0)
				{
					char* err_msg = "cp: Error - failed to open host file: %s\n";
					k_fprintf_short(STDERR_FILENO, err_msg, tokens[2]);
					goto cleanup_tokens;
				}

				// Create destination file in PennFAT
				int pennfat_fd = k_open(tokens[3], F_WRITE);
				if (pennfat_fd < 0)
				{
					char* err_msg = "cp: Error - failed to create PennFAT file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, pennfat_fd);
					close(host_fd);
					goto cleanup_tokens;
				}

				// Copy data
				char buffer[1024];
				ssize_t bytes_read;
				while ((bytes_read = read(host_fd, buffer, sizeof(buffer))) > 0)
				{
					int k_write_status = k_write(pennfat_fd, buffer, bytes_read);
					if (k_write_status < 0)
					{
						char* err_msg = "cp: Error - failed to write to PennFAT file with error code %d\n";
						k_fprintf_short(STDERR_FILENO, err_msg, k_write_status);
						close(host_fd);
						k_close(pennfat_fd);
						goto cleanup_tokens;
					}
				}

				close(host_fd);
				k_close(pennfat_fd);
			}
			// cp PENNFAT_FILE -h HOST_FILE (from pennfat to host)
			else if (n_tokens == 4 && strcmp(tokens[2], "-h") == 0)
			{
				// Open PennFAT file
				int pennfat_fd = k_open(tokens[1], F_READ);
				if (pennfat_fd < 0)
				{
					char* err_msg = "cp: Error - failed to open PennFAT file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, pennfat_fd);
					goto cleanup_tokens;
				}

				// Create host file
				int host_fd = open(tokens[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (host_fd < 0)
				{
					char* err_msg = "cp: Error - failed to create host file: %s\n";
					k_fprintf_short(STDERR_FILENO, err_msg, tokens[3]);
					k_close(pennfat_fd);
					goto cleanup_tokens;
				}

				// Copy data
				char buffer[1024];
				int bytes_read;
				while ((bytes_read = k_read(pennfat_fd, sizeof(buffer), buffer)) > 0)
				{
					if (write(host_fd, buffer, bytes_read) < 0)
					{
						char* err_msg = "cp: Error - failed to write to host file\n";
						k_fprintf_short(STDERR_FILENO, err_msg);
						k_close(pennfat_fd);
						close(host_fd);
						goto cleanup_tokens;
					}
				}

				k_close(pennfat_fd);
				close(host_fd);
			}
			// cp SOURCE DEST (both in PennFAT)
			else if (n_tokens == 3)
			{
				// Open source file
				int src_fd = k_open(tokens[1], F_READ);
				if (src_fd < 0)
				{
					char* err_msg = "cp: Error - failed to open source file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, src_fd);
					goto cleanup_tokens;
				}

    			// Open destination file (overwrite, or create if it doesn't exist)
				int dest_fd = k_open(tokens[2], F_WRITE);
				if (dest_fd < 0)
				{
					char* err_msg = "cp: Error - failed to create destination file with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, dest_fd);
					k_close(src_fd);
					goto cleanup_tokens;
				}

				// Copy data
				char buffer[1024];
				int bytes_read;
				while ((bytes_read = k_read(src_fd, sizeof(buffer), buffer)) > 0)
				{
					if (k_write(dest_fd, buffer, bytes_read) < 0)
					{
						char* err_msg = "cp: Error - failed to write to destination file\n";
						k_write(STDERR_FILENO, err_msg, strlen(err_msg));
						k_close(src_fd);
						k_close(dest_fd);
						goto cleanup_tokens;
					}
				}

				k_close(src_fd);
				k_close(dest_fd);
			}
			else
			{
				char* err_msg = "cp: invalid arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "chmod") == 0)
		{
			// TODO: this is technically not correct since this doesn't match chmod(1)
			if (!is_mounted())
			{
				char* err_msg = "chmod: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			if (n_tokens != 3)
			{
				char* err_msg = "chmod: got wrong number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			// Parse permission mode
			if (strlen(tokens[1]) != 1 || tokens[1][0] < '0' || tokens[1][0] > '7')
			{
				char* err_msg = "chmod: invalid permission mode (must be 0-7)\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			uint8_t perm = tokens[1][0] - '0';

			int chmod_status = k_chmod(tokens[2], perm, F_CHMOD_SET);
			if (chmod_status != 0)
			{
				char* err_msg = "chmod: failed with error code %d\n";
				k_fprintf_short(STDERR_FILENO, err_msg, chmod_status);
				goto cleanup_tokens;
			}
		}
		else if (strcmp(tokens[0], "ls") == 0)
		{
			if (!is_mounted())
			{
				char* err_msg = "ls: there is no filesystem mounted\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}

			// If no arguments, list all files
			if (n_tokens == 1)
			{
				int ls_status = k_ls(NULL);
				if (ls_status < 0)
				{
					char* err_msg = "ls: failed with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, ls_status);
					goto cleanup_tokens;
				}
			}
			// List specific file
			else if (n_tokens == 2)
			{
				int ls_status = k_ls(tokens[1]);
				if (ls_status < 0)
				{
					char* err_msg = "ls: failed to list file %s with error code %d\n";
					k_fprintf_short(STDERR_FILENO, err_msg, tokens[1], ls_status);
					goto cleanup_tokens;
				}
			}
			else
			{
				char* err_msg = "ls: got wrong number of arguments\n";
				k_write(STDERR_FILENO, err_msg, strlen(err_msg));
				goto cleanup_tokens;
			}
		}
		else
		{
			char* err_msg = "Unrecognized command\n";
			k_write(STDERR_FILENO, err_msg, strlen(err_msg));
			goto cleanup_tokens;
		}

	// Last thing: free line and tokenized version
	cleanup_tokens:
		free(tokens);
	}
}
