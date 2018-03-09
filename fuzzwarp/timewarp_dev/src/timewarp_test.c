#include <unistd.h>
#include <stdio.h>
#include "afl-timewarp.h"

#define PORT "8081"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, int argv) {
  printf("starting.");
    int pipefd[2];

    int ret = start_timewarp_ctrl_server(PORT, pipefd);
    printf("Started on port %s, ret: %d\n", PORT, ret);

    while(1) {
        printf("%s\n", timewarp_stage_name(get_last_action()));
        sleep(1);
    }

    CLOSE_ALL(pipefd[1], pipefd[2]);

    return 0;
}
#pragma clang diagnostic pop