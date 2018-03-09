//
// Created by domenukk on 3/6/18.
//

#ifndef FUZZWARP_AFL_TIMEWARP_H
#define FUZZWARP_AFL_TIMEWARP_H

#include <zconf.h>

#ifdef TIMEWARP_MODE

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

void close_all(size_t len, ...);

typedef enum _timewarp_stage {
    STAGE_LEARN = 'L',
    STAGE_TIMEWARP = 'W',
    STAGE_FUZZ = 'F',
    STAGE_QUIT = 'Q'
} timewarp_stage;

/**
 * Initiates and starts the timewarp server, handling stdin over socket
 * @param port the port to listen to
 * @param pipefd the pipe to forward socket data to
 * @return error code or 0 if successful
 */
int start_timewarp_ctrl_server(char *port, int *pipefd);

/**
 * IO to the timewarped process
 * @param port the port
 * @param stdio stdio[0] will include user input, child process output an errors should be redirected stdio[1]
 * @param stdio_cpy if not NULL, stdio_cpy[0] will be filled with user input, stdio_cpy[1] with program output
 * @return error codes if needed
 */
int start_timewarp_io_server(char *port, int *stdio, int *stdio_cpy);

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

void timewarp_tidy();

#endif /* ^TIMEWARP_MODE */
#endif /* ^FUZZWARP_AFL_TIMEWARP_H */
