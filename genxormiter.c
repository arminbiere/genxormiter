// clang-format off

static char  * usage = 
"usage: genxormiter [ <option> ... ] [ <inputs> [ <seed> ] ]\n"
"\n"
"where '<option>' is one of the following\n"
"\n"
"  -h | --help     print this command line option summary\n"
"  -v | --verbose  include information on variable and XOR order\n"
"  -l | --linear   linear order (no randomization)\n"
"  -s | --same     same input order for parity circuits\n"
"  -r | --reverse  reverse order in second circuit\n"
"\n"

"The generator prints on '<stdout>' the miter between two circuits of\n"
"the given number of shared '<inputs>'.  The DIMACS encoding of the input\n"
"variables as well as the order of the temporary variables introduces for\n"
"the XOR gates in both circuits is completely random based on the given\n"
"seed.  Without any seed specified we generate one base on the number of\n"
"clock ticks of the processor and the current time.\n"
"\n"
"The generated instances can be solved through XOR reasoning (as in\n"
"'Lingeling'), as well as trivially with congruence closure if '-s'\n"
"is specified, which forces the two circuits two reduce inputs in the\n"
"same way.  Interesting enough, if the two circuits reduce the inputs\n"
"in a reverse order (with '-r'), thus in essence a miter between left-\n"
"versus right-associative parity reduction, the instance is solved with\n"
"the help of bounded variable elimination.\n"
"\n"
"Finally randomizing input and temporary indices can be disabled with '-l'.\n"

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

static size_t inputs;
static size_t temporaries;
static size_t variables;
static size_t expected;
static size_t clauses;
static size_t distincts;
static size_t xors;

static bool same;
static bool linear;
static bool reverse;
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
  assert(abs(lit0) != abs(lit1));
  assert(abs(lit0) != abs(lit2));
  assert(abs(lit1) != abs(lit2));
  int lits[3] = {lit0, lit1, lit2};
  if (!linear)
    for (int k = 0; k != 3; k++)
      SWAP(lits, k, 3);
  if (verbose > 1)
    printf("c c[%zu] %d %d %d\n", clauses, lits[0], lits[1], lits[2]);
  assert(clauses < expected);
  memcpy(c[clauses].lits, lits, sizeof lits);
  clauses++;
}

static void xordef(struct clause *c, int lhs, int rhs0, int rhs1) {
  if (!linear) {
    if (flip())
      lhs = -lhs, rhs0 = -rhs0;
    if (flip())
      lhs = -lhs, rhs1 = -rhs1;
    if (flip())
      rhs0 = -rhs0, rhs1 = -rhs1;
  }
  if (verbose)
    printf("c x[%zu] %d = %d ^ %d\n", xors, lhs, rhs0, rhs1);
  ternary(c, lhs, rhs0, -rhs1);
  ternary(c, lhs, -rhs0, rhs1);
  ternary(c, -lhs, rhs0, rhs1);
  ternary(c, -lhs, -rhs0, -rhs1);
  xors++;
}

static void binary(struct clause *c, int lit0, int lit1) {
  valid(lit0), valid(lit1);
  assert(abs(lit0) != abs(lit1));
  int lits[3] = {lit0, lit1, 0};
  if (!linear)
    for (int k = 0; k != 2; k++)
      SWAP(lits, k, 2);
  if (verbose > 1)
    printf("c c[%zu] %d %d\n", clauses, lits[0], lits[1]);
  assert(clauses < expected);
  memcpy(c[clauses].lits, lits, sizeof lits);
  clauses++;
}

static void distinct(struct clause *c, int lit0, int lit1) {
  if (!linear && flip())
    lit0 = -lit0, lit1 = -lit1;
  if (verbose)
    printf("c d[%zu] %d != %d\n", distincts, lit0, lit1);
  binary(c, lit0, lit1);
  binary(c, -lit0, -lit1);
  distincts++;
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
    } else if (!strcmp(arg, "-l") || !strcmp(arg, "--linear")) {
      linear = true;
    } else if (!strcmp(arg, "-s") || !strcmp(arg, "--same")) {
      same = true;
    } else if (!strcmp(arg, "-r") || !strcmp(arg, "--reverse")) {
      reverse = true;
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

  printf("c genxormiter %zu %" PRIu64 "\n", inputs, seed);

  if (inputs == 0) {
    printf("p cnf 0 1\n0\n");
    return 0;
  }

  if (inputs == 1) {
    int lit = !linear && flip() ? -1 : 1;
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
    if (!linear && flip())
      lit = -lit;
    m[j] = lit;
    if (!linear)
      SWAP(m, j, variables);
  }

  if (verbose) {
    for (size_t j = 0; j != inputs; j++)
      printf("c m[%zu] = input[%zu] = %d\n", j, j, m[j]);
    for (size_t j = inputs; j != variables; j++)
      printf("c m[%zu] = temporary[%zu] = %d\n", j, j - inputs, m[j]);
  }

  expected = 4 * temporaries + 2;
  struct clause *c = malloc(expected * sizeof *c);
  if (!c)
    die("out-of-memory allocating clauses");

  int *s[2];
  for (int i = 0; i != 2; i++) {
    if (!(s[i] = malloc(inputs * sizeof *s[i])))
      die("out-of-memory allocating %s stack", i ? "first" : "second");
    if (i && same) {
      for (int j = 0; j != inputs; j++)
        s[1][j] = s[0][j];
    } else if (i && reverse) {
      for (int j = 0; j != inputs; j++)
        s[1][j] = s[0][inputs - 1 - j];
    } else {
      for (int j = 0; j != inputs; j++) {
        s[i][j] = m[j];
        if (!linear)
          SWAP(s[i], j, inputs);
      }
    }
  }

  if (verbose)
    for (int i = 0; i != 2; i++)
      for (int j = 0; j != inputs; j++)
        printf("c s[%d][%d] = input[%d] = %d\n", i, j, i, s[i][j]);

  int n[2] = {inputs, inputs};
  assert(n[0]), assert(n[1]);
  int temporary = inputs;
  while (n[0] > 1 || n[1] > 1) {
    int i;
    if (n[0] == 1)
      i = 1;
    else if (n[1] == 1)
      i = 0;
    else if (linear)
      i = 0;
    else
      i = flip();
    assert(n[i] >= 2);
    int lhs = m[temporary++], rhs[2];
    for (int k = 0; k != 2; k++) {
      if (!linear && !same && !reverse)
        SWAP(s[i], n[i] - 1, n[i]);
      rhs[k] = s[i][--n[i]];
    }
    xordef(c, lhs, rhs[0], rhs[1]);
    assert(n[i] < inputs);
    s[i][n[i]++] = lhs;
  }

  assert(n[0] == 1), assert(n[1] == 1);
  assert(temporary == variables);

  distinct(c, s[0][0], s[1][0]);

  assert(clauses == expected);

  if (!linear)
    for (size_t j = 0; j != clauses; j++)
      SWAP(c, j, clauses);

  printf("p cnf %zu %zu\n", variables, clauses);

  for (size_t j = 0; j != clauses; j++) {
    for (size_t k = 0; k != 3; k++) {
      int lit = c[j].lits[k];
      if (lit)
        printf("%d ", lit);
    }
    fputs("0\n", stdout);
  }

  for (int i = 0; i != 2; i++)
    free(s[i]);

  free(m);
  free(c);

  return 0;
}
