#include "ldk/arena.h"
#include "ldk/os.h"
#include <math.h>
#include <stdio.h>

#ifndef ARENA_ALLOCATE
#define ARENA_ALLOCATE(size) ldkOsMemoryAlloc(size)
#endif // ARENA_ALLOCATE


#ifndef ARENA_FREE
#define ARENA_FREE(mem) ldkOsMemoryFree(mem)
#endif // ARENA_FREE


#ifndef ARENA_REALLOC
#define ARENA_REALLOC(mem, size) ldkOsMemoryResize(mem, size)
#endif // ARENA_REALLOC

//
// Internal functions
//

// Adds a new chunk to the arena and updates the arena chunk information
static LDKChunk* s_arenaAddLDKChunk(LDKArena* arena, size_t size)
{
  uint32 chunkIndex = arena->numChunks++;
  arena->chunks = (LDKChunk*) ARENA_REALLOC(arena->chunks, sizeof(LDKChunk) * arena->numChunks);
  arena->chunks[chunkIndex].data = (uint8*) ARENA_ALLOCATE(arena->chunkCapacity);
  arena->chunks[chunkIndex].capacity = arena->chunkCapacity;
  arena->chunks[chunkIndex].used = 0;

  if (arena->chunkCapacity < size)
    arena->chunkCapacity = size;

  return &arena->chunks[chunkIndex];
}

static uint8* s_arenaAllocateFromLDKChunk(LDKChunk* chunk, size_t size)
{
  if (!chunk)
    return NULL;

  size_t freeSpace = chunk->capacity - chunk->used;
  if (size > freeSpace)
    return NULL;

  uint8* memory = chunk->data + chunk->used;
  chunk->used += size;
  return memory;
}

//
// Public functions
//

bool ldkArenaCreate(LDKArena* out, size_t capacity)
{
  out->numChunks = 0;
  out->chunkCapacity = capacity;
  out->chunks = NULL;
  return s_arenaAddLDKChunk(out, capacity) != NULL;
}

void ldkArenaDestroy(LDKArena* out)
{
  for (uint32 i = 0; i < out->numChunks; i++)
  {
    ARENA_FREE((void*) out->chunks[i].data);
  }

  ARENA_FREE((void*) out->chunks);
  out->chunks = NULL;
  out->numChunks = 0;
  out->chunkCapacity = 0;
}

//
// Allocates size from existing chunks. If no chunk has enough space to fit the
// allocation, a new chunck with enough space is allocated.
// An allocation is never split across multiple chunks.
//
//
uint8* ldkArenaAllocateSize(LDKArena* arena, size_t size)
{
  // Can we find space in any existing chunk ?
  for (uint32 i = 0; i < arena->numChunks; i++)
  {
    LDKChunk* chunk = &arena->chunks[i];
    size_t freeSpace = chunk->capacity - chunk->used;

    if (freeSpace >= size)
    {
      chunk = &arena->chunks[i];
      return s_arenaAllocateFromLDKChunk(chunk, size);
    }
  }

  // We need to allocate a new chunk.
  // Should the new chunk be larger ?
  size_t newSize = arena->chunkCapacity;
  if (size > arena->chunkCapacity)	
    newSize = arena->chunkCapacity + size;

  LDKChunk* chunk = s_arenaAddLDKChunk(arena, newSize);
  return s_arenaAllocateFromLDKChunk(chunk, size);
}

void ldkArenaReset(LDKArena* arena)
{
  for (uint32 i = 0; i < arena->numChunks; i++)
  {
    arena->chunks[i].used = 0;
  }
}
