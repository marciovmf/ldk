#include "../include/ldk/common.h"
#include "../include/ldk/hlist.h"
#include <signal.h>
#include <stdint.h>
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"

int test_ldkHListCreate()
{
  LDKHList hlist;
  bool result = ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);

  if (!result) {
    printf("ldkHListCreate test failed\n");
    return 1;
  }

  if (hlist.elementSize != sizeof(int)) {
    printf("ldkHListCreate test failed: Incorrect element size\n");
    return 1;
  }
  return 0;
}

int test_ldkHListReserve()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);
  LDKHandle handle = ldkHListReserve(&hlist);
  ASSERT_NEQ_MSG(handle, LDK_HANDLE_INVALID, "HANDLE VALUE = %llX", handle);
  return 0;
}

int test_ldkHListRemove()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);
  LDKHandle handle = ldkHListReserve(&hlist);
  bool result = ldkHListRemove(&hlist, handle);
  if (!result)
  {
    printf("ldkHListRemove test failed\n");
    return 1;
  }
  return 0;
}

int test_ldkHListCount()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);
  LDKHandle h1 = ldkHListReserve(&hlist);
  LDKHandle h2 = ldkHListReserve(&hlist);
  LDKHandle h3 = ldkHListReserve(&hlist);
  ASSERT_EQ(ldkHListCount(&hlist), 3);

  ldkHListRemove(&hlist, h1);
  ASSERT_EQ(ldkHListCount(&hlist), 2);

  ldkHListRemove(&hlist, h2);
  ASSERT_EQ(ldkHListCount(&hlist), 1);

  ldkHListRemove(&hlist, h3);
  ASSERT_EQ(ldkHListCount(&hlist), 0);

  ldkHListReserve(&hlist);
  ldkHListReserve(&hlist);
  ldkHListReserve(&hlist);
  ASSERT_EQ(ldkHListCount(&hlist), 3);

  return 0;
}

int test_ldkHListIterator()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);
  // Populate the hlist with some elements (mocked)

  // Add elements
  LDKHandle h1 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h1)) = 10;

  LDKHandle h2 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h2)) = 20;

  LDKHandle h3 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h3)) = 30;

  LDKHandle h4 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h4)) = 40;

  LDKHandle h5 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h5)) = 50;

  // remove some elements

  ldkHListRemove(&hlist, h2);
  ldkHListRemove(&hlist, h3);
  ldkHListRemove(&hlist, h4);

  ASSERT_EQ(ldkHListCount(&hlist), 2);

  LDKHandle h6 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h6)) = 60;

  LDKHandle h7 = ldkHListReserve(&hlist);
  *((int*)ldkHListLookup(&hlist, h7)) = 70;

  ASSERT_EQ(ldkHListCount(&hlist), 4);

  // Test the iterator
  LDKHListIterator it = ldkHListIteratorCreate(&hlist);
  uint32 count = 0;
  while (ldkHListIteratorNext(&it))
  {
    int* value = (int*) ldkHListIteratorCurrent(&it);
    ASSERT_TRUE(*value == 10 || *value == 50 || *value == 60 || *value == 70);
    count++;
  }
  ASSERT_TRUE(count == 4);
  return 0;
}

int test_ldkHListReset()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 5);
  LDKHandle handles[10];

  // Add 10 elements on a HList of capacity 5 and expect it to expand.
  for(uint32 i = 0; i < 10; i++)
  {
    handles[i] = ldkHListReserve(&hlist);
    ASSERT_TRUE_MSG(handles[i] != LDK_HANDLE_INVALID, "when i = %d", i);
    int* ptrValue = (int*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    *ptrValue = 10 * i;
  }
  ASSERT_TRUE(hlist.count == 10);

  // Make sure all data remains
  for (uint32 i = 0; i < 10; i++)
  {
    int32* ptrValue = (int32*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    ASSERT_TRUE(*ptrValue == i * 10);
  }

  // Reset
  ldkHListReset(&hlist);
  ASSERT_TRUE(ldkHListCount(&hlist) == 0);

  // Make sure all handles are now INVALID
  for (uint32 i = 0; i < 10; i++)
  {
    int32* ptrValue = (int32*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue == NULL);
  }

  // Add data again
  for(uint32 i = 0; i < 10; i++)
  {
    handles[i] = ldkHListReserve(&hlist);
    ASSERT_TRUE_MSG(handles[i] != LDK_HANDLE_INVALID, "when i = %d", i);
    int* ptrValue = (int*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    *ptrValue = 10 * i;
  }

  // Make sure all handles are valid and data is correct
  for (uint32 i = 0; i < 10; i++)
  {
    int32* ptrValue = (int32*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    ASSERT_TRUE(*ptrValue == i * 10);
  }

  ldkHListDestroy(&hlist);
  return 0;



}

int test_invalidHandle()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 10);
  LDKHandle h1 = ldkHListReserve(&hlist);
  int* ptrValue = (int*) ldkHListLookup(&hlist, h1);
  *ptrValue = 42;
  ASSERT_TRUE(ptrValue != NULL);
  ldkHListRemove(&hlist, h1);
  ptrValue = (int*) ldkHListLookup(&hlist, h1);
  ASSERT_TRUE(ptrValue == NULL);
  ldkHListDestroy(&hlist);
  return 0;
}

int test_forceResize()
{
  LDKHList hlist;
  ldkHListCreate(&hlist, typeid(int), sizeof(int), 5);
  LDKHandle handles[10];

  // Add 10 elements on a HList of capacity 5 and expect it to expand.
  for(uint32 i = 0; i < 10; i++)
  {
    handles[i] = ldkHListReserve(&hlist);
    ASSERT_TRUE_MSG(handles[i] != LDK_HANDLE_INVALID, "when i = %d", i);
    int* ptrValue = (int*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    *ptrValue = 10 * i;
  }
  ASSERT_TRUE(hlist.count == 10);

  // Make sure all data remains
  for (uint32 i = 0; i < 10; i++)
  {
    int32* ptrValue = (int32*) ldkHListLookup(&hlist, handles[i]);
    ASSERT_TRUE(ptrValue != NULL);
    ASSERT_TRUE(*ptrValue == i * 10);
  }

  // Remove some elements in random order
  ldkHListRemove(&hlist, handles[9]);
  ldkHListRemove(&hlist, handles[7]);
  ldkHListRemove(&hlist, handles[0]);
  ldkHListRemove(&hlist, handles[4]);
  ldkHListRemove(&hlist, handles[5]);
  ldkHListRemove(&hlist, handles[6]);
  ldkHListRemove(&hlist, handles[2]);
  ldkHListRemove(&hlist, handles[1]);
  ASSERT_TRUE(hlist.count == 2);

  int32* ptrValue; 
  ptrValue = (int32*) ldkHListLookup(&hlist, handles[3]);
  ASSERT_TRUE(*ptrValue == 3 * 10);

  ptrValue = (int32*) ldkHListLookup(&hlist, handles[8]);
  ASSERT_TRUE(*ptrValue == 8 * 10);

  ldkHListDestroy(&hlist);
  return 0;
}

int main()
{
  LDKTestCase tests[] =
  {
    TEST_CASE(test_ldkHListCreate),
    TEST_CASE(test_ldkHListReserve),
    TEST_CASE(test_ldkHListRemove),
    TEST_CASE(test_ldkHListCount),
    TEST_CASE(test_ldkHListIterator),
    TEST_CASE(test_invalidHandle),
    TEST_CASE(test_ldkHListReset),
    TEST_CASE(test_forceResize),
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
