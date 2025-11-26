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

// We need
//
//   variables <= INT_MAX
//
// and for 'inputs > 1' we have
//
//    temporaries = 2*(inputs - 1)
//      variables =  inputs + temporaries
//
// thus
//
//       INT_MAX >= variables
//                = inputs + 2*(inputs-1)
//                = 3*inputs - 2
//
// which yields
//
//   INT_MAX + 2 >= 3*inputs
//
// and finally
//
//        inputs <= (INT_MAX+2)/3

#define INPUTS_MAX ((int)(((INT_MAX + 2u) / 3)))

// This gives with 32-bit two-complement
//
//       INT_MAX = 2^31 - 1 = 2147483647
//    INPUTS_MAX = (2147483647 + 2)/3 = 2147483649/3 = 715827883
//
// and note that
//
//     INT_MAX/3 = 2147483647/3 = 715827882
//
// as INT_MAX is divisible by 3.  That would be off by one.

static int inputs;
static int temporaries;
static int variables;
static size_t clauses;
static size_t xors;

static uint64_t seed;
static int verbose;

static uint64_t next(void) {
  return seed = seed * 6364136223846793005ul + 1442695040888963407ul;
}

static int pick(unsigned mod) { return (mod / 4294967296.0) * (next() >> 32); }

static bool flip(void) { return pick(2); }

struct clause {
  int lits[3];
};

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
      die("number of inputs '%s' too large (maximum '%d')", arg);
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

static void valid(int lit) {
  assert(lit);
  assert(lit != INT_MIN);
  assert(abs(lit) <= variables);
}

static void ternary(struct clause *c, int lit0, int lit1, int lit2) {
  valid(lit0), valid(lit1), valid(lit2);
  int lits[3] = {lit0, lit1, lit2};
  for (int k = 0; k != 3; k++)
    SWAP(lits, k, 3);
  if (verbose > 1)
    printf("c c[%zu] %d %d %d\n", clauses, lits[0], lits[1], lits[2]);
  memcpy(c[clauses].lits, lits, sizeof lits);
  clauses++;
}

static void XOR(struct clause *c, int lhs, int rhs0, int rhs1) {
  if (flip())
    lhs = -lhs, rhs0 = -rhs0;
  if (flip())
    lhs = -lhs, rhs1 = -rhs1;
  if (flip())
    rhs0 = -rhs0, rhs1 = -rhs1;
  if (verbose)
    printf("c x[%zu] %d = %d ^ %d\n", xors, lhs, rhs0, rhs1);
  ternary(c, lhs, rhs0, -rhs1);
  ternary(c, lhs, -rhs0, rhs1);
  ternary(c, -lhs, rhs0, rhs1);
  ternary(c, -lhs, -rhs0, -rhs1);
  xors++;
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
      verbose += (verbose < 2);
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

  if (inputs == 1) {
    int lit = flip() ? -1 : 1;
    printf("p cnf 1 2\n");
    printf("%d 0\n", lit);
    printf("%d 0\n", -lit);
    return 0;
  }

  temporaries = 2 * (inputs - 1);
  variables = inputs + temporaries;

  int *m = malloc(variables * sizeof *m);
  if (inputs && !m)
    die("out-of-memory allocating variables");

  for (int j = 0; j != variables; j++) {
    int lit = j + 1;
    if (flip())
      lit = -lit;
    m[j] = lit;
    SWAP(m, j, variables);
  }

  if (verbose) {
    for (int j = 0; j != inputs; j++)
      printf("c m[%d] = input[%d] = %d\n", j, j, m[j]);
    for (int j = inputs; j != variables; j++)
      printf("c m[%d] = temporary[%d] = %d\n", j, j - inputs, m[j]);
  }

  struct clause *c = malloc(4 * temporaries * sizeof *c);
  if (!c)
    die("out-of-memory allocating clauses");

  int *s[2];
  for (int i = 0; i != 2; i++) {
    if (!(s[i] = malloc(inputs * sizeof *s[i])))
      die("out-of-memory allocating %s stack", i ? "first" : "second");
    for (int j = 0; j != inputs; j++) {
      s[i][j] = m[j];
      SWAP(s[i], j, inputs);
    }
  }

  if (verbose)
    for (int i = 0; i != 2; i++)
      for (int j = 0; j != inputs; j++)
        printf("c s[%d][%d] = input[%d] = %d\n", i, j, i, s[i][j]);

  int n[2] = {inputs, inputs};
  assert(n[0]), assert(n[1]);
  int temporary = inputs;
  while (n[0] + n[1] > 3) {
    int i;
    if (n[0] == 1)
      i = 1;
    else if (n[1] == 1)
      i = 0;
    else
      i = flip();
    assert(n[i] >= 2);
    int lhs = m[temporary++], rhs[2];
    for (int k = 0; k != 2; k++) {
      SWAP(s[i], n[i] - 1, n[i]);
      rhs[k] = s[i][--n[i]];
    }
    XOR(c, lhs, rhs[0], rhs[1]);
    assert(n[i] < inputs);
    s[i][n[i]++] = lhs;
  }
  assert(n[0] + n[1] == 3);
  assert(temporary + 1 == variables);

  printf("p cnf %d %zu\n", variables, clauses);

  int i = (n[0] == 1);
  assert(n[i] == 2);
  assert(n[!i] == 1);

  for (int i = 0; i != 2; i++)
    free(s[i]);

  free(m);
  free(c);

  return 0;
}
