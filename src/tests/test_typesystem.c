#include "../include/ldk/common.h"
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"
#include <string.h>

int test_typeId()
{
  LDKTypeId typeIdInt = typeid(int);
  LDKTypeId typeIdFloat = typeid(float);
  LDKTypeId typeIdChar = typeid(char);

  ASSERT_TRUE(typeIdInt != LDK_TYPE_ID_UNKNOWN);
  ASSERT_TRUE(typeIdFloat != LDK_TYPE_ID_UNKNOWN);
  ASSERT_TRUE(typeIdChar != LDK_TYPE_ID_UNKNOWN);
  ASSERT_TRUE(typeIdInt != typeIdChar && typeIdChar != typeIdFloat && typeIdInt != typeIdFloat);

  // Make sure types are not redefined
  ASSERT_TRUE(typeIdInt == typeid(int));
  ASSERT_TRUE(typeIdFloat == typeid(float));
  ASSERT_TRUE(typeIdChar == typeid(char));
  return 0;
}

int test_typeName()
{
  LDKTypeId typeIdInt = typeid(int);
  ASSERT_TRUE(strncmp(typename(typeIdInt), "int", 3) == 0);
  LDKTypeId typeIdFloat = typeid(float);
  ASSERT_TRUE(strncmp(typename(typeIdFloat), "float", 4) == 0);
  LDKTypeId typeIdChar = typeid(char);
  ASSERT_TRUE(strncmp(typename(typeIdChar), "char", 4) == 0);
  return 0;
}

int test_typeSize()
{
  LDKTypeId typeIdInt = typeid(int);
  ASSERT_TRUE(typesize(typeIdInt) == sizeof(int));
  LDKTypeId typeIdFloat = typeid(float);
  ASSERT_TRUE(typesize(typeIdFloat) == sizeof(float));
  LDKTypeId typeIdChar = typeid(char);
  ASSERT_TRUE(typesize(typeIdChar) == sizeof(char));
  return 0;
}

#define LDK_TYPE_SYSTEM_MAX_TYPES 64

int main()
{
  LDKTestCase tests[] = {
    TEST_CASE(test_typeId),
    TEST_CASE(test_typeSize),
    TEST_CASE(test_typeName),
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
