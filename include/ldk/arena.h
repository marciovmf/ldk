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

  typedef uint32_t uint32;
  typedef int32_t int32;
  typedef uint8_t uint8;
  typedef int8_t int8;

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

  bool ldkArenaCreate(LDKArena* out, size_t chunkSize);
  void ldkArenaDestroy(LDKArena* arena);
  uint8* ldkArenaAllocateSize(LDKArena* arena, size_t size);   // gets memory from chunk
  void ldkArenaReset(LDKArena* arena);


#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA_H
