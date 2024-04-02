#include "ldk/array.h"
#include "ldk/os.h"
#include <memory.h>

struct LDKArray_t
{
  void *array;
  size_t size;
  size_t capacity;
  size_t elementSize;
  size_t defaultCapacity;
};

LDKArray* ldkArrayCreate(size_t elementSize, size_t capacity)
{
  LDKArray* arr = (LDKArray*) ldkOsMemoryAlloc(sizeof(LDKArray));
  if (arr == NULL)
  {
    return NULL;
  }

  arr->array = NULL;
  arr->size = 0;
  arr->capacity = 0;
  arr->defaultCapacity = capacity;
  arr->elementSize = elementSize;

  LDK_ASSERT(capacity > 0);
  return arr;
}

void ldkArrayAdd(LDKArray* arr, void* data)
{
  if (arr->size >= arr->capacity)
  {
    arr->capacity = arr->capacity == 0 ? arr->defaultCapacity : arr->capacity * 2;
    arr->array = ldkOsMemoryResize(arr->array, arr->capacity * arr->elementSize);
    if (!arr->array)
    {
      ldkLogError("Memory allocation failed");
    }
  }

  if (data != NULL)
    memcpy((char*)arr->array + (arr->size * arr->elementSize), data, arr->elementSize);

  arr->size++;
}

void ldkArrayInsert(LDKArray* arr, void* data, size_t index)
{
  if (index > arr->size)
  {
    ldkLogError("Index out of bounds");
    return;
  }

  if (arr->size >= arr->capacity)
  {
    arr->capacity = arr->capacity == 0 ? 1 : arr->capacity * 2;
    arr->array = ldkOsMemoryResize(arr->array, arr->capacity * arr->elementSize);
    if (!arr->array)
    {
      ldkLogError("Memory allocation failed");
      return;
    }
  }

  memmove((char*)arr->array + ((index + 1) * arr->elementSize),
      (char*)arr->array + (index * arr->elementSize),
      (arr->size - index) * arr->elementSize);
  memcpy((char*)arr->array + (index * arr->elementSize), data, arr->elementSize);
  arr->size++;
}

void* ldkArrayGet(LDKArray* arr, size_t index)
{
  if (index >= arr->size)
  {
    ldkLogError("Index out of bounds");
    return NULL;;
  }

  return (char*)arr->array + (index * arr->elementSize);
}

void ldkArrayDestroy(LDKArray* arr)
{
  ldkOsMemoryFree(arr->array);
  ldkOsMemoryFree(arr);
}

void ldkArrayDeleteRange(LDKArray* arr, size_t start, size_t end)
{
  if (start >= arr->size || end >= arr->size || start > end)
  {
    ldkLogError("Invalid range %d - %d on array of size %d", start, end, arr->size);
    return;
  }

  size_t deleteCount = end - start + 1;
  memmove(
      (char*)arr->array + (start * arr->elementSize),       // Destination
      (char*)arr->array + ((end + 1) * arr->elementSize),   // Source
      (arr->size - end - 1) * arr->elementSize);            // Size
  arr->size -= deleteCount;
}

void ldkArrayClear(LDKArray* arr)
{
  arr->size = 0;
}

uint32 ldkArrayCount(LDKArray* arr)
{
  return (uint32) arr->size;
}

void ldkArrayDeleteAt(LDKArray* arr, size_t index)
{
  ldkArrayDeleteRange(arr, index, index);
}

void* ldkArrayGetData(LDKArray* arr)
{
  return arr->array;
}

