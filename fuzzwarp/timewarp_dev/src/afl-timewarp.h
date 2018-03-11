//
// Created by domenukk on 3/6/18.
//

#ifndef FUZZWARP_AFL_TIMEWARP_H
#define FUZZWARP_AFL_TIMEWARP_H

#include <zconf.h>
#include <stdio.h>

#ifdef TIMEWARP_MODE

#define MAX_CNC_LINE_LENGTH 4096         /* Longer lines will be ignored for CNC */

/**
 * Get amount of varargs
 */

#define __NUM_ARGS__(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

/**
 * Close a number of fds
 */

#define CLOSE_ALL(...) do{ \
  close_all(__NUM_ARGS__(__VA_ARGS__), __VA_ARGS__); \
} while(0)

/**
 * DePipe it
 */

#define _P(pipe) (pipe)[1], (pipe)[2]

/**
 * DePipe standardin/out
 */

#define _IN(stdio) _P((stdio).in)
#define _OUT(stdio) _P((stdio).out), _P((stdio).err)
#define _ALL(stdio) _IN(stdio), _OUT(stdio)

/**
 * The correct fds for reading and writing pipes
 */

#define _R(pipe) (pipe)[0]
#define _W(pipe) (pipe)[1]

int fdprintf(int fd, const char *format, ...);

/**
 * Closes up to len sockets
 * @param len the length
 * @param ...
 */

void close_all(size_t len, ...);

typedef enum _timewarp_stage {
  STAGE_LEARN = 'L',
  STAGE_TIMEWARP = 'W',
  STAGE_FUZZ = 'F',
  STAGE_QUIT = 'Q'
} timewarp_stage;

typedef struct _stdpipes {
  int in[2];
  int out[2];
  int err[2];
} stdpipes;

/**
 * Initiates and starts the timewarp server, handling stdin over socket
 * @param port the port to listen to
 * @param pipefd the pipe to forward socket data to
 * @return error code or 0 if successful
 */
int start_timewarp_cnc_server(char *port, stdpipes *cncio, stdpipes *cncio_tap);

/**
 * IO to the timewarped process
 * @param port the port
 * @param stdio stdio[0] will include user input, child process output an errors should be redirected stdio[1]
 * @param stdio_cpy if not NULL, stdio_cpy[0] will be filled with user input, stdio_cpy[1] with program output
 * @return error codes if needed
 */
int start_timewarp_io_server(char *port, stdpipes *stdio, stdpipes *cncio_tap);

/**
 * Stringify a stage name
 * @param stage the stage
 * @return A char* to the stage
 */
char *timewarp_stage_name(timewarp_stage stage);

/**
 *
 * @return
 */
timewarp_stage get_last_action();

/**
 * Creates a bunch of pipes and opens them
 * @return stdpipes with opened in, out and err members.
 */
stdpipes create_stdpipes();

void timewarp_tidy();

#endif /* ^TIMEWARP_MODE */
#endif /* ^FUZZWARP_AFL_TIMEWARP_H */
