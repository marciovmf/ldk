#include "ldk/hlist.h"
#include "ldk/os.h"
#include <memory.h>
#include <string.h>

// A SlotInfo holds the index of the actual element in a LDKHandleList

struct LDKSlot_t
{
  uint32 version;
  int32 dataIndex;
};

static uint32 getHandleIndex(LDKHandle handle)
{
  return (uint32_t)((handle >> 16) & 0xFFFFFFFF);
  //return handle & HANDLE_INDEX_MASK;
}

static uint32 getHandleVersion(LDKHandle handle)
{
  //return (handle & HANDLE_VERSION_MASK) >> HANDLE_VERSION_SHIFT;
  return (uint32) ((handle >> 48) & 0xFFFF);
}

static uint32 getHandleType(LDKHandle handle)
{
  return (uint16)(handle & 0xFFFF);
}

static LDKHandle makeHandle(LDKTypeId type, int index, int version)
{
  //return (version << HANDLE_VERSION_SHIFT) | (index & HANDLE_INDEX_MASK);
  return ((uint64_t) version << 48) | ((uint64_t) index << 16) | type;
}

bool ldkHListCreate(LDKHList* hlist, LDKHandleType type, size_t elementSize, int count)
{
  if (!hlist || elementSize == 0 || count <= 0) return false;

  hlist->elementSize = elementSize;
  hlist->capacity = count;
  hlist->freeCount = 0;
  hlist->count = 0;
  hlist->type           = type;
  hlist->data           = (byte*) ldkOsMemoryAlloc(count * elementSize);
  hlist->slots          = (LDKSlot*) ldkOsMemoryAlloc(count * sizeof(LDKSlot));
  hlist->freeSlots      = (int*) ldkOsMemoryAlloc(count * sizeof(int));
  hlist->dataToSlotMap  = (int*) ldkOsMemoryAlloc(count * sizeof(int));
  memset(hlist->slots, 0, count * sizeof(LDKSlot));

  if (!hlist->data || !hlist->slots)
  {
    ldkOsMemoryFree(hlist->data);
    ldkOsMemoryFree(hlist->slots);
    ldkOsMemoryFree(hlist->freeSlots);
    ldkOsMemoryFree(hlist->dataToSlotMap);
    return false;
  }

  return true;
}

LDKHandle ldkHListReserve(LDKHList* hlist)
{
  if (!hlist) return 0;

  if ((hlist->count + 1) >= hlist->capacity)
  {
    uint32 newCapacity = hlist->capacity * 2;
    hlist->data           = (byte*) ldkOsMemoryResize(hlist->data, newCapacity * hlist->elementSize);
    hlist->slots          = (LDKSlot*)ldkOsMemoryResize(hlist->slots, newCapacity * sizeof(LDKSlot));
    hlist->freeSlots      = (int*) ldkOsMemoryResize(hlist->freeSlots, newCapacity * sizeof(int));
    hlist->dataToSlotMap  = (int*) ldkOsMemoryResize(hlist->dataToSlotMap, newCapacity * sizeof(int));

    // make sure new slots have version = 0
    memset((hlist->slots + hlist->capacity), 0, hlist->capacity * sizeof(LDKSlot));
    hlist->capacity = newCapacity;
  }

  // Find a deleted slot or use a new one
  int slotIndex =
    hlist->freeCount > 0 ?
    hlist->freeSlots[--hlist->freeCount]: hlist->count;

  hlist->slots[slotIndex].dataIndex   = hlist->count; // Append to the end of the data list
  hlist->dataToSlotMap[hlist->count]  = slotIndex;    // map the address of this data slot
  hlist->count++;
  return makeHandle(hlist->type, slotIndex, hlist->slots[slotIndex].version);
}

bool ldkHListRemove(LDKHList* hlist, LDKHandle handle)
{
  if (!hlist || hlist->count == 0) return false;

  uint32 index = getHandleIndex(handle);
  uint32 version = getHandleVersion(handle);

  if (index >= hlist->capacity || !hlist->slots || hlist->slots[index].version != version)
    return false;

  // Move the last element into the removed slot to keep the data contiguous
  if (index < hlist->count - 1)
  {
    // Copy the contents of the last element to the "space" left by the deleted element
    byte* deletedDataPtr = hlist->data + index * hlist->elementSize;
    byte* dataToMovePtr = hlist->data + (hlist->count - 1) * hlist->elementSize;
    memcpy(deletedDataPtr, dataToMovePtr, hlist->elementSize);

    // update version of the deleted slot
    hlist->slots[index].version++;
    // Update slot dataIndex of the moved element 
    uint32 lastElementSlot = hlist->dataToSlotMap[hlist->count - 1];
    hlist->slots[lastElementSlot].dataIndex = index;
  }

  // Add the freed slot to freeSlots
  hlist->freeSlots[hlist->freeCount++] = index;
  hlist->count--;

  return true;
}

int32 ldkHListCount(const LDKHList* hlist)
{
  if (!hlist) return 0;
  return hlist->count;
}

byte* ldkHListLookup(LDKHList* hlist, LDKHandle handle)
{
  if (!hlist || hlist->count == 0) return NULL;

  uint32 index = getHandleIndex(handle);
  uint32 version = getHandleVersion(handle);

  if (index >= hlist->capacity || !hlist->slots || hlist->slots[index].version != version) return NULL;

  return hlist->data + index * hlist->elementSize;
}

void ldkHListReset(LDKHList* hlist)
{
  if (!hlist) return;

  hlist->count = 0;
  hlist->freeCount = 0;
}

bool ldkHListDestroy(LDKHList* hlist)
{
  if (!hlist) return false;

  ldkHListReset(hlist);

  ldkOsMemoryFree(hlist->data);
  ldkOsMemoryFree(hlist->slots);
  ldkOsMemoryFree(hlist->freeSlots);
  ldkOsMemoryFree(hlist->dataToSlotMap);

  hlist->data = NULL;
  hlist->slots = NULL;
  hlist->freeSlots = NULL;
  hlist->dataToSlotMap = NULL;
  hlist->capacity = 0;
  hlist->elementSize = 0;
  hlist->count = 0;
  hlist->type = 0;

  return true;
}

LDKHandleType ldkHandleType(LDKHandle handle)
{
  return getHandleType(handle);
}

LDKHListIterator ldkHListIteratorCreate(LDKHList* hlist)
{
  LDKHListIterator it = { hlist, -1 };
  return it;
}

bool ldkHListIteratorHasNext(LDKHListIterator* it)
{
  if (!it || !it->hlist)
    return false;

  return it->current + 1 < (int32) it->hlist->count;
}

bool ldkHListIteratorNext(LDKHListIterator* it)
{
  if (!it || !it->hlist || !ldkHListIteratorHasNext(it))
    return false;
  ++it->current;
  return true;
}

void* ldkHListIteratorFirst(LDKHListIterator* it)
{
  if (!it || !it->hlist || it->hlist->count == 0)
    return NULL;
  it->current = 0;
  return ldkHListLookup(it->hlist, makeHandle(it->hlist->type, it->current, it->hlist->slots[it->current].version));
}

void* ldkHListIteratorLast(LDKHListIterator* it)
{
  if (!it || !it->hlist || it->hlist->count == 0)
    return NULL;
  it->current = it->hlist->count - 1;
  return ldkHListLookup(it->hlist, makeHandle(it->hlist->type, it->current, it->hlist->slots[it->current].version));
}

void* ldkHListIteratorCurrent(LDKHListIterator* it)
{
  if (!it || !it->hlist || it->current < 0 || it->current >= (int32) it->hlist->count)
    return NULL;
  return ldkHListLookup(it->hlist, makeHandle(it->hlist->type, it->current, it->hlist->slots[it->current].version));
}
