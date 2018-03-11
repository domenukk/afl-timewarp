/*
** afl-timewarp.c -- TimeWarp mode fun
*/
#ifdef TIMEWARP_MODE

#include "afl-timewarp.h"
#include "../../../debug.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>

#define BACKLOG 10               /* how many pending connections queue will hold */

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

int fdprintf(int fd, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len < 1) ABORT("snprintf failed work.");

  char buf[len + 1];

  va_start(args, format);
  len = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  if (len < 1) ABORT("snprintf failed work.");

  ck_write(fd, buf, len, "Write Failed.");

  return len;
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

/*
 * Open a listening socket on the given port.
 * Will totally kill the application if an error occurs.
 * Returns the fd of the listening socket (call accept on it)
 */

static int open_server_socket(char *port) {
  int sock_fd = 0;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) FATAL("Getaddrinfo error: %s\n", gai_strerror(rv));

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sock_fd = socket(p->ai_family, p->ai_socktype,
                          p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) PFATAL("Setsockopt failed");

    if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sock_fd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (!sock_fd) PFATAL("No socket found");

  freeaddrinfo(servinfo);

  if (!p) FATAL("server: failed to bind to port %s\n", port);
  if (listen(sock_fd, BACKLOG) == -1) PFATAL("Unable to listen on socket %s\n", port);

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) PFATAL("Error in sigaction ");

  SAYF("Started listening socket on port %s\n", port);
  return sock_fd;
}

// This actually starts the cnc server after the initial process has been forked.
static int _start_timewarp_server(char *port, int *pipefd) {

  int new_fd = 0;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  int sockfd = open_server_socket(port);

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


/**
 * Read data and write it to 2 file descriptors
 * @param fd_from input file descriptor
 * @param fd_to output file descriptor
 * @param fd_to2 second output file descriptor or -1 if not set
 */
void forward_output(int fd_from, int fd_to, int fd_to2) {
  int buf[4096];
  // forward stdout to socket and err_fd2
  ssize_t len1 = read(fd_from, buf, sizeof(buf));
  if (len1 < 1) PFATAL("reading from fd");

  ck_write(fd_to, buf, (size_t) len1, "writing to fd");

  if (fd_to2 > -1) ck_write(fd_to2, buf, (size_t) len1, "wrtingn to stdout2");
  // forward stderr to socket and err_fd2
}

static int max(int a, int b) {
  return a > b ? a : b;
}

int start_tap_server(char *port, stdpipes *stdio, stdpipes *stdio2)  {
  // TODO: forking/allow reattach?
  int new_fd = 0;

  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  char buf[4096];

  int server_fd = open_server_socket(port);


  sin_size = sizeof their_addr;
  int sock_fd = accept(server_fd, (struct sockaddr *) &their_addr, &sin_size);
  if (sock_fd < 0) PFATAL("Error accepting client");

  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
  printf("timewarp: accepted new client: %s\n", s);

  close(server_fd); // Already got a connection to this one. :)

  int child = fork();
  if (child < 0) FATAL("Fork failed");

  if (child) {

    // Thread forward the traffic.

    CLOSE_ALL(
        _R(stdio->in),
        _W(stdio->out),
        _W(stdio->err)
    );

    if (stdio2) {
      CLOSE_ALL(
          _R(stdio2->in),
          _R(stdio2->out),
          _R(stdio2->err)
      );
    }

    int in_fd = _W(stdio->in);
    int in_fd2 = stdio2 ? _W(stdio2->in) : -1;

    int out_fd = _R(stdio->out);
    int err_fd = _R(stdio->err);

    int out_fd2 = stdio2 ? _W(stdio2->out) : -1;
    int err_fd2 = stdio2 ? _W(stdio2->err) : -1;

    fcntl(out_fd, F_SETFL, O_NONBLOCK);
    fcntl(err_fd, F_SETFL, O_NONBLOCK);
    fcntl(sock_fd, F_SETFL, O_NONBLOCK);

    // select needs the max file descriptor + 1
    int nfds = max(max(out_fd, err_fd), sock_fd) + 1;

    while (1) {

      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(out_fd, &readfds);
      FD_SET(err_fd, &readfds);
      FD_SET(sock_fd, &readfds);

      int ret = select(nfds, &readfds, NULL, NULL, NULL);
      if (ret == -1) PFATAL("select()");

      // Read from socket, write to both
      if (FD_ISSET(sock_fd, &readfds)) {

        forward_output(sock_fd, in_fd, in_fd2);

      }

      // Read from fd, write to socket and fd2
      if (FD_ISSET(out_fd, &readfds)) {

        forward_output(out_fd, sock_fd, out_fd2);

      }

      // Read from fd, write to socket and fd2
      if (FD_ISSET(err_fd, &readfds)) {

        forward_output(err_fd, sock_fd, err_fd2);

      }

    }

    ABORT("We left an endless loop?");
  }


  CLOSE_ALL(
      _W(stdio->in),
      _R(stdio->out),
      _R(stdio->err),
      sock_fd
  );

  if (stdio2) {
    CLOSE_ALL(
        _W(stdio2->in),
        _W(stdio2->out),
        _W(stdio2->err)
    );
  }

  return sock_fd;

}

int start_timewarp_cnc_server(char *port, stdpipes *cncio, stdpipes *cncio_tap) {

  pid_t child_pid;

  int sck, client;
  size_t addrlen;
  struct sockaddr_in this_addr, peer_addr;

  return start_tap_server(port, cncio, cncio_tap);

  /**
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
   **/

  FATAL("Not implemented yet");
  return 0;

}


stdpipes create_stdpipes() {
  stdpipes pipes = {0};
  pipe(pipes.in);
  pipe(pipes.out);
  pipe(pipes.err);
  return pipes;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

/*
 * Block until we got a connection on the I/O port
 */

int start_timewarp_io_server(char *port, stdpipes *stdio, stdpipes *cncio_tap) {

  SAYF("Waiting for connection to stdin/stdout socket on port %s", port);
  return start_tap_server(port, stdio, cncio_tap);

}

#pragma clang diagnostic pop


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

void close_all(size_t len, ...) {

  va_list params;
  va_start(params, len);

  for (size_t i = 0; i < len; i++) {
    //printf("%d\n", va_arg(params, int));
    close(va_arg(params, int));
  }

  va_end(params);
}

#endif /* ^TIMEWARP_MODE */