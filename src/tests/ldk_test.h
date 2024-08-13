#ifndef LDK_TEST_H
#define LDK_TEST_H

#ifdef LDK_TEST_IMPLEMENTATION
#include <stdio.h>
#include <signal.h>
#endif
#include <math.h>

// Original macros without message and format string support
#define ASSERT_TRUE(expr) do { \
  if (!(expr)) { \
    printf("\t%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, (#expr)); \
    return 1; \
  } \
} while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(actual, expected) do { \
  if ((actual) != (expected)) { \
    printf("\t%s:%d: Assertion failed: %s == %s\n", __FILE__, __LINE__, #actual, #expected); \
    return 1; \
  } \
} while (0)

#define LDK_TEST_FLOAT_EPSILON 0.1f
#define ASSERT_FLOAT_EQ(actual, expected) do { \
  if (fabs((actual) - (expected)) > LDK_TEST_FLOAT_EPSILON) { \
    printf("\t%s:%d: Assertion failed: %s == %s\n", __FILE__, __LINE__, #actual, #expected); \
    return 1; \
  } \
} while (0)

#define ASSERT_NEQ(actual, expected) do { \
  if ((actual) == (expected)) { \
    printf("\t%s:%d: Assertion failed: %s != %s\n", __FILE__, __LINE__, #actual, #expected); \
    return 1; \
  } \
} while (0)

// Updated macros with message and format string support
#define ASSERT_WITH_MSG(expr, message, ...) do { \
  if (!(expr)) { \
    printf("\t%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, (#expr)); \
    printf("\t\t"); \
    printf(message, ##__VA_ARGS__); \
    printf("\n"); \
    return 1; \
  } \
} while (0)

#define ASSERT_TRUE_MSG(expr, message, ...) ASSERT_WITH_MSG((expr), message, ##__VA_ARGS__)

#define ASSERT_FALSE_MSG(expr, message, ...) ASSERT_WITH_MSG(!(expr), message, ##__VA_ARGS__)

#define ASSERT_EQ_MSG(actual, expected, message, ...) do { \
  if ((actual) != (expected)) { \
    printf("\t%s:%d: Assertion failed: %s == %s\n", __FILE__, __LINE__, #actual, #expected); \
    printf("\t\t"); \
    printf(message, ##__VA_ARGS__); \
    printf("\n"); \
    return 1; \
  } \
} while (0)

#define ASSERT_NEQ_MSG(actual, expected, message, ...) do { \
  if ((actual) == (expected)) { \
    printf("\t%s:%d: Assertion failed: %s != %s\n", __FILE__, __LINE__, #actual, #expected); \
    printf("\t\t"); \
    printf(message, ##__VA_ARGS__); \
    printf("\n"); \
    return 1; \
  } \
} while (0)

typedef int (*LDKTestFunction)();

typedef struct
{
  const char *name;
  LDKTestFunction func;
} LDKTestCase;

#define TEST_CASE(name) {#name, name}

#ifdef LDK_TEST_IMPLEMENTATION

static void internalOnSignal(int signal)
{
  const char* signalName = "Unknown signal";
  switch(signal)
  {
    case SIGABRT: signalName = (const char*) "SIGABRT"; break;
    case SIGFPE:  signalName = (const char*) "SIGFPE"; break;
    case SIGILL:  signalName = (const char*) "SIGILL"; break;
    case SIGINT:  signalName = (const char*) "SIGINT"; break;
    case SIGSEGV: signalName = (const char*) "SIGSEGV"; break;
    case SIGTERM: signalName = (const char*) "SIGTERM"; break;
  }

  fflush(stderr);
  fflush(stdout);
  fprintf(stderr, "\n[!!!!]  Test Crashed! %s\n", signalName);
}

int ldk_run_tests(LDKTestCase* tests, unsigned int num_tests)
{ 
  signal(SIGABRT, internalOnSignal);
  signal(SIGFPE,  internalOnSignal);
  signal(SIGILL,  internalOnSignal);
  signal(SIGINT,  internalOnSignal);
  signal(SIGSEGV, internalOnSignal);
  signal(SIGTERM, internalOnSignal);

  int passed = 0; 
  for (unsigned int i = 0; i < num_tests; ++i)
  {
    fflush(stdout);

    int result = tests[i].func();
    if (result == 0)
    {
      printf(" [PASS]  %d/%d ..... %s \n", i+1, num_tests, tests[i].name);
      passed++;
    }
    else
    {
      printf(" [FAIL]  %d/%d ..... %s \n", i+1, num_tests, tests[i].name);
    }
  }
  printf("       %d/%d tests passed.\n", passed, num_tests);

  return passed != num_tests;
}
#endif

#endif // LDK_TEST_H
