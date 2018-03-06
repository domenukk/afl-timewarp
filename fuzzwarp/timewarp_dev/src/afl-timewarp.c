/*
** afl-timewarp.c -- TimeWarp mode fun
*/
#ifdef TIMEWARP_MODE

#include "afl-timewarp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>

#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

/** get sockaddr, IPv4 or IPv6*/
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *) sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

/** Returns true on success, or false if there was an error */
bool set_socket_blocking(int fd, bool blocking) {
  if (fd < 0) return false;

#ifdef _WIN32
  unsigned long mode = blocking ? 0 : 1;
  return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
  return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}

static void writes(int socket_fd, char *s) {
  write(socket_fd, s, strlen(s));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn" // Supposed to run forever.

static void child_handle_input(int socket_fd, int *pipefd, int child_pid) {
  dprintf(socket_fd, "%s\n%s\n%s\n%s\n%s\n",
          "Welcome to AFL Timewarp.",
          "Start learning with \"L\"",
          "reset to L and accept current input as Fuzzer input using \"R\" (repeat this multiple times),",
          "then start Fuzzing with \"F\",",
          "exit with \"E\"."); // TODO: Payload or not?

  char buf[512];
  ssize_t received = 0;

  //TODO: Replace with DUP2 and
  while (1) {
    // Parsing Type, Length, Value
    received = recv(socket_fd, buf, sizeof(buf), 0);
    if (!received) {
      printf("Child %d finished receiving. Exiting.", child_pid);
    }
    for (int i = 0; i < received; i++) {
      switch (buf[i]) {
        // We don't really handle any of our known tokens differently for now; Maybe later.
        case STAGE_LEARN:
        case STAGE_TIMEWARP:
        case STAGE_FUZZ:
        case STAGE_QUIT:
          // send this on to the parent's parent process write()
          write(pipefd[1], &buf[i], 1);
          break;
        default:
          break;
      }
    }
    // fflush(pipefd[1]);
  }
}

#pragma clang diagnostic pop

// This actually starts the cnc server after the initial process has been forked.
static int _start_timewarp_server(char *port, int *pipefd) {

  int sockfd = 0;
  int new_fd = 0;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }
  if (!sockfd) {
    perror("No socket found");
    exit(1);
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connection on Port %s...\n", port);

  while (1) {

    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
    printf("timewarp: accepted new client: %s\n", s);

    int child_pid = 0;
    if (!(child_pid = fork())) { // this is the child process
      close(sockfd); // child doesn't need the listener

      child_handle_input(new_fd, pipefd, child_pid);

      /* never returns */
      close(new_fd);
      close(pipefd[1]);
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }
  return 0;
}

int start_timewarp_server(char *port, int *pipefd) {

  pid_t cpid;
  char buf;

  pipe(pipefd);
  cpid = fork();
  if (cpid == 0) {
    // child
    close(pipefd[0]); // close read-end of the pipe
    _start_timewarp_server(port, pipefd);
    // Never returns
    fprintf(stderr, "Server loop exited; This point should never be reached.");
    exit(1);
  } else {

    close(pipefd[1]); // close the write-end of the pipe, thus sending EOF to the reader
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK); // non blocking beauty
  }
  return 0;
}

timewarp_stage get_last_action() { /*int in_pipe) { */
  timewarp_stage action;
  // read_
  //while (read(pipefd[0], &buf, 1) > 0) // read while EOF;
//my_struct.a = 8;
  /**
   * TODO
   */
  //   {}
  return STAGE_LEARN;
}

// TODO: What do I need to clean up?
void timewarp_tidy() {
  // Parent:
  //close(pipefd[1]);
  wait(NULL); // wait for the child process to exit before I do the same
}

char *timewarp_stage_name(timewarp_stage stage) {
  switch (stage) {
    case STAGE_LEARN:
      return "Learning";
    case STAGE_TIMEWARP:
      return "TimeWarp";
    case STAGE_FUZZ:
      return "Fuzzing";
    case STAGE_QUIT:
      return "Quitting";
    default:
      return "Unknown";
  }
}

#endif /* ^TIMEWARP_MODE */