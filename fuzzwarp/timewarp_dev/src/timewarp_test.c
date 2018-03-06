#define TIMEWARP_MODE

#include <stdio.h>
#include "./afl-timewarp.c"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, int argv) {
    printf("starting.");
    int pipefd[2];
    start_timewarp_server(8081, pipefd);
    while(1) {
        printf("%s", get_last_action());
        sleep(1000); 
    }
    return 0;
}
#pragma clang diagnostic pop