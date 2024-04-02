/**
 *
 * hlist.h
 * 
 * Provides an expanding list of elements that are associated with a handle.
 *
 */

#ifndef LDK_HANDLE_LIST_H
#define LDK_HANDLE_LIST_H

#include "common.h"
#include "array.h"
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKSlotInfo_t LDKSlotInfo;

  typedef struct
  {
    LDKArray*     slots;        // An array of slot information. It maps slots to the actual element address
    LDKArena      elements;     // a list of actual elements stored.
    uint32        elementCount; // how many valid elements are stored
    size_t        elementSize;
    uint32        firstFreeSlotIndex;
    uint32        freeSlotCount;
    LDKHandleType elementType;
  } LDKHList;

  typedef struct
  {
    LDKHList* hlist;
    void* ptr;        // pointer to the actual data
    size_t index;
  } LDKHListIterator;

  LDK_API bool ldkHListCreate(LDKHList* hlist, LDKHandleType type, size_t elementSize, int count);
  LDK_API LDKHandle ldkHListReserve(LDKHList* hlist);
  LDK_API bool ldkHListRemove(LDKHList* hlist, LDKHandle handle);
  LDK_API int ldkHListCount(const LDKHList* hlist);
  LDK_API byte* ldkHListLookup(LDKHList* hlist, LDKHandle handle);
  LDK_API void ldkHListReset(LDKHList* hlist);
  LDK_API bool ldkHListDestroy(LDKHList* hlist);
  LDK_API LDKTypeId ldkHandleType(LDKHandle handle);
  LDK_API LDKHListIterator ldkHListIteratorCreate(LDKHList* array);
  LDK_API bool  ldkHListIteratorHasNext(LDKHListIterator* it);
  LDK_API bool  ldkHListIteratorNext(LDKHListIterator* it);
  LDK_API void* ldkHListIteratorFirst(LDKHListIterator* it);
  LDK_API void* ldkHListIteratorLast(LDKHListIterator* it);

#ifdef __cplusplus
}
#endif

#endif  // LDK_HANDLE_LIST_H
