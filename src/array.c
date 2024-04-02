#include "ldk/array.h"
#include "ldk/os.h"
#include <memory.h>

struct LDKArray_t
{
  void *array;
  size_t size;
  size_t capacity;
  size_t elementSize;
};

LDKArray* ldkArrayCreate(size_t elementSize, size_t capacity)
{
  LDKArray* arr = (LDKArray*) ldkOsMemoryAlloc(sizeof(LDKArray));
  if (arr == NULL)
  {
    return NULL;
  }

  arr->array = ldkOsMemoryAlloc(capacity * elementSize);
  arr->size = 0;
  arr->capacity = capacity;
  arr->elementSize = elementSize;

  LDK_ASSERT(capacity > 0);
  return arr;
}

void ldkArrayAdd(LDKArray* arr, void* data)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);

  if (arr->size >= arr->capacity)
  {
    arr->capacity = arr->capacity == 0 ? arr->capacity : arr->capacity * 2;
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
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);

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
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  if (index >= arr->size)
  {
    ldkLogError("Index out of bounds");
    return NULL;;
  }

  return (char*)arr->array + (index * arr->elementSize);
}

void ldkArrayDestroy(LDKArray* arr)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);

  ldkOsMemoryFree(arr->array);
  ldkOsMemoryFree(arr);
}

void ldkArrayDeleteRange(LDKArray* arr, size_t start, size_t end)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  
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
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  arr->size = 0;
}

uint32 ldkArrayCount(LDKArray* arr)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  return (uint32) arr->size;
}

uint32 ldkArrayCapacity(LDKArray* arr)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  return (uint32) arr->capacity;
}

void ldkArrayDeleteAt(LDKArray* arr, size_t index)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  ldkArrayDeleteRange(arr, index, index);
}

void* ldkArrayGetData(LDKArray* arr)
{
  LDK_ASSERT(arr->array != NULL);
  LDK_ASSERT(arr->capacity > 0);
  return arr->array;
}

