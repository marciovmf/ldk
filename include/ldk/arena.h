/**
 *
 * arena.h
 * 
 * An arena is a pre-allocated, fixed-size (but expandable) block of memory that is divided into smaller sized blocks called "chunks"".
 * Arenas are used to manage memory efficiently by reducing the overhead of frequent allocation and deallocation of small memory segments.
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

  LDK_API bool  ldkArenaCreate(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaDestroy(LDKArena* arena);
  LDK_API byte* ldkArenaAllocate(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaFree(LDKArena* arena, size_t size);
  LDK_API void  ldkArenaReset(LDKArena* arena);
  LDK_API size_t	ldkArenaSizeGet(const LDKArena* arena);
  LDK_API size_t	ldkArenaUsedGet(const LDKArena* arena);
  LDK_API byte* ldkArenaDataGet(const LDKArena* arena);
  LDK_API void  ldkArenaReset(LDKArena* arena);

#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA_H
