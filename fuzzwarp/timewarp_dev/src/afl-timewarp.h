//
// Created by domenukk on 3/6/18.
//

#ifndef FUZZWARP_AFL_TIMEWARP_H
#define FUZZWARP_AFL_TIMEWARP_H

#ifdef TIMEWARP_MODE

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
int start_timewarp_server(char *port, int* pipefd);

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
