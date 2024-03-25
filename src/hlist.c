#include "ldk/hlist.h"
#include <memory.h>


/* Internals */

// A SlotInfo holds the index of the actual element in a LDKHandleList
typedef struct
{
  union
  {
    uint32 elementIndex;        // Index of the element this slot points to
    uint32 nextFreeSlotIndex;   // Index to the next free slot.
  };

  int32 version;
} SlotInfo;

// A struct that holds the same information as a LDKHandle.
typedef struct
{
  uint32      slotIndex;
  uint16      version;
  LDKHandleType  type; //16bit
} LDKHandleInfo;

inline static LDKHandle handle_encode(LDKHandleInfo* hInternal)
{
  LDKHandle handle = ((uint64_t) hInternal->version << 48) | ((uint64_t) hInternal->slotIndex << 16) | hInternal->type;
  return handle;
}

inline static void handle_decode(LDKHandleInfo* hInfo, LDKHandle handle)
{
  hInfo->type       = (uint16)(handle & 0xFFFF);
  hInfo->slotIndex  = (uint32_t)((handle >> 16) & 0xFFFFFFFF);
  hInfo->version    = (uint16_t)((handle >> 48) & 0xFFFF);

}

bool ldkHListCreate(LDKHList* hlist, LDKHandleType type, size_t elementSize, int count)
{
  hlist->elementType       = type; // We just store the type to validate handles
  hlist->elementSize       = elementSize;
  hlist->elementCount      = 0;
  hlist->freeSlotListCount = 0;
  hlist->freeSlotListStart = 0;
  bool success = true;
  success &= ldkArenaCreate(&hlist->elements, count * elementSize);
  success &= ldkArenaCreate(&hlist->slots, count * sizeof(SlotInfo));
  success &= ldkArenaCreate(&hlist->revSlots, count * sizeof(uint32));
  return success;
}

int ldkHListCount(const LDKHList* hlist)
{
  return hlist->elementCount;
}

byte* ldkHListArrayGet(const LDKHList* hlist)
{
  return ldkArenaDataGet(&hlist->elements);
}

LDKHandle ldkHListReserve(LDKHList* hlist)
{
  // A slot is identified by its index. It's contet points to an element.
  // A revSlot (reverseSlot) is identitied by its index. It's conent points to a slot.
  // Reverse slots are becessary to allow us to find the the slot from an
  // arbitrary data index.

  SlotInfo* slotPtr;
  hlist->elementCount++;

  if(hlist->freeSlotListCount)
  {
    // Reuse a slot.
    hlist->freeSlotListCount--;
    slotPtr = ((SlotInfo*) ldkArenaDataGet(&hlist->slots)) + hlist->freeSlotListStart; 
    hlist->freeSlotListStart = slotPtr->nextFreeSlotIndex;
  }
  else
  {
    // Add a new slot and a new element
    slotPtr = (SlotInfo*) ldkArenaAllocate(&hlist->slots, SlotInfo);
    slotPtr->version = 0;
    ldkArenaAllocate(&hlist->elements, hlist->elementSize);
  }

  slotPtr->elementIndex = hlist->elementCount - 1;

  // Create a handle to the element
  LDKHandleInfo hInfo;
  hInfo.type = hlist->elementType;

  // Get the index of current slot based on it's address.
  hInfo.slotIndex = ((int32)((byte*) (slotPtr) - (byte*) ldkArenaDataGet(&hlist->slots)) / sizeof(SlotInfo));
  hInfo.version = slotPtr->version;

  // The reverse slot list keeps, at the same index as the data, the index of the slot pointing to the data.
  uint32* revSlot = ((uint32*) ldkArenaDataGet(&hlist->revSlots)) + slotPtr->elementIndex;
  *revSlot = hInfo.slotIndex;

  return handle_encode(&hInfo);
}

byte* ldkHListLookup(LDKHList* hlist, LDKHandle handle)
{
  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);

  // Index out of bounds ?
  const uint32 maxSlots = (uint32) (ldkArenaSizeGet(&hlist->slots) / sizeof(SlotInfo));
  if (hInfo.slotIndex >= maxSlots)
    return NULL;

  // Wrong handle type ?
  if (hInfo.type != hlist->elementType)
    return NULL;

  // Wrong handle version ?
  SlotInfo* slotInfo = ((SlotInfo*) ldkArenaDataGet(&hlist->slots)) + hInfo.slotIndex;
  if (hInfo.version != slotInfo->version)
    return NULL;

  byte* element = ((byte*) ldkArenaDataGet(&hlist->elements)) + (hlist->elementSize * slotInfo->elementIndex);
  return element;
}

bool ldkHListRemove(LDKHList* hlist, LDKHandle handle)
{
  // We never leave holes on the element list!
  // When deleting any element (other than the last one) we actually
  // move the last element to the place of the one being deleted
  // and fix the slot so it points to the correct element index.
  // The slot version will be incremented so any existing handle to it will
  // now become invalid.

  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);

  // Index out of bounds ?
  const uint32 maxSlots = (uint32) (ldkArenaSizeGet(&hlist->slots) / sizeof(SlotInfo));
  if (hInfo.slotIndex >= maxSlots || hInfo.slotIndex < 0)
    return false;

  // Wrong handle type ?
  if (hInfo.type != hlist->elementType)
    return false;

  SlotInfo* allSlots = (SlotInfo*) ldkArenaDataGet(&hlist->slots);
  uint32* allRevSlots = (uint32*) ldkArenaDataGet(&hlist->revSlots);
  uint32 indexOfSlotOfLast = *(allRevSlots + (hlist->elementCount - 1));
  SlotInfo* slotOfLast = allSlots + indexOfSlotOfLast;
  SlotInfo* slotOfRemoved = allSlots + hInfo.slotIndex;

  // Prevent deleting from an outdated handle
  if (slotOfRemoved->version != hInfo.version)
    return false;

  ++slotOfRemoved->version;

  if (slotOfRemoved != slotOfLast)
  {
    char* dataPtr = (char*) ldkArenaDataGet(&hlist->elements);
    const size_t elementSize = hlist->elementSize;

    // Move the last element to the space left by the one being removed
    void* elementLast    = dataPtr + (slotOfLast->elementIndex * elementSize);
    void* elementRemoved = dataPtr + (slotOfRemoved->elementIndex * elementSize);
    memcpy(elementRemoved, elementLast, elementSize);
    slotOfLast->elementIndex = slotOfRemoved->elementIndex;
    allRevSlots[slotOfRemoved->elementIndex] = indexOfSlotOfLast;
  }

  slotOfRemoved->nextFreeSlotIndex = hlist->freeSlotListStart;
  hlist->freeSlotListStart = hInfo.slotIndex; 
  hlist->freeSlotListCount++;
  hlist->elementCount--;
  return true;
}

void ldkHListReset(LDKHList* hlist)
{
  // invalidate ALL slots
  for (uint32 i = 0; i < hlist->elementCount; i++)
  {
    SlotInfo* slot = ((SlotInfo*) ldkArenaDataGet(&hlist->slots)) + i;
    slot->version++;
  }

  ldkArenaReset(&hlist->slots);
  ldkArenaReset(&hlist->elements);
  hlist->elementCount = 0;
  hlist->freeSlotListCount = 0;
  hlist->freeSlotListStart = -1;
}

bool ldkHListDestroy(LDKHList* hlist)
{
  ldkArenaDestroy(&hlist->slots);
  ldkArenaDestroy(&hlist->elements);
  hlist->elementCount = 0;
  hlist->freeSlotListCount = 0;
  hlist->freeSlotListStart = -1;
  return true;
}

LDKTypeId ldkHandleType(LDKHandle handle)
{
  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);
  return hInfo.type;
}
