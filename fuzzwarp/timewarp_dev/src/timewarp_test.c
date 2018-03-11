#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <kdefakes.h>
#include "afl-timewarp.h"

#define PORT "8081"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, int argv) {
  printf("starting.");
  stdpipes stdio1 = create_stdpipes();
  stdpipes stdio2 = create_stdpipes();
  int ret = start_timewarp_io_server(PORT, &stdio1, &stdio2);

  char buf[4096];
  buf[sizeof(buf) -1] = '\0';

  strcpy(buf, "Hello Server");

  fdprintf(_W(stdio1.out), "%s\n", buf);
  return 1;

  while(1) {

    write(_W(stdio1.out), buf, strlen(buf));

    ssize_t len = read(_R(stdio1.in), buf, sizeof(buf) - 1);
    if (len <= 0) {
      printf("Broken foo");
      return 1;
    }
    printf("%s\n", buf);

  }

  return 0;

/*
    int ret = start_timewarp_cnc_server(PORT, pipefd);
    printf("Started on port %s, ret: %d\n", PORT, ret);

    while(1) {
        printf("%s\n", timewarp_stage_name(get_last_action()));
        sleep(1);
    }

    CLOSE_ALL(_P(pipefd));
    */

    return 0;
}
#pragma clang diagnostic pop