#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void die(const char * msg) {
	perror(msg);
	fprintf(stderr, "Error number (errno): %d.\n", errno);
	exit(1);
}


long get_file_size(FILE * file) {
	long current_pos = ftell(file);
	if (current_pos == -1) {
		die("could not get file position");
	};
	int result = fseek(file, 0, SEEK_END);
	if (result != 0) {
		die("could not seek to end of file");
	}
	long end = ftell(file);
	if (end == -1) {
		die("could not get end of file position");
	}
	result = fseek(file, current_pos, SEEK_SET);
	if (result != 0) {
		die("could not seek back to old file position while determining file size");
	}
	return end;
}


