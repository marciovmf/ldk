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

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKSlot_t LDKSlot;

  typedef struct
  {
    size_t elementSize;
    uint32 capacity;
    uint32 count;
    byte *data;
    int32 *freeSlots;
    int32 *dataToSlotMap;
    int32 freeCount;
    LDKHandleType type;
    LDKSlot* slots;
  } LDKHList;

typedef struct
{
  LDKHList *hlist;
  int32 current;
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
  LDK_API void* ldkHListIteratorCurrent(LDKHListIterator* it);

#ifdef __cplusplus
}
#endif

#endif  // LDK_HANDLE_LIST_H
