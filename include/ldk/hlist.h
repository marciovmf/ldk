/**
 *
 * hlist.h
 * 
 * Provides an expanding list of elements that are associated with a handle.
 * Elements are always sequentialy placed in memory.
 */

#ifndef LDK_HANDLE_LIST_H
#define LDK_HANDLE_LIST_H

#include "common.h"
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    LDKArena      slots;        // a list of slot information. It maps a slot index to an element index.
    LDKArena      revSlots;     // a list of indexes to the slots list. It maps an element index to a slot index.
    LDKArena      elements;     // a list of actual elements stored.
    size_t        elementSize;
    uint32        elementCount;
    uint32        freeSlotListCount;
    uint32        freeSlotListStart;
    LDKHandleType elementType;
  } LDKHList;

  /*
   * Initializes a LDKHList.
   * elementSize: is the size of the type of element stored in this LDKHList
   * count: How many elements this LDKHList can store before without resizing the arenas.
   */
  LDK_API bool ldkHListCreate(LDKHList* hlist, LDKHandleType type, size_t elementSize, int count);

  /*
   * Atempts to remove a element from the list.
   * Returs the true if the handle is valid and deletion succeeded or false if handle is invalid
   */
  LDK_API bool ldkHListRemove(LDKHList* hlist, LDKHandle handle);

  /*
   * Reserves memory from the internal arena for a new element and returns a handle to it.
   */
  LDK_API LDKHandle ldkHListReserve(LDKHList* hlist);

  /*
   * Returns the number of elements in the LDKHList
   */
  LDK_API int ldkHListCount(const LDKHList* hlist);

  /*
   * Returns the a pointer to the list of elements. 
   * All elements are sequentialy placed in the list.
   * The order of the elements is undefined.
   */
  LDK_API byte* ldkHListArrayGet(const LDKHList* hlist);

  /*
   * Returns the address of the element associated to the Handle
   */
  LDK_API byte* ldkHListLookup(LDKHList* hlist, LDKHandle handle);

  /*
   * Resets the hlist counters and internal arenas.
   * Any existing handle obtained from this LDKHList will become invalid.
   */
  LDK_API void ldkHListReset(LDKHList* hlist);

  /*
   * Frees all memory allocated for the list.
   * Any existing handle obtained from this LDKHList will become invalid.
   */
  LDK_API bool ldkHListDestroy(LDKHList* hlist);


  LDK_API LDKTypeId ldkHandleType(LDKHandle handle);

#ifdef __cplusplus
}
#endif

#endif  // LDK_HANDLE_LIST_H
