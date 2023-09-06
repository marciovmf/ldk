
#include <stdint.h>
#include <memory.h>
#include "../include/ldk/hlist.h"


/* Internals */

// A SlotInfo holds the index of the actual element in a HandleList
typedef struct
{
  union
  {
    uint32 elementIndex;        // Index of the element this slot points to
    uint32 nextFreeSlotIndex;   // Index to the next free slot.
  };

  int32 version;
} SlotInfo;

// A struct that holds the same information as a Handle.
typedef struct
{
  HandleType  type;
  uint16      version;
  uint32      slotIndex;
} HandleInternal;

inline static Handle handle_encode(HandleInternal* hInternal)
{
  Handle handle = ((uint64_t) hInternal->type << 48) | ((uint64_t) hInternal->version << 32) | hInternal->slotIndex;
  return handle;
}

inline static void handle_decode(HandleInternal* hInternal, Handle handle)
{
  hInternal->type       = (uint16_t)((handle >> 48) & 0xFFFF);
  hInternal->version    = (uint16_t)((handle >> 32) & 0xFFFF);
  hInternal->slotIndex  = (uint32_t)(handle & 0xFFFFFFFF);
}


bool hlist_initialize(HandleList* hlist, HandleType type, size_t elementSize, int count)
{
  hlist->elementType       = type;
  hlist->elementSize       = elementSize;
  hlist->elementCount      = 0;
  hlist->freeSlotListCount = 0;
  hlist->freeSlotListStart = 0;
  bool success = true;
  success &= ldk_arena_initialize(&hlist->elements,  count * elementSize);
  success &= ldk_arena_initialize(&hlist->slots,      count * sizeof(SlotInfo));
  return success;
}

int hlist_count_get(const HandleList* hlist)
{
  return hlist->elementCount;
}

byte* hlist_get_array(const HandleList* hlist)
{
  return ldk_arena_data_get(&hlist->elements);
}

Handle hlist_reserve(HandleList* hlist)
{
  SlotInfo* slotPtr;
  hlist->elementCount++;

  if(hlist->freeSlotListCount)
  {
    // Reuse a slot.
    hlist->freeSlotListCount--;
    slotPtr = ((SlotInfo*) ldk_arena_data_get(&hlist->slots)) + hlist->freeSlotListStart; 
    hlist->freeSlotListStart = slotPtr->nextFreeSlotIndex;
  }
  else
  {
    // Add a new slot and a new element
    slotPtr = (SlotInfo*) ldk_arena_allocate(&hlist->slots, sizeof(SlotInfo));
    slotPtr->version = 0;
    ldk_arena_allocate(&hlist->elements, hlist->elementSize);
  }

  slotPtr->elementIndex = hlist->elementCount - 1;

  // gets the element this slot points to
  byte* element = ((byte*) ldk_arena_data_get(&hlist->elements)) + slotPtr->elementIndex;

  // Create a handle to the element
  HandleInternal hInternal;
  hInternal.type = hlist->elementType;

  // Get the index of current slot based on it's address.
  hInternal.slotIndex = ((int32)((byte*) (slotPtr) - (byte*) ldk_arena_data_get(&hlist->slots)) / sizeof(SlotInfo));
  hInternal.version = slotPtr->version;
  return handle_encode(&hInternal);
}

byte* hlist_lookup(HandleList* hlist, Handle handle)
{
  HandleInternal hInternal;
  handle_decode(&hInternal, handle);

  // Index out of bounds ?
  const uint32 maxSlots = (uint32) (ldk_arena_size_get(&hlist->slots) / sizeof(SlotInfo));
  if (hInternal.slotIndex >= maxSlots)
    return nullptr;

  // Wrong handle type ?
  if (hInternal.type != hlist->elementType)
    return nullptr;

  // Wrong handle version ?
  SlotInfo* slotInfo = ((SlotInfo*) ldk_arena_data_get(&hlist->slots)) + hInternal.slotIndex;
  if (hInternal.version != slotInfo->version)
    return nullptr;

  byte* element = ((byte*) ldk_arena_data_get(&hlist->elements)) + (hlist->elementSize * slotInfo->elementIndex);
  return element;
}

bool hlist_remove(HandleList* hlist, Handle handle)
{
  // We never leave holes on the element list!
  // When deleting any element (other than the last one) we actually
  // move the last element to the place of the one being deleted
  // and fix the slot so it points to the correct element index.
  // The slot version will be incremented so any existing handle to it will
  // now become invalid.

  HandleInternal hInternal;
  handle_decode(&hInternal, handle);

  // Index out of bounds ?
  const uint32 maxSlots = (uint32) (ldk_arena_size_get(&hlist->slots) / sizeof(SlotInfo));
  if (hInternal.slotIndex >= maxSlots || hInternal.slotIndex < 0)
    return false;

  // Wrong handle type ?
  if (hInternal.type != hlist->elementType)
    return false;

  const uint32 elementCount = hlist->elementCount;
  SlotInfo* allSlots = (SlotInfo*) ldk_arena_data_get(&hlist->slots);
  SlotInfo* slotOfLast    = allSlots + (elementCount-1);
  SlotInfo* slotOfRemoved = allSlots + hInternal.slotIndex;
  ++slotOfRemoved->version;

  if (slotOfRemoved != slotOfLast)
  {
    char* dataPtr = (char*) ldk_arena_data_get(&hlist->elements);
    const size_t elementSize = hlist->elementSize;

    // Move the last element to the space left by the one being removed
    void* elementLast    = dataPtr + (slotOfLast->elementIndex * elementSize);
    void* elementRemoved = dataPtr + (slotOfRemoved->elementIndex * elementSize);
    memcpy(elementRemoved, elementLast, elementSize);
  }

  slotOfRemoved->nextFreeSlotIndex = hlist->freeSlotListStart;
  hlist->freeSlotListStart = hInternal.slotIndex;
  hlist->freeSlotListCount++;
  hlist->elementCount--;
  return true;
}

void hlist_reset(HandleList* hlist)
{
  // invalidate ALL slots
  for (uint32 i = 0; i < hlist->elementCount; i++)
  {
    SlotInfo* slot = ((SlotInfo*) ldk_arena_data_get(&hlist->slots)) + i;
    slot->version++;
  }

  ldk_arena_reset(&hlist->slots);
  ldk_arena_reset(&hlist->elements);
  hlist->elementCount = 0;
  hlist->freeSlotListCount = 0;
  hlist->freeSlotListStart = -1;
}

