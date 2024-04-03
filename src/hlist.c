#include "ldk/hlist.h"
#include <memory.h>

/* Internals */

// A SlotInfo holds the index of the actual element in a LDKHandleList

struct LDKSlotInfo_t
{
  bool used;                  // true if the slot is being used
  void* elementAddress;       // pointer to the actual element
  uint32 nextFreeSlotIndex;   // if this slot is free, this points to the index of the next free slot
  uint32 index;               // the index of this slot
  int32 version;
};

// A struct that holds the same information as a LDKHandle.
typedef struct
{
  uint32        slotIndex;
  uint16        version;
  LDKHandleType type; //16bit
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
  hlist->elementType        = type;
  hlist->elementSize        = elementSize;
  hlist->slots              = ldkArrayCreate(sizeof(LDKSlotInfo), count);
  hlist->freeSlotCount      = 0;
  hlist->firstFreeSlotIndex = 0;

  bool success  = ldkArenaCreate(&hlist->elements, count * elementSize);
  return success;
}

int ldkHListCount(const LDKHList* hlist)
{
  return hlist->elementCount;
}

LDKHandle ldkHListReserve(LDKHList* hlist)
{
  LDKHandleInfo handleInfo;

  // try to reuse a slot
  if (hlist->freeSlotCount > 0)
  {
    LDKSlotInfo* slotInfo = ldkArrayGet(hlist->slots, hlist->firstFreeSlotIndex);
    LDK_ASSERT(slotInfo->used == false);

    hlist->firstFreeSlotIndex = slotInfo->nextFreeSlotIndex;
    hlist->freeSlotCount--;

    handleInfo.version = slotInfo->version;
    handleInfo.slotIndex = slotInfo->index;
    handleInfo.type = hlist->elementType;
  }
  else
  {
    LDKSlotInfo newSlot;
    uint32 version = 0;

    uint32 slotCount = ldkArrayCount(hlist->slots);

    // If this hlist was reset, we might have more slots than items. In that
    // case, we want to keep the existing slot version.
    // We never override anything from an existing slot, except the version,
    if (hlist->elementCount < slotCount)
    {
      LDKSlotInfo* si = ldkArrayGet(hlist->slots, hlist->elementCount);
      LDK_ASSERT(si->used == false);
      version = si->version;
    }

    newSlot.version = version;
    newSlot.used = true;
    newSlot.nextFreeSlotIndex = 0;
    newSlot.elementAddress = ldkArenaAllocateSize(&hlist->elements, hlist->elementSize);
    newSlot.index = ldkArrayCount(hlist->slots);
    ldkArrayAdd(hlist->slots, &newSlot); 

    handleInfo.version = 0;
    handleInfo.slotIndex = newSlot.index;
    handleInfo.type = hlist->elementType;
  }

  hlist->elementCount++;
  return handle_encode(&handleInfo);
}

byte* ldkHListLookup(LDKHList* hlist, LDKHandle handle)
{
  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);

  // Index out of bounds ?
  const uint32 slotCount = ldkArrayCount(hlist->slots);
  if (hInfo.slotIndex >= slotCount)
    return NULL;

  // Wrong handle type ?
  if (hInfo.type != hlist->elementType)
    return NULL;

  // Wrong handle version ?
  LDKSlotInfo* slotInfo = ldkArrayGet(hlist->slots, hInfo.slotIndex);
  if (hInfo.version != slotInfo->version)
    return NULL;

  byte* element = (byte*) slotInfo->elementAddress;
  return element;
}

bool ldkHListRemove(LDKHList* hlist, LDKHandle handle)
{
  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);

  const uint32 slotCount = ldkArrayCount(hlist->slots);

  // Index out of bounds ?
  if (hInfo.slotIndex > slotCount)
    return false;

  // Wrong handle type ?
  if (hInfo.type != hlist->elementType)
    return false;

  LDKSlotInfo* slotInfo = ldkArrayGet(hlist->slots, hInfo.slotIndex);
  slotInfo->used = false;
  slotInfo->version++;
  slotInfo->nextFreeSlotIndex = hlist->firstFreeSlotIndex;

  hlist->firstFreeSlotIndex = hInfo.slotIndex;
  hlist->freeSlotCount++;
  hlist->elementCount--;
  return true;
}

void ldkHListReset(LDKHList* hlist)
{
  for (uint32 i = 0; i < hlist->elementCount; i++)
  {
    LDKSlotInfo* slot = ((LDKSlotInfo*) ldkArrayGetData(hlist->slots)) + i;
    slot->version++;
  }

  hlist->elementCount = 0;
  hlist->freeSlotCount = 0;
  hlist->firstFreeSlotIndex = 0;
  ldkArrayClear(hlist->slots);
  ldkArenaReset(&hlist->elements);
}

bool ldkHListDestroy(LDKHList* hlist)
{
  ldkArrayDestroy(hlist->slots);
  ldkArenaDestroy(&hlist->elements);
  return true;
}

LDKTypeId ldkHandleType(LDKHandle handle)
{
  LDKHandleInfo hInfo;
  handle_decode(&hInfo, handle);
  return hInfo.type;
}

LDKHListIterator ldkHListIteratorCreate(LDKHList* array)
{
  LDKHListIterator it;
  it.hlist = array;
  it.index = -1;
  it.count = 0;
  return it;
}

bool ldkHListIteratorHasNext(LDKHListIterator* it)
{
  size_t index = it->index + 1;
  while(1)
  {
    it->index++;
    if (it->hlist->elementCount <= index)
      return false;

    LDKSlotInfo* slotInfo = ldkArrayGet(it->hlist->slots, index);

    if (slotInfo->used == false)
      continue;

    return true;
  }

  return false;
}

bool ldkHListIteratorNext(LDKHListIterator* it)
{
  while(1)
  {
    it->index++;
    if (it->count >= it->hlist->elementCount)
      return false;

    LDKSlotInfo* slotInfo = ldkArrayGet(it->hlist->slots, it->index);

    if (slotInfo->used == false)
      continue;

    it->ptr = slotInfo->elementAddress;
    it->count++;
    return true;
  }

  return false;
}

void* ldkHListIteratorFirst(LDKHListIterator* it)
{
  if (it->hlist->elementCount == 0)
    return NULL;

  return ldkArrayGet(it->hlist->slots, 0);
}

void* ldkHListIteratorLast(LDKHListIterator* it)
{
  uint32 elementCount = it->hlist->elementCount;
  if (elementCount == 0)
    return NULL;

  return ldkArrayGet(it->hlist->slots, elementCount - 1);
}

