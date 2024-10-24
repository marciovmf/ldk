#include "../include/ldk/maths.h"
#include "common.h"
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"

int test_floats()
{
  ASSERT_TRUE(floatIsZero(0.000000f));
  ASSERT_TRUE(floatIsPositive(0.000000f));
  ASSERT_TRUE(floatIsPositive(0.000001f));
  ASSERT_TRUE(floatIsNegative(-0.00001f));
  ASSERT_TRUE(floatsAreEqual(-0.00201f, -0.00201f));
  ASSERT_TRUE(floatIsGreaterThan(0.23451f, 0.2345f));
  ASSERT_TRUE(floatIsLessThan(0.2345f, 0.23451f));
  return 0;
}

int test_lerp()
{
  float f1 = 0.0f;
  float f2 = 1.0f;
  float result;
  
  result = lerp(f1, f2, 0.0f);
  ASSERT_TRUE(floatIsZero(result));

  result = lerp(f1, f2, 1.0f);
  ASSERT_TRUE(floatsAreEqual(result, f2));

  result = lerp(f1, f2, 0.5f);
  ASSERT_TRUE(between(result, f1, f2));

  ASSERT_TRUE(floatIsGreaterThan(result, f1));
  ASSERT_TRUE(floatIsLessThan(result, f2));
  return 0;
}


int main()
{
  LDKTestCase tests[] =
  {
    TEST_CASE(test_floats),
    TEST_CASE(test_lerp),
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
