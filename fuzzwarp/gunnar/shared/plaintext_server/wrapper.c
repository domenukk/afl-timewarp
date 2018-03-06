#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <assert.h>
#include "tools.h"

/**
 * This function will be called for every incoming connection.
 * Implement it to create an exercise.
 * handle_client can communicate with the client in one of three ways,
 * determined by C preprocessor directives:
 * - If DUP2 is defined, then the wrapper function will overwrite the stdin,
 *   stdout, and stderr file descriptors with the file descriptor for the
 *   socket via the dup2-system call.
 *   All communication with the client can then use stdin/stdout, e.g.
 *   functions like printf, puts, fgets, and so on.
 * - If SOCKET_COMM is defined, the server will store the socket
 *   file descriptor in a global variable (see below) for use by handle_client.
 * - If neither of these is defined, the wrapper server will open a FILE *
 *   stream (seen below) that is connected to the client. Data can then
 *   be read or written to that stream with fgets, fputs, fwrite, fread,
 *   and the lot.
 */
extern void handle_client();

#ifndef DUP2
	#ifndef SOCKET_COMM
		FILE * client_stream;
	#else
		int client_socket;
	#endif
#endif

void handle_client_wrapper(int connection) {
	
	int result;
	
	// configure the socket to linger
	// (i.e., actually send all data before close() returns).
	{
		struct linger l = {
			.l_onoff = 1,
			.l_linger = 2
		};
		result = setsockopt(connection, SOL_SOCKET,
			SO_LINGER, &l, sizeof(struct linger)
		);
		if (result != 0) {
			die("setsockopt failed");
		};
	}
	
	//set up input/output
	#ifdef DUP2
		result = dup2(connection, STDIN_FILENO);
		result |= dup2(connection, STDOUT_FILENO);
		result |= dup2(connection, STDERR_FILENO);
		if (result == -1) {
			die("error on dup2");
		}
		result = setvbuf(stdout, (void *) 0, _IONBF, 0);
		result |= setvbuf(stdin, (void *) 0, _IONBF, 0);
		result |= setvbuf(stderr, (void *) 0, _IONBF, 0);
		if (result != 0) {
			die("setvbuf failed");
		};
	#else
		#ifdef SOCKET_COMM
			client_socket = connection;
		#else
			client_stream = fdopen(connection, "a+");
			if (client_stream == NULL) {
				die("could not open socket as stream");
			}
			result = setvbuf(client_stream, NULL, _IONBF, 0);
			if (result != 0) {
				die("setvbuf failed");
			};
		#endif
	#endif

	//actually answer the client
	handle_client();

	//finally, clean up: flush output buffers,
	//close the connection and exit peacefully.
	#if defined(DUP2)
		// these should not actually be necessary,
		//since stdout and stderr are set to unbuffered
		result = fflush(stdout);
		result |= fflush(stderr);
		if (result != 0) {
			die("Flushing output buffers failed");
		}
	#elif !defined(SOCKET_COMM)
		result = fflush(client_stream);
		if (result != 0) {
			die("Flushing output buffers failed");
		}
	#endif
	
	// wait until all data has been actually transmitted by
	// the kernel (see the SO_LINGER option above).
	// Note: close() may return early if there is some data
	// that has been received by the kernel but not yet
	// delivered to user space. shutdown() should not.
	// Therefore, using shutdown() first seems safer.
	result = shutdown(connection, SHUT_WR);
	if (result != 0) {
		die("shutdown() failed");
	};
	
	
	#if defined(DUP2) || defined(SOCKET_COMM)
		result = close(connection);
		if (result != 0) {
			die("could not close client connection after handling it");
		}
	#else
		result = fclose(client_stream);
		if (result != 0) {
			die("could not close client stream");
		}
	#endif
	exit(0);
};

#ifdef EXECVE
int main(int argc, char ** argv) {
	
	if (argc != 2) {
		die("too few arguments");
	}
	
	int fd = atoi(argv[1]);
	
	handle_client_wrapper(fd);
	
	return 0;
}
#endif
