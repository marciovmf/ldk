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

  typedef struct LDKChunk_t LDKChunk;

  typedef struct
  {
    LDKChunk* chunks;
    uint32 numChunks;
    size_t chunkCapacity;
  } LDKArena;

/**
 * ldkArenaAllocate
 * 
 * A macro that simplifies memory allocation from the arena for a specific type.
 * It automatically calculates the size of the type and calls `ldkArenaAllocateSize`
 * to allocate the appropriate amount of memory from the arena.
 *
 * @param arena Pointer to the arena from which memory is to be allocated.
 * @param type The data type for which memory is to be allocated.
 * @return Returns a pointer to the allocated memory for the given type, or NULL if allocation fails.
 */
#define ldkArenaAllocate(arena, type) ldkArenaAllocateSize(arena, sizeof(type))

/**
 * ldkArenaCreate
 * 
 * Initializes an arena with an initial chunk size. The arena can allocate memory
 * as needed from chunks, and more chunks are added dynamically if required.
 *
 * @param out Pointer to the arena to be initialized.
 * @param chunkSize The size of the initial chunk to allocate for the arena.
 * @return Returns true if the arena is successfully created, otherwise false.
 */
LDK_API bool ldkArenaCreate(LDKArena* out, size_t chunkSize);

/**
 * ldkArenaDestroy
 * 
 * Frees all memory used by the arena and deallocates all chunks. 
 * After calling this function, the arena is no longer valid and 
 * should not be used unless re-initialized.
 *
 * @param arena Pointer to the arena to be destroyed.
 */
LDK_API void ldkArenaDestroy(LDKArena* arena);

/**
 * ldkArenaAllocateSize
 * 
 * Allocates memory of the specified size from the current chunk in the arena.
 * If there isn't enough memory in the current chunk, a new chunk is allocated.
 *
 * @param arena Pointer to the arena from which memory is to be allocated.
 * @param size The amount of memory to allocate in bytes.
 * @return Returns a pointer to the allocated memory, or NULL if allocation fails.
 */
LDK_API uint8* ldkArenaAllocateSize(LDKArena* arena, size_t size);
/**
 * ldkArenaReset
 * 
 * Resets the arena by releasing all memory from all chunks, but keeps the 
 * allocated chunks ready for reuse. This allows future allocations without 
 * needing to destroy and recreate the arena, reducing allocation overhead.
 *
 * @param arena Pointer to the arena to be reset.
 */
LDK_API void ldkArenaReset(LDKArena* arena);


//
// Query per chunk information.
// These are unlikely to be used unless for debug/test code
//
LDK_API size_t ldkArenaGetChunkCpacity(LDKArena* arena, uint32 chunkIndex);
LDK_API size_t ldkArenaGetChunkUsed(LDKArena* arena, uint32 chunkIndex);
LDK_API uint8* ldkArenaGetChunkDataPtr(LDKArena* arena, uint32 chunkIndex);


#ifdef __cplusplus
}
#endif

#endif  // LDK_ARENA_H
