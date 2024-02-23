/**
 *
 * hasthable.h
 *
 * A simple Open-adressing, linear probing, hashtable.
 *
 */

#ifndef LDK_HASHMAP_H
#define LDK_HASHMAP_H

#include <stdbool.h>

#include "ldk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct 
  {
    void* key;
    void* value;
    void* ht;
    size_t index;
  } LDKHashMapIterator;

  LDK_API typedef struct LDKHashMap LDKHashMap;

  LDK_API typedef LDKHash (*ldkHashMapHashFunc)(const void* key);
  LDK_API typedef bool (*ldkHashMapCompareFunc)(const void* key1, const void* key2);
  LDK_API typedef void (*ldkHashtableDestroyFunc)(void* key, void* value);

  LDK_API LDKHashMap* ldkHashMapCreate(ldkHashMapHashFunc hashFunction, ldkHashMapCompareFunc compareFunction);
  LDK_API uint32 ldkHashMapCount(LDKHashMap* table);
  LDK_API void  ldkHashMapInsert(LDKHashMap* table, void* key, void* value);
  LDK_API void  ldkHashMapRemove(LDKHashMap* table, void* key);
  LDK_API void  ldkHashMapRemoveWith(LDKHashMap* table, void* key, ldkHashtableDestroyFunc destroyFunc);
  LDK_API void* ldkHashMapGet(LDKHashMap* table, void* key);
  LDK_API bool  ldkHashMapHasKey(LDKHashMap* table, void* key);
  LDK_API void  ldkHashMapDestroy(LDKHashMap* table);
  LDK_API void  ldkHashMapDestroyWith(LDKHashMap* table, ldkHashtableDestroyFunc destroyFunc);

  //
  // Iterator
  //
  LDK_API LDKHashMapIterator ldkHashMapIteratorGet(LDKHashMap* table);
  LDK_API bool ldkHashMapIteratorNext(LDKHashMapIterator* it);


  // utility functions for string keys
  LDK_API LDKHash ldkHashMapStrHashFunc(const void* data);
  LDK_API bool ldkHashMapStrCompareFunc(const void* key1, const void* key2);

  LDK_API LDKHash ldkHashMapPathHashFunc(const void* data);
  LDK_API bool ldkHashMapPathCompareFunc(const void* key1, const void* key2);

#ifdef __cplusplus
} 
#endif
#endif //LDK_HASHMAP_H

