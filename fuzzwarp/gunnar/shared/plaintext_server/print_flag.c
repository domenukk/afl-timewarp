#include <stdio.h>
#include "tools.h"
#include <assert.h>
#include <string.h>



#ifndef DUP2
	#ifndef SOCKET_COMM
		extern FILE * client_stream;
	#else
		extern int client_socket;
	#endif
#endif

void print_flag() {
	FILE* file = fopen("flag.txt", "r");
	if (file == NULL) {
		die("could not open flag file. Sorry! You actually did it!\n This is just a problem with giving you your deserved solution.");
	}
	size_t size = get_file_size(file);
	char buf[1024];
	assert(size < sizeof(buf));
	memset(buf, 0, sizeof(buf));
	size_t bytes_read = 0;
	while (bytes_read < size) {
		bytes_read += fread(buf + bytes_read, sizeof(char), sizeof(buf) - 1 - bytes_read, file);
	}
	assert(bytes_read == size);
	fclose(file);
	#ifdef DUP2
		puts(buf);
	#elif defined(SOCKET_COMM)
		#error print_flag() is currently not implemented in combination with SOCKET_COMM
	#else
		fputs(buf, client_stream);
	#endif
}
