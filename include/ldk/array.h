/**
 *
 * array.h
 *
 * An array is a contiguous memory block that may be resized to accomodate more
 * elements. Do not store pointers to data inside the array as they might become
 * invalid when the array is resized.
 *
 */

#ifndef LDK_ARRAY_H
#define LDK_ARRAY_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKArray_t LDKArray;


  LDK_API LDKArray* ldkArrayCreate(size_t elementSize, size_t capacity);
  LDK_API void ldkArrayAdd(LDKArray* arr, void* data);
  LDK_API void ldkArrayInsert(LDKArray* arr, void* data, size_t index);
  LDK_API void* ldkArrayGet(LDKArray* arr, size_t index);
  LDK_API void* ldkArrayGetData(LDKArray* arr);
  LDK_API uint32 ldkArrayCount(LDKArray* arr);
  LDK_API uint32 ldkArrayCapacity(LDKArray* arr);
  LDK_API void ldkArrayDeleteRange(LDKArray* arr, size_t start, size_t end);
  LDK_API void ldkArrayClear(LDKArray* arr);
  LDK_API void ldkArrayDeleteAt(LDKArray* arr, size_t index);
  LDK_API void ldkArrayDestroy(LDKArray* arr);  

#ifdef __cplusplus
}
#endif
#endif //LDK_ARRAY_H
