#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <kdefakes.h>
#include "afl-timewarp.h"

#define PORT "2800"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, int argv) {
  printf("starting.");
  stdpipes stdio = create_stdpipes();
  stdpipes stdio_tap = create_stdpipes();

  char *stdio_srv_port = PORT;

  /* Umpf. On OpenBSD, the default fd limit for root users is set to
     soft 128. Let's try to fix that... */

  /* Isolate the process and configure standard descriptors. If out_file is
     specified, stdin is /dev/null; otherwise, out_fd is cloned instead. */

  //setsid();

  // start_timewarp_cnc_server(cnc_srv_port, &cncio, NULL); // TODO: Tap that?

  start_timewarp_io_server(stdio_srv_port, &stdio, &stdio_tap);

  dup2(_R(stdio.in), 0);
  dup2(_W(stdio.out), 1);
  dup2(_W(stdio.err), 2);

  CLOSE_ALL(
      _R(stdio_tap.in),
      _W(stdio_tap.out),
      _W(stdio_tap.err)
  );

  execv("./reviveme", NULL);
  return 0;

}


      /*
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
       */

/*
    int ret = start_timewarp_cnc_server(PORT, pipefd);
    printf("Started on port %s, ret: %d\n", PORT, ret);

    while(1) {
        printf("%s\n", timewarp_stage_name(get_last_action()));
        sleep(1);
    }

    CLOSE_ALL(_P(pipefd));
    */

#pragma clang diagnostic pop