#include <unistd.h>
#include <stdio.h>
#include "afl-timewarp.h"

#define PORT "2800"

int main(int argc, int argv) {
  printf("starting.");
  stdpipes stdio;
  stdpipes stdio_tap;

  char *stdio_srv_port = PORT;

  open_stdpipes(&stdio);
  open_stdpipes(&stdio_tap);

  start_timewarp_io_server(stdio_srv_port, &stdio, &stdio_tap);

  ck_dup2(_R(stdio.in), 0);
  ck_dup2(_W(stdio.out), 1);
  ck_dup2(_W(stdio.err), 2);

  CLOSE_ALL(
      _W(stdio_tap.in),
      _W(stdio_tap.out),
      _W(stdio_tap.err)
  );

  execv("./reviveme", NULL);
  printf("done");

  return 0;

}
