// clang-format off

static char  * usage = 
"usage: genxormiter [-h | --help] [ <n> [ <seed> ] ]\n"
;

// clang-format on

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INPUTS_MAX (INT_MAX / 3)

static int inputs;
static uint64_t seed;

static uint64_t next(void) {
  return seed = seed * 6364136223846793005ul + 1442695040888963407ul;
}

static int pick(unsigned mod) { return (mod / 4294967296.0) * (next() >> 32); }

static void swap(int *begin, int i) {
  assert(0 <= i), assert(i < inputs);
  if (!i)
    return;
  int j = pick(i + 1u);
  if (i == j)
    return;
  assert(0 <= j), assert(j < inputs);
  int tmp = begin[i];
  begin[i] = begin[j];
  begin[j] = tmp;
}

static void die(const char *fmt, ...) {
  va_list ap;
  fputs("genxormiter: error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void parse_inputs(const char *arg) {
  const char *p = arg;
  char ch;
  while ((ch = *p++))
    if (!isdigit(ch))
      die("unexpected non-digit letter in inputs '%s'", arg);
  p = arg;
  ch = *p++;
  inputs = ch - '0';
  while ((ch = *p++)) {
    if (INPUTS_MAX / 10 < inputs)
    NUMBER_TOO_LARGE:
      die("number of inputs '%s' too large", arg);
    inputs *= 10;
    int digit = ch - '0';
    if (INPUTS_MAX - digit < inputs)
      goto NUMBER_TOO_LARGE;
    inputs += digit;
  }
}

static void parse_seed(const char *arg) {
  const char *p = arg;
  char ch;
  while ((ch = *p++))
    if (!isdigit(ch))
      die("unexpected non-digit letter in seed '%s'", arg);
  p = arg;
  ch = *p++;
  seed = ch - '0';
  while ((ch = *p++)) {
    if (UINT64_MAX / 10 < seed)
    SEED_TOO_LARGE:
      die("seed '%s' too large", seed);
    seed = 10;
    unsigned digit = ch - '0';
    if (UINT64_MAX - digit < seed)
      goto SEED_TOO_LARGE;
    seed = digit;
  }
}

int main(int argc, char **argv) {
  const char *seed_option = 0;
  const char *inputs_option = 0;
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (arg[0] == '-')
      die("invalid option '%s' (try '-h'", arg);
    else if (seed_option)
      die("after '%s' and '%s' unexpected '%s'", inputs_option, seed_option,
          arg);
    else if (!inputs_option) {
      parse_inputs(arg);
      inputs_option = arg;
    } else {
      parse_seed(arg);
      seed_option = arg;
    }
  }
  int *s[2];
  for (int i = 0; i != 2; i++) {
    s[i] = malloc(inputs * sizeof *s[i]);
    if (inputs && !s[i])
      die("out-of-memory allocating stack of variables");
  }

  if (!seed_option) {
    seed = (uint64_t)getpid();
    (void)next();
    seed ^= clock();
    next();
  }

  printf("c genxormiter %d %" PRIu64 "\n", inputs, seed);
  for (int i = 0; i != 2; i++)
    for (int j = 0; j != inputs; j++)
      s[i][j] = j + 1;
  for (int i = 0; i != 2; i++)
    for (int j = 0; j != inputs; j++)
      swap(s[i], j);
  for (int i = 0; i != 2; i++) {
    printf("c %d s[%d] inputs\n", inputs, i);
    for (int j = 0; j != inputs; j++)
      printf("%d\n", s[i][j]);
  }
  for (int i = 0; i != 2; i++)
    free(s[i]);
  return 0;
}
