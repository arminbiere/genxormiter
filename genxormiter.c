// clang-format off

static char  * usage = 
"usage: genxormiter [-h | --help] [-v | --verbose] [ <n> [ <seed> ] ]\n"
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
static bool verbose;

static uint64_t next(void) {
  return seed = seed * 6364136223846793005ul + 1442695040888963407ul;
}

static int pick(unsigned mod) { return (mod / 4294967296.0) * (next() >> 32); }

static bool flip(void) { return pick(2); }

struct clause {
  int size;
  int lits[4];
};

#if 0
static void swap(int *begin, int i, int n) {
  assert(0 <= i), assert(i < n);
  if (!i)
    return;
  int j = pick(i + 1u);
  if (i == j)
    return;
  assert(0 <= j), assert(j < n);
  int tmp = begin[i];
  begin[i] = begin[j];
  begin[j] = tmp;
}
#else
#define SWAP(BEGIN, I, N)                                                      \
  do {                                                                         \
    assert(0 <= I), assert(I < N);                                             \
    if (!I)                                                                    \
      break;                                                                   \
    int J = pick(I + 1u);                                                      \
    if (I == J)                                                                \
      break;                                                                   \
    assert(0 <= J), assert(J < N);                                             \
    const size_t BYTES = sizeof BEGIN[0];                                      \
    char TMP[BYTES];                                                           \
    memcpy(TMP, BEGIN + I, BYTES);                                             \
    memcpy(BEGIN + I, BEGIN + J, BYTES);                                       \
    memcpy(BEGIN + J, TMP, BYTES);                                             \
  } while (0)
#endif

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
    } else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
      verbose = true;
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

  if (inputs == 0) {
    printf("p cnf 0 1\n0\n");
    return 0;
  }

  for (int i = 0; i != 2; i++)
    for (int j = 0; j != inputs; j++) {
      int lit = j + 1;
      if (flip())
        lit = -lit;
      s[i][j] = lit;
    }

  for (int i = 0; i != 2; i++)
    for (int j = 0; j != inputs; j++)
      SWAP(s[i], j, inputs);

  if (verbose)
    for (int i = 0; i != 2; i++) {
      printf("c s[%d] inputs\n", i);
      for (int j = 0; j != inputs; j++)
        printf("c s[%d][%d] = %d\n", i, j, s[i][j]);
    }

  const int temporaries = inputs - 1;
  int *t[2] = {0, 0};

  for (int i = 0; i != 2; i++)
    if (inputs > 1 && !(t[i] = malloc((temporaries) * sizeof *t[i])))
      die("out-of-memory allocating temporary stack of variables");

  int n[2] = {0, 0};

  int variables = inputs;

  while (n[0] != temporaries || n[1] != temporaries) {
    int i;
    if (n[0] == temporaries)
      i = 1;
    else if (n[1] == temporaries)
      i = 0;
    else
      i = flip();
    assert(n[i] < temporaries);
    int lit = ++variables;
    if (flip())
      lit = -lit;
    t[i][n[i]++] = lit;
  }
  for (int i = 0; i != 2; i++)
    for (int j = 0; j != temporaries; j++)
      SWAP(s[i], j, temporaries);

  if (verbose)
    for (int i = 0; i != 2; i++) {
      printf("c t[%d] temporaries\n", i);
      for (int j = 0; j != n[i]; j++)
        printf("c t[%d][%d] = %d\n", i, j, t[i][j]);
    }

  size_t clauses = 8 * (temporaries) + 2;
  struct clause *c = malloc(clauses * sizeof *c);
  if (!c)
    die("out-of-memory allocating clauses");

  int m[2] = {inputs, inputs};
  while (m[0] > 1 || m[1] > 1) {
    int i;
    if (m[0] == 1)
      i = 1;
    else if (m[1] == 1)
      i = 0;
    else
      i = flip();
    assert(m[i] > 1);
    break;
  }

  printf("p cnf %d %zu\n", variables, clauses);

  for (int i = 0; i != 2; i++)
    free(s[i]), free(t[i]);
  free(c);

  return 0;
}
