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

#include <stdlib.h>
#include "common.h"

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

  bool		ldk_arena_initialize(Arena* arena, size_t size);
  void		ldk_arena_destroy(Arena* arena);
  byte*		ldk_arena_allocate(Arena* arena, size_t size);
  void		ldk_arena_reset(Arena* arena);
  size_t	ldk_arena_size_get(const Arena* arena);
  size_t	ldk_arena_used_get(const Arena* arena);
  byte*   ldk_arena_data_get(const Arena* arena);
  void		ldk_arena_reset(Arena* arena);

#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA
