/**
 *
 * hlist
 * 
 * Provides an expanding list of elements that are associated with a handle.
 * Elements are always sequentialy placed in memory.
 */

#ifndef SMOL_HANDLE_LIST_H
#define SMOL_HANDLE_LIST_H

#include "core/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

  /*
   * A list of elements identified by a Handle.
   * Elements are always linear in memory.
   */
  typedef struct
  {
    size_t      elementSize;
    Arena       slots;
    Arena       elements;
    uint32      elementCount;
    uint32      freeSlotListCount;
    uint32      freeSlotListStart;
    HandleType  elementType;
  } HandleList;

  /*
   * Initializes a HandleList.
   * elementSize: is the size of the type of element stored in this HandleList
   * count: How many elements this HandleList can store before without resizing the arenas.
   */
  bool hlist_initialize(HandleList* hlist, HandleType type, size_t elementSize, int count);

  /*
   * Atempts to remove a element from the list.
   * Returs the true if the handle is valid and deletion succeeded or false if handle is invalid
   */
  bool hlist_remove(HandleList* hlist, Handle handle);

  /*
   * Reserves memory from the internal arena for a new element and returns a handle to it.
   */
  Handle hlist_reserve(HandleList* hlist);

  /*
   * Returns the number of elements in the HandleList
   */
  int hlist_count_get(const HandleList* hlist);

  /*
   * Returns the a pointer to the list of elements. 
   * All elements are sequentialy placed in the list.
   * The order of the elements is undefined.
   */
  byte* hlist_get_array(const HandleList* hlist);

  /*
   * Returns the address of the element associated to the Handle
   */
  byte* hlist_lookup(HandleList* hlist, Handle handle);

  /*
   * Resets the hlist counters and internal arenas.
   * Any existing handle obtained from this HandleList will become invalid.
   */
  void hlist_reset(HandleList* hlist);

#ifdef __cplusplus
}
#endif

#endif  // SMOL_HANDLE_LIST_H
