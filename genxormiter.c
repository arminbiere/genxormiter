// clang-format off

static char  * usage = 
"usage: genxormiter [-h | --help] [ <n> [ <seed> ] ]\n"
;

// clang-format on

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t seed;
static int n;

static void die(const char *fmt, ...) {
  va_list ap;
  fputs("genxormiter: error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

struct stack {
  int *begin;
  int *end;
};

static int parse_int (const char * arg) {
  int res = 0;
  const char * p = arg;
  char ch;
  if (!isdigit ((ch = *p++)))
    die ("expected digit as first letter in '%s'", arg);
  res = ch - '0';
}

int main(int argc, char **argv) {
  const char *seed_option = 0;
  const char *n_option = 0;
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp (arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (arg[0] == '-')
      die("invalid option '%s' (try '-h'", arg);
    else if (seed_option)
      die("after '%s' and '%s' unexpected '%s'", n_option, seed_option, arg);
    else if (!n_option)
      n = parse_int(n_option = arg);
    else
      random = parse_uint64(seed_option = arg);
  }
  if (argc != 2)
    die("expected exactly one argument");
  int n = atoi(argv[1]);
  if (n < 2)
    die("expected number larger than 1 as argument but got '%s'", argv[1]);
  stack first, second;
  first.end = first.begin = malloc(n * sizeof *first.begin);
  second.end = second.begin = malloc(n * sizeof *second.begin);
  printf("c genxormiter %d %" PRIu64 "\n", n, random);
  assert(first.end);
  assert(second.end);
  for (int i = 0; i != n; i++)
    first.end++ = second.end++ = i;
  return 0;
}
