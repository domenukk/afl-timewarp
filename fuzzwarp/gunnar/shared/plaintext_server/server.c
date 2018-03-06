#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ucontext.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <sys/capability.h>
#include "tools.h"


extern void handle_client_wrapper(int);

volatile int terminate = 0;
void handle_term_signal(int signal_number) {
	fprintf(stderr, "received signal number %d\n", signal_number);
	terminate = 1;
}



/*void handle_child_exit(int signal_number, siginfo_t * siginfo, void * context) {
	if (signal_number == SIGCHLD && siginfo->si_signo == signal_number) {
		if(siginfo->si_code == CLD_EXITED || siginfo->si_code == CLD_DUMPED) {
			int result = waitpid(siginfo->si_pid, (void *) 0, WNOHANG);
			if (result == -1) {
				die("error eliminating zombie. (pumpgun stuck)");
			}
		} else {
			die("child process didn't exit or die. Something is strange.");
		};
	} else {
		die("received signal for something different than a child. Something is strange.");
	};
}*/

void set_signal_handler(int signo, void (*handler)(int), int flags) {
	
	int result;
	struct sigaction sighandler;
	result = sigfillset(&sighandler.sa_mask);
	if (result == -1) {
		die("could not initialize signal set");
	};
	sighandler.sa_handler = handler;
	sighandler.sa_flags = flags;
	result = sigaction(signo, &sighandler, (void*) 0);
	if (result == -1) {
		die("could not set up signal handler");
	}
	
}

int main(int argc, char** argv, char** envp) {
	
	in_addr_t bind_ips = INADDR_LOOPBACK;
	in_port_t bind_port = 2000;
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--public") == 0) {
			bind_ips = INADDR_ANY;
		} else if (strcmp(argv[i], "--local") == 0) {
			bind_ips = INADDR_LOOPBACK;
		} else if (strcmp(argv[i], "--daemon") == 0) {
			int result = daemon(1, 0);
			if (result != 0) {
				die("error switching to daemon mode");
			};
		} else if (strcmp(argv[i], "--port") == 0) {
			if (i >= argc - 1) {
				die("missing port number");
			} else {
				unsigned long temp = strtoul(argv[i+1], NULL, 10);
				bind_port = (in_port_t) temp;
				if ((unsigned long) bind_port != temp) {
					die("invalid port number");
				}
				i++;
			}
		} else {
			die("unrecognized option, only --port <port>, --public and --daemon accepted.");
		}
	}
	
	
	int result = 0;
	
	// setup cleaning of zombie child processes
	set_signal_handler(SIGCHLD, SIG_DFL, SA_NOCLDSTOP | SA_NOCLDWAIT);
	
	// setup graceful exit on certain signals
	set_signal_handler(SIGINT, &handle_term_signal, 0);
	set_signal_handler(SIGTERM, &handle_term_signal, 0);
	// dont override the default for SIGQUIT: SIGQUIT indicates an immediate exit with core dump.
	set_signal_handler(SIGHUP, &handle_term_signal, 0);
	
	// create a socket
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		die("could not create socket");
	}
	
	// allow re-binding to a port
	{
		int t = 1;
		result = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));
		if (result != 0) {
			die("could not enable socket reuse");
		};
	}
	
	//bind socket to port
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(bind_port);
	sockaddr.sin_addr.s_addr = htonl(bind_ips);
	result = bind(s, (struct sockaddr*) &sockaddr, sizeof(struct sockaddr_in));
	if (result == -1) {
		die("could not bind socket");
	};
	
	// listen
	result = listen(s, 10);
	if (result == -1) {
		die("could not listen on socket");
	}
	
	// if we have the CAP_SYS_ADMIN capability, we can use it to
	// move this process to a new network namespace.
	// By default, the new namespace will not have any network device
	// (except for a loopback device). Thus, the process will
	// be isolated/containerized from accessing network.
	{
		// get the set of capabilities we have
		cap_t my_caps = cap_get_proc();
		if (my_caps == NULL) die("Failed to request process capability set.");
		
		// extract the CAP_SYS_ADMIN capability
		cap_flag_value_t value;
		result = cap_get_flag(my_caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &value);
		assert(result == 0);
		
		// if we have the capability, switch to a new network namespace.
		if (value == CAP_SET) {
			result = unshare(CLONE_NEWNET);
			if (result != 0) {
				die("could not create to a new network namespace.");
			};
		}
		
		// in any case, drop all priviledges.
		// First step: make the capability set the empty set.
		result = cap_clear(my_caps);
		assert(result == 0);
		
		// Second step: overwrite this process' capability set with the empty set
		result = cap_set_proc(my_caps);
		if (result != 0) die("Failed to drop capabilities.");
		
		// free the allocated object
		result = cap_free(my_caps);
		if (result != 0) die("could not free capability set.");
		
	}
	
	// make sure to drop other priviledges
	uid_t euid = geteuid();
	result = setresuid(euid, euid, euid);
	if (result != 0) {
		die("could not set effective/real/saved uid");
	};
	gid_t egid = getegid();
	result = setresgid(egid, egid, egid);
	if (result != 0) {
		die("could not set effective/real/saved gid");
	};
	
	// wait for incoming connections
	int connection;
	while (terminate == 0 && result != -1) {
		connection = accept(s, NULL, 0);
		if (connection == -1 && errno == EINTR) {
			// this case happens if the process received a signal while
			// waiting for a connection. In this case, probably terminate == 1.
			// so, we don't have to do anything.
		} else if (connection == -1) {
			perror("error on accept");
			terminate = 1;
		} else {
			// we have a valid connection. handle it!
			pid_t pid = fork();
			if (pid == -1) {
				perror("error forking");
				terminate = 1;
			} else if (pid == 0) {
				// child process
				
				// firstly, close the server (lisetning) socket.
				result = close(s);
				if (result != 0) {
					die("child could not close its copy of the server socket");
				}
				
				//then, answer the client
#ifdef NO_EXECVE
				handle_client_wrapper(connection);
#else
				char arg[16];
				sprintf(arg, "%d", connection);
				char * program_name = "handler";
				char * args[3] = { program_name, arg, NULL };
				result = execve(program_name, args, envp);
				if (result == -1) {
					die("execve failed");
				};
#endif
				
				exit(0);
				
			} else {
				// parent process
				result = close(connection);
				if (result != 0) {
					perror("could not close client connection");
					terminate = 1;
				}
			}
		}
	};
	
	result = close(s);
	if (result != 0) {
		die("could not close server socket");
	}
	
	printf("Shutting down :)\n");
	
	return 0;
	
}
