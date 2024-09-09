/**
 *
 * arena.h
 * 
 * An arena is a collection of Chunks. Memory from chunks can be requested and
 * manually managed in order to reduce the overhead of frequent small
 * allocations and deallocations.
 * More chunks are added to the arena as needed.
 *
 */

#ifndef LDK_ARENA_H
#define LDK_ARENA_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    uint8* data;
    size_t used;
    size_t capacity;
  } LDKChunk;

  typedef struct
  {
    LDKChunk* chunks;
    uint32 numChunks;
    size_t chunkCapacity;
  } LDKArena;


#define ldkArenaAllocate(arena, type) ldkArenaAllocateSize(arena, sizeof(type))

LDK_API  bool ldkArenaCreate(LDKArena* out, size_t chunkSize);
LDK_API  void ldkArenaDestroy(LDKArena* arena);
LDK_API  uint8* ldkArenaAllocateSize(LDKArena* arena, size_t size);   // gets memory from chunk
LDK_API  void ldkArenaReset(LDKArena* arena);


#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA_H
