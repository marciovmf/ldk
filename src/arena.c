#include "ldk/arena.h"
#include "ldk/os.h"
#include <math.h>
#include <stdio.h>

bool ldkArenaCreate(Arena* arena, size_t size)
{
  LDK_ASSERT(arena->initialized == false);
  LDK_ASSERT(size > 0);

  if (size <= 0 || !arena)
    return false;

  arena->bufferSize = size;
  arena->used = 0;
  arena->data = (byte*) ldkOsMemoryAlloc(size);
  arena->initialized = true;
  return arena->data != nullptr;
}

void ldkArenaDestroy(Arena* arena)
{
  //TODO(marcio): Assert if not initialized
  ldkOsMemoryFree(arena->data); 
  arena->bufferSize = 0;
  arena->used = 0;
  arena->data = nullptr;
}

byte* ldkArenaAllocate(Arena* arena, size_t size)
{
  LDK_ASSERT(arena->initialized == true);
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

void ldkArenaReset(Arena* arena)
{
  //TODO(marcio): Assert if not initialized
  arena->used = 0;
}

size_t ldkArenaSizeGet(const Arena* arena)
{
  //TODO(marcio): Assert if not initialized
  return arena->bufferSize;
}

size_t ldkArenaUsedGet(const Arena* arena)
{
  //TODO(marcio): Assert if not initialized
  return arena->used;
}

byte* ldkArenaDataGet(const Arena* arena)
{
  //TODO(marcio): Assert if not initialized
  return arena->data;
}
