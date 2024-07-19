#include "../include/ldk/array.h"
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"

int test_ldkArrayCreate() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  ASSERT_TRUE(arr != NULL);
  ASSERT_TRUE(ldkArrayCapacity(arr) == 10);
  ASSERT_TRUE(ldkArrayCount(arr) == 0);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayAdd() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int value = 5;
  ldkArrayAdd(arr, &value);
  ASSERT_TRUE(ldkArrayCount(arr) == 1);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 0) == value);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayInsert() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int value1 = 5;
  int value2 = 10;
  ldkArrayAdd(arr, &value1);
  ldkArrayInsert(arr, &value2, 0);
  ASSERT_TRUE(ldkArrayCount(arr) == 2);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 0) == value2);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 1) == value1);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayGet() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int value = 5;
  ldkArrayAdd(arr, &value);
  int* result = (int*)ldkArrayGet(arr, 0);
  ASSERT_TRUE(result != NULL);
  ASSERT_TRUE(*result == value);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayGetData() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int value = 5;
  ldkArrayAdd(arr, &value);
  int* data = (int*)ldkArrayGetData(arr);
  ASSERT_TRUE(data != NULL);
  ASSERT_TRUE(data[0] == value);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayCount() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  ASSERT_TRUE(ldkArrayCount(arr) == 0);
  int value = 5;
  ldkArrayAdd(arr, &value);
  ASSERT_TRUE(ldkArrayCount(arr) == 1);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayCapacity() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  ASSERT_TRUE(ldkArrayCapacity(arr) == 10);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayDeleteRange() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int values[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++) {
    ldkArrayAdd(arr, &values[i]);
  }
  ldkArrayDeleteRange(arr, 1, 3);
  ASSERT_TRUE(ldkArrayCount(arr) == 2);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 0) == 1);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 1) == 5);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayClear() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int value = 5;
  ldkArrayAdd(arr, &value);
  ldkArrayClear(arr);
  ASSERT_TRUE(ldkArrayCount(arr) == 0);
  ldkArrayDestroy(arr);
  return 0;
}

int test_ldkArrayDeleteAt() {
  LDKArray* arr = ldkArrayCreate(sizeof(int), 10);
  int values[] = {1, 2, 3};
  for (int i = 0; i < 3; i++) {
    ldkArrayAdd(arr, &values[i]);
  }
  ldkArrayDeleteAt(arr, 1);
  ASSERT_TRUE(ldkArrayCount(arr) == 2);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 0) == 1);
  ASSERT_TRUE(*(int*)ldkArrayGet(arr, 1) == 3);
  ldkArrayDestroy(arr);
  return 0;
}

int main()
{
  LDKTestCase tests[] = {
    TEST_CASE(test_ldkArrayCreate),
    TEST_CASE(test_ldkArrayAdd),
    TEST_CASE(test_ldkArrayInsert),
    TEST_CASE(test_ldkArrayGet),
    TEST_CASE(test_ldkArrayGetData),
    TEST_CASE(test_ldkArrayCount),
    TEST_CASE(test_ldkArrayCapacity),
    TEST_CASE(test_ldkArrayDeleteRange),
    TEST_CASE(test_ldkArrayClear),
    TEST_CASE(test_ldkArrayDeleteAt),
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
