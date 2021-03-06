/*
** afl-timewarp.c -- TimeWarp mode fun
*/
#ifdef TIMEWARP_MODE

#include "afl-timewarp.h"
#include "../debug.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdbool.h>

/**
 * Rather dirty way to close all unneeded fds.
 * @param count
 * @param ... the fds. Will ignore negative values.
 * @return the max count
 */

int close_others(int count, ...) {

  int keep_fds[count];
  int max_fd = 0;
  int found_count = 0;
  struct rlimit fd_limit;

  int len = 0;
  va_list params;

  va_start(params, count);
  for (int i = 0; i < count; i++) {

    int fd = va_arg(params, int);
    if (fd >= 0) {
      keep_fds[len] = fd;
      len++;
    }

  }
  va_end(params);

  if (getrlimit(RLIMIT_NOFILE, &fd_limit) < 0) PFATAL("getrlimit");

  for (int i = 0; i < fd_limit.rlim_cur - 1; i++) {

    bool found = false;

    if (found_count < len) {

      for (int k = 0; k < len; k++) {

        if ((keep_fds[k]) == i) {

          found_count++;
          found = true;
          max_fd = i;
          break;

        }
      }
    }

    if (!found) {

      int TODO = 1;
      // TODO: Reenable!    (void) close(i);

    }
  }

  return max_fd;

}

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

  if (listen(sock_fd, 10) == -1) PFATAL("Unable to listen on socket %s\n", port);

  SAYF("Started listening socket on port %s\n", port);
  return sock_fd;

}

/**
 * Read data and write it to 2 file descriptors
 * @param fd_from input file descriptor
 * @param fd_to output file descriptor
 * @param fd_to2 second output file descriptor or -1 if not set
 */
static void forward_output(int fd_from, int fd_to, int fd_to2) {

  u8 buf[4096];
  // forward stdout to socket and err_fd2
  ssize_t len1 = read(fd_from, buf, sizeof(buf));
  if (len1 < 1) RPFATAL("reading from fd"); //TODO: Handle this less brutally

  ck_write(fd_to, buf, (size_t) len1, "Forwarding to fd");

  if (fd_to2 > -1) {

    ck_write(fd_to2, buf, (size_t) len1, "Forwarding to tap");

  }

}

/**
 * Calls pipe on all elements of a stdpipes struct
 * (in, out, error)
 * @param pointer to a new instance of the pipes struct.
 */
void open_stdpipes(stdpipes *pipes) {

  if (pipe(pipes->in)
      | pipe(pipes->out)
      | pipe(pipes->err)) {

    PFATAL("Pipe failed");

  }

}

/**
 * Starts a server that will forward socket communication will mirror all traffic to tap
 * @param port Port to forward to stdpipes
 * @param stdio An stdio element that forwards _R(in), _W(out), _W(err) to the given port.
 * @param stdio_tap will receive all traffic in _R(in), _R(out), _R(err)
 */
static void start_tap_server(char *port, stdpipes *stdio, stdpipes *stdio_tap) {

  open_stdpipes(stdio);
  open_stdpipes(stdio_tap);

  // TODO: forking/allow reattach?

  struct sockaddr_storage their_addr_storage; // connector's address information
  struct sockaddr *their_addr = (struct sockaddr *) &their_addr_storage;
  struct sockaddr_in *their_addr4 = (struct sockaddr_in *) their_addr;
  struct sockaddr_in6 *their_addr6 = (struct sockaddr_in6 *) their_addr;

  socklen_t sin_size;
  char s[INET6_ADDRSTRLEN];

  int server_fd = open_server_socket(port);

  sin_size = sizeof their_addr_storage;
  int sock_fd = accept(server_fd, their_addr, &sin_size);
  if (sock_fd < 0) PFATAL("Error accepting client");

  void *sin_addr =
      their_addr->sa_family == AF_INET ? (void *) &their_addr4->sin_addr : (void *) &their_addr6->sin6_addr;

  inet_ntop(their_addr_storage.ss_family, sin_addr, s, sizeof s);

  SAYF("TimeWarp accepted new Client on port %s\n", port);

  close(server_fd); // Already got a connection to this one. :)

  int child = fork();
  if (child < 0) FATAL("Fork failed");

  if (!child) {
    // In child, forward the traffic.

    int in_fd = _W(stdio->in);
    int in_fd2 = stdio_tap ? _W(stdio_tap->in) : -1;

    int out_fd = _R(stdio->out);
    int err_fd = _R(stdio->err);

    int out_fd2 = stdio_tap ? _W(stdio_tap->out) : -1;
    int err_fd2 = stdio_tap ? _W(stdio_tap->err) : -1;

    int max_fd = CLOSE_OTHERS(
        in_fd,
        in_fd2,
        out_fd,
        out_fd2,
        err_fd,
        err_fd2,
        sock_fd
    );

    while (1) {

      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(out_fd, &readfds);
      FD_SET(err_fd, &readfds);
      FD_SET(sock_fd, &readfds);

      int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
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

  // Parent: Close unnneeded foo.
  CLOSE_ALL(
      _W(stdio->in),
      _R(stdio->out),
      _R(stdio->err),
      sock_fd
  );

  if (stdio_tap) {

    CLOSE_ALL(
        _W(stdio_tap->in),
        _W(stdio_tap->out),
        _W(stdio_tap->err)
    );

  }
}

/**
 * Open the timewarp cnc server
 * @param port server port
 * @param cncio the stdio pipes
 * @param cncio_tap tap object that will also receive any stdin/out data
 */

void start_timewarp_cnc_server(char *port, stdpipes *cncio, stdpipes *cncio_tap) {

  start_tap_server(port, cncio, cncio_tap);


  dprintf(_W(cncio->out), "%d %d %d", cncio->in[0], cncio->in[1], cncio_tap->out[1]);


  dprintf(_W(cncio->out), "%s\n%s\n%s\n%s\n%s\n",
          "Welcome to AFL Timewarp.",
          "Start learning with \"L\"",
          "reset to L and accept current input as Fuzzer input using \"R\" (repeat this multiple times),",
          "then start Fuzzing with \"F\",",
          "exit with \"E\"."); // TODO: Payload or not?

}


/**
 *
 * Start the timewarp server.
 * Blocks until we got a connection on the I/O port
 * @param port the port to open server on
 * @param stdio stdio element to forward io to
 * @param stdio_tap tap port to output in and out of the io port
 */
void start_timewarp_io_server(char *port, stdpipes *stdio, stdpipes *stdio_tap) {

  SAYF("Waiting for connection to stdin/stdout socket on port %s\n", port);
  start_tap_server(port, stdio, stdio_tap);

  dprintf(_W(stdio->out), "%s",
              "Welcome to AF1's TimeWarp port.\n"
              "Interact with the program for as long as you like.\n"
              "To start fuzzing, connect to the CnC port.\n"
              "Child i/o will start after the beep.\n"
              "________________________________\n"
  );
}

// TODO: What do I need to clean up?
void timewarp_tidy() {
  // Parent:
  //close(pipefd[1]);
  wait(NULL); // wait for the child process to exit before I do the same

}


/**
 * Stringify timewarp stages
 * @param stage stage
 * @return a string representation of the current stage
 */
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

/**
 * Closes all given sockets.
 * If using the CLOSE_ALL makro, len will be filled in automatically,
 * @param len the socket count
 * @param ... socket fids (int)
 */
void close_all(size_t len, ...) {

  va_list params;
  va_start(params, len);

  for (size_t i = 0; i < len; i++) {

    close(va_arg(params, int));

  }

  va_end(params);

}

void ck_dup2(int fd_new, int fd_old) {
  if (dup2(fd_new, fd_old) < 0) PFATAL("dup2 failed");
}

/**
 * Processes command and contro input
 * @param fd the cnc in fd
 * @return will not return.
 *//*

int process_cnc(int fd) {

  //TODO: (!!)
  char buf[MAX_CNC_LINE_LENGTH];
  char *current =  buf;
  char *next;
  int len = sizeof(buf);
  int illegal = 0;

  for (;;) {

    // TODO: fix this.
    ssize_t n = read(fd, buf, len + (current - buf) - 2);

    if (n == EINTR) continue;
    if (n < 1) FATAL("Connection to CnC Server lost. Aborting.");

    if (illegal) {

      current = strchr(buf, '\n');
      if (current = NULL) {

        dprintf(fd, "Ignored long line\n");
        current = buf;
        continue;

      }

      illegal = 0;

    }

    next = strchr(current, '\n');

    if (next == NULL) {

      if (current == buf) {

        dprintf(fd, "Line exceeded limit of %d chars", MAX_CNC_LINE_LENGTH);
        illegal = 1;
        continue;

      }

      memmove(buf, current, current - buf);
      continue;

    }

    if (next == buf) {

      memmove(buf, buf + 1, sizeof(buf));
      continue;

    }

    next[0] = '\0';

    if (buf[0] == 'F') {

      dprintf(fd, "Starting to fuzz.");
      warp_mode = STAGE_FUZZ;
      // TODO: stdio foo.
      break;

    }
    // TODO: Handle other actions

  return buf;
}
 */

#endif /* TIMEWARP_MODE */