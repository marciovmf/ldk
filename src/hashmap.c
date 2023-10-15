#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ldk/os.h"
#include "ldk/hashmap.h"
#include "ldk/common.h"

#ifndef LDK_HASHMAP_INITIAL_CAPACITY
#define LDK_HASHMAP_INITIAL_CAPACITY 64
#endif

#ifndef LDK_HASHMAP_LOAD_FACTOR_THRESHOLD
#define LDK_HASHMAP_LOAD_FACTOR_THRESHOLD 0.6
#endif

typedef struct
{
  void* key;
  void* value;
} LDKHashMapEntry;

struct LDKHashMap
{
  uint32 count;
  size_t capacity;
  LDKHashMapEntry* entry;
  ldkHashMapHashFunc hashFunction;
  ldkHashMapCompareFunc compareFunction;
};

LDKHashMap* ldkHashMapCreate(ldkHashMapHashFunc hashFunction, ldkHashMapCompareFunc compareFunction)
{
  LDKHashMap* table = (LDKHashMap*) ldkOsMemoryAlloc(sizeof(LDKHashMap));
  if (table == NULL)
  {
    return NULL;
  }

  table->count = 0;
  table->capacity = LDK_HASHMAP_INITIAL_CAPACITY;
  table->hashFunction = hashFunction;
  table->compareFunction = compareFunction;
  table->entry = (LDKHashMapEntry*) ldkOsMemoryAlloc(table->capacity * sizeof(LDKHashMapEntry));

  if (table->entry == NULL)
  {
    ldkOsMemoryFree(table);
    return NULL;
  }
  memset(table->entry, 0, table->capacity * sizeof(LDKHashMapEntry));
  return table;
}

uint32 ldkHashMapCount(LDKHashMap* table)
{
  return table->count;
}

static void internalHashTabeResize(LDKHashMap* table)
{
  size_t newCapacity = table->capacity * 2;
  LDKHashMapEntry* newEntryList = (LDKHashMapEntry*)ldkOsMemoryAlloc(newCapacity * sizeof(LDKHashMapEntry));

  for (size_t i = 0; i < table->capacity; i++)
  {
    if (table->entry[i].key != NULL)
    {
      // Reinsert elements
      size_t index = table->hashFunction(table->entry[i].key) % newCapacity;
      while (newEntryList[index].key != NULL)
      {
        index = (index + 1) % newCapacity;
      }
      newEntryList[index] = table->entry[i];
    }
  }

  ldkOsMemoryFree(table->entry);
  table->entry = newEntryList;
  table->capacity = newCapacity;
}

void ldkHashMapInsert(LDKHashMap* table, void* key, void* value)
{
  if ((double)(table->count) / table->capacity > LDK_HASHMAP_LOAD_FACTOR_THRESHOLD)
  {
    internalHashTabeResize(table);
  }

  size_t index = table->hashFunction(key) % table->capacity;

  while (table->entry[index].key != NULL)
  {
    if (table->compareFunction(table->entry[index].key, key))
    {
      table->entry[index].value = value;
      return;
    }
    index = (index + 1) % table->capacity;
  }

  // Key doesn't exist, insert a new entry
  table->entry[index].key = key;
  table->entry[index].value = value;
  table->count++;
}

void ldkHashMapRemove(LDKHashMap* table, void* key)
{
  ldkHashMapRemoveWith(table, key, NULL);
}

void ldkHashMapRemoveWith(LDKHashMap* table, void* key, ldkHashtableDestroyFunc destroyFunc)
{
  size_t index = table->hashFunction(key) % table->capacity;

  while (table->entry[index].key != NULL)
  {
    if (table->compareFunction(table->entry[index].key, key))
    {
      if (destroyFunc != NULL)
      {
        destroyFunc( table->entry[index].key, table->entry[index].value);
      }

      table->entry[index].key = NULL;
      table->entry[index].value = NULL;
      table->count--;

      return;
    }
    index = (index + 1) % table->capacity;
  }
}

void* ldkHashMapGet(LDKHashMap* table, void* key)
{
  size_t index = table->hashFunction(key) % table->capacity;

  while (table->entry[index].key != NULL)
  {
    if (table->compareFunction(table->entry[index].key, key))
    {
      return table->entry[index].value;
    }
    index = (index + 1) % table->capacity;
  }
  return NULL;
}

bool ldkHashMapHasKey(LDKHashMap* table, void* key)
{
  size_t index = table->hashFunction(key) % table->capacity;

  while (table->entry[index].key != NULL)
  {
    if (table->compareFunction(table->entry[index].key, key))
    {
      return true;
    }
    index = (index + 1) % table->capacity;
  }
  return false;
}

void ldkHashMapDestroy(LDKHashMap* table)
{
  ldkOsMemoryFree(table->entry);
  ldkOsMemoryFree(table);
}

void ldkHashMapDestroyWith(LDKHashMap* table, ldkHashtableDestroyFunc destroyFunc)
{
  for (uint32 i = 0; i < table->capacity; i++)
  {
    if (table->entry[i].key == NULL)
      continue;
    destroyFunc(table->entry[i].key, table->entry[i].value);
  }

  ldkHashMapDestroy(table);
}

//
// Iterator
//

LDKHashMapIterator ldkHashMapIteratorGet(LDKHashMap* table)
{
  LDKHashMapIterator it = {0};
  if (table->count > 0)
  {
    it.index = -1;
    it.ht = table;
  }
  return it;
}

bool ldkHashMapIteratorNext(LDKHashMapIterator* it)
{
  LDKHashMap* table = it->ht;

  while (++it->index < table->capacity)
  {
    if (table->entry[it->index].key == NULL)
      continue;

    it->key = table->entry[it->index].key;
    it->value = table->entry[it->index].value;
    return true;
  }

  return false;
}

// Compare function for string keys (const char*)
bool ldkHashMapStrCompareFunc(const void* key1, const void* key2)
{
  return strcmp((const char*)key1, (const char*)key2) == 0;
}

