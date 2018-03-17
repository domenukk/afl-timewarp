#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2

#define PWSIZE 128

static int getLine(char *prmpt, char *buff, size_t sz) {
  int ch, extra;

  // Get line with buffer overrun protection.
  if (prmpt != NULL) {
    printf("%s", prmpt);
    fflush(stdout);
  }
  if (fgets(buff, sz, stdin) == NULL) {
    return NO_INPUT;
  }

  // If it was too long, there'll be no newline. In that case, we flush
  // to end of line so that excess doesn't affect the next call.
  if (buff[strlen(buff) - 1] != '\n') {
    extra = 0;
    while (((ch = getchar()) != '\n') && (ch != EOF))
      extra = 1;
    return (extra == 1) ? TOO_LONG : OK;
  }

  // Otherwise remove newline and give string back to caller.
  buff[strlen(buff) - 1] = '\0';
  return OK;
}

int func(void) {
  printf("What?");
}


// Test program for getLine().

int main(void) {
  int rc;

  char *pw = (char *)malloc(PWSIZE * sizeof(char));
  char *buf = (char *)malloc(PWSIZE * sizeof(char));
  char *complete = (char *)malloc(PWSIZE * 2 * sizeof(char));

  rc = getLine("Enter 8 Char Pwd> ", buf, PWSIZE);
  if (rc == NO_INPUT) {
    // Extra NL since my system doesn't output that on EOF.
    return printf("\nNo Input\n");
  }
  if (rc == TOO_LONG) {
    return printf("Input too long [%s]\n", buf);
  }

  getLine("Same Pwd again> ", pw, PWSIZE);
  if (strlen(pw) < 8 || strncmp(buf, pw, PWSIZE) != 0) {
    fprintf(stderr, "Passwords needs to be at least 8 chars long and matching.\n");
    fflush(stderr);
    return 1;
  }

  strncat(complete, buf, PWSIZE * 2);

  getLine("String to append to pwd> ", buf, PWSIZE);

  strncat(complete + strlen(complete), buf, PWSIZE * 2 - strlen(complete));

  printf("We got [%s]\n", complete);

  fflush(stdout);

  free(pw);
  free(buf);
  free(complete);

  printf("Success :)");

  return 0;
}