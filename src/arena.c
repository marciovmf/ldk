#include "ldk/arena.h"
#include "ldk/os.h"
#include <math.h>
#include <stdio.h>

bool ldkArenaCreate(LDKArena* arena, size_t size)
{
  LDK_ASSERT(arena->initialized == false);
  LDK_ASSERT(size > 0);

  if (size <= 0 || !arena)
    return false;

  arena->bufferSize = size;
  arena->used = 0;
  arena->data = (byte*) ldkOsMemoryAlloc(size);
  arena->initialized = true;
  return arena->data != NULL;
}

void ldkArenaDestroy(LDKArena* arena)
{
  LDK_ASSERT(arena->initialized);

  ldkOsMemoryFree(arena->data); 
  arena->bufferSize = 0;
  arena->used = 0;
  arena->data = NULL;
}

byte* ldkArenaAllocateSize(LDKArena* arena, size_t size)
{
  LDK_ASSERT(arena->initialized);
  if (arena->used + size >= arena->bufferSize)
  {
    size_t newCapacity = 2 * (arena->bufferSize + size);
    // get next pow2 larger than current capacity
    newCapacity = (newCapacity >> 1) | newCapacity;
    newCapacity = (newCapacity >> 2) | newCapacity;
    newCapacity = (newCapacity >> 4) | newCapacity;
    newCapacity = (newCapacity >> 8) | newCapacity;
    newCapacity = (newCapacity >> 16) | newCapacity;
    newCapacity = (newCapacity >> 32) | newCapacity;
    newCapacity++;
    arena->data = (byte*) ldkOsMemoryResize(arena->data, newCapacity);
    arena->bufferSize = newCapacity;
  }

  byte* memPtr = arena->data + arena->used;
  arena->used += size;
  return memPtr;
}

void ldkArenaFreeSize(LDKArena* arena, size_t size)
{
  LDK_ASSERT(arena->initialized);
  LDK_ASSERT(size <= arena->used);
  LDK_ASSERT(size <= arena->bufferSize);
  arena->used -= size;
}

void ldkArenaReset(LDKArena* arena)
{
  LDK_ASSERT(arena->initialized);
  arena->used = 0;
}

size_t ldkArenaSizeGet(const LDKArena* arena)
{
  LDK_ASSERT(arena->initialized);
  return arena->bufferSize;
}

size_t ldkArenaUsedGet(const LDKArena* arena)
{
  LDK_ASSERT(arena->initialized);
  return arena->used;
}

byte* ldkArenaDataGet(const LDKArena* arena)
{
  LDK_ASSERT(arena->initialized);
  return arena->data;
}
