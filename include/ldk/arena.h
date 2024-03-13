/**
 *
 * arena.h
 * 
 * An arena is a pre-allocated, fixed-size (but expandable) block of memory that
 * can be manually managed and subdivided into smaller blocks as needed.
 * Arenas are used to manage memory efficiently by reducing the overhead of
 * frequent allocation and deallocation of small memory chunks.
 */

#ifndef LDK_ARENA_H
#define LDK_ARENA_H

#include "common.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    size_t bufferSize;
    size_t used;
    byte* data;
    bool initialized;
  } LDKArena;

#define ldkArenaAllocate(arena, type) ldkArenaAllocateSize(arena, sizeof(type))
#define ldkArenaFree(arena, type) ldkArenaFreeSize(arena, sizeof(type))

  LDK_API bool  ldkArenaCreate(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaDestroy(LDKArena* arena);
  LDK_API byte* ldkArenaAllocateSize(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaFreeSize(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaReset(LDKArena* arena);
  LDK_API size_t	ldkArenaSizeGet(const LDKArena* arena);
  LDK_API size_t	ldkArenaUsedGet(const LDKArena* arena);
  LDK_API byte* ldkArenaDataGet(const LDKArena* arena);
  LDK_API void  ldkArenaReset(LDKArena* arena);

#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA_H
