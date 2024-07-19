#include "../include/ldk/common.h"
#include "../include/ldk/hlist.h"
#include <signal.h>
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"


int test_ldkArenaCreate()
{
    LDKArena arena;
    size_t chunkSize = 1024;
    bool result = ldkArenaCreate(&arena, chunkSize);
    ASSERT_TRUE(result == true);
    ASSERT_TRUE(arena.numChunks == 1);
    ASSERT_TRUE(arena.chunkCapacity == chunkSize);
    ASSERT_TRUE(arena.chunks[0].capacity == chunkSize);
    ASSERT_TRUE(arena.chunks[0].used == 0);
    ASSERT_TRUE(arena.chunks[0].data != NULL);
    ldkArenaDestroy(&arena);
    return 0;
}

int test_ldkArenaDestroy()
{
    LDKArena arena;
    ldkArenaCreate(&arena, 1024);
    ldkArenaDestroy(&arena);
    ASSERT_TRUE(arena.chunks == NULL);
    ASSERT_TRUE(arena.numChunks == 0);
    ASSERT_TRUE(arena.chunkCapacity == 0);
    return 0;
}

int test_ldkArenaAllocateSize()
{
    LDKArena arena;
    ldkArenaCreate(&arena, 1024);
    
    uint8* mem1 = ldkArenaAllocateSize(&arena, 128);
    ASSERT_TRUE(mem1 != NULL);
    ASSERT_TRUE(arena.chunks[0].used == 128);

    uint8* mem2 = ldkArenaAllocateSize(&arena, 512);
    ASSERT_TRUE(mem2 != NULL);
    ASSERT_TRUE(arena.chunks[0].used == 640);

    uint8* mem3 = ldkArenaAllocateSize(&arena, 512);
    ASSERT_TRUE(mem3 != NULL);

    ASSERT_TRUE(arena.numChunks == 2);
    ASSERT_TRUE(arena.chunks[0].used == 640);
    ASSERT_TRUE(arena.chunks[1].used == 512);
    
    ldkArenaDestroy(&arena);
    return 0;
}

int test_ldkArenaReset()
{
    LDKArena arena;
    ldkArenaCreate(&arena, 1024);
    
    ldkArenaAllocateSize(&arena, 128);
    ldkArenaAllocateSize(&arena, 512);

    ldkArenaReset(&arena);
    ASSERT_TRUE(arena.numChunks == 1);
    ASSERT_TRUE(arena.chunks[0].used == 0);

    ldkArenaAllocateSize(&arena, 512);
    ldkArenaAllocateSize(&arena, 512);
    ASSERT_TRUE(arena.numChunks == 1);
    ASSERT_TRUE(arena.chunks[0].used == 1024);
    
    ldkArenaDestroy(&arena);
    return 0;
}


int main()
{
  LDKTestCase tests[] =
  {
    TEST_CASE(test_ldkArenaCreate),
    TEST_CASE(test_ldkArenaDestroy),
    TEST_CASE(test_ldkArenaAllocateSize),
    TEST_CASE(test_ldkArenaReset),
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
