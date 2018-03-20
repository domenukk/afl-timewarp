#include <stdio.h>
#include "afl-timewarp.h"

#include<signal.h>
#include <memory.h>

#define PORT "2800"

void sig_handler(int signo) {
  printf("Signal received\n");
  if (signo == SIGUSR1) {
    printf("received SIGUSR1\n");
    int pid = fork();
    if (!pid) {
      printf("I'm the child :)");
      fflush(stdout);
    } else {
      printf("Parent right here.");
      fflush(stdout);
    }
  }
}

int main(int argc, int argv) {

  printf("starting.");
/*
  char buf[16];
  strncpy(buf, ""
      "Thank you for your attention."
      "\n", sizeof(buf));
  printf("%s", buf);

  return 0;
*/

  /*if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
    printf("\ncan't catch SIGUSR1\n");
    return 1;
  }*/

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

  int rec = execv("./reviveme", NULL);

  printf("done, %d", rec);

  return 0;

}
