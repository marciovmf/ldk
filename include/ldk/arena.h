/**
 *
 * arena
 * 
 * An arena is a pre-allocated, fixed-size (but expandable) block of memory that is divided into smaller sized blocks called "chunks"".
 * Arenas are used to manage memory efficiently by reducing the overhead of frequent allocation and deallocation of small memory segments.
 * 
 */
#ifndef LDK_ARENA
#define LDK_ARENA

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
  } Arena;

  bool		ldkArenaCreate(Arena* arena, size_t size);
  void		ldkArenaDestroy(Arena* arena);
  byte*		ldkArenaAllocate(Arena* arena, size_t size);
  void		ldkArenaReset(Arena* arena);
  size_t	ldkArenaSizeGet(const Arena* arena);
  size_t	ldkArenaUsedGet(const Arena* arena);
  byte*   ldkArenaDataGet(const Arena* arena);
  void		ldkArenaReset(Arena* arena);

#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA
