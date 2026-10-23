#include "ldk_editor_undo.h"

#include <stdlib.h>
#include <string.h>

struct LDKEditorUndoEntry
{
  LDKEditorUndoKey key;
  LDKFieldType type;
  size_t size;
  unsigned char *before;
  unsigned char *after;
};

static void s_editor_undo_entry_free(LDKEditorUndoEntry *entry)
{
  if (entry == NULL)
  {
    return;
  }
  free(entry->before);
  free(entry->after);
  memset(entry, 0, sizeof(*entry));
}

void ldk_editor_undo_initialize(
    LDKEditorUndo *undo, LDKEditorUndoResolveFn resolve, void *user)
{
  if (undo == NULL)
  {
    return;
  }
  memset(undo, 0, sizeof(*undo));
  undo->resolve = resolve;
  undo->user = user;
}

void ldk_editor_undo_clear(LDKEditorUndo *undo)
{
  if (undo == NULL)
  {
    return;
  }
  for (size_t i = 0; i < undo->count; ++i)
  {
    s_editor_undo_entry_free(&undo->entries[i]);
  }
  free(undo->entries);
  undo->entries = NULL;
  undo->count = 0;
  undo->capacity = 0;
  undo->position = 0;
  if (undo->active != NULL)
  {
    s_editor_undo_entry_free(undo->active);
    free(undo->active);
    undo->active = NULL;
  }
}

void ldk_editor_undo_terminate(LDKEditorUndo *undo)
{
  if (undo == NULL)
  {
    return;
  }
  ldk_editor_undo_clear(undo);
  undo->resolve = NULL;
  undo->user = NULL;
}

bool ldk_editor_undo_begin_change(LDKEditorUndo *undo,
    LDKEditorUndoKey key, LDKFieldType type, const void *initial, size_t size)
{
  LDKEditorUndoEntry *entry;
  if (undo == NULL || undo->active != NULL || undo->resolve == NULL ||
      initial == NULL || size == 0)
  {
    return false;
  }
  entry = (LDKEditorUndoEntry *)calloc(1, sizeof(*entry));
  if (entry == NULL)
  {
    return false;
  }
  entry->before = (unsigned char *)malloc(size);
  if (entry->before == NULL)
  {
    free(entry);
    return false;
  }
  memcpy(entry->before, initial, size);
  entry->key = key;
  entry->type = type;
  entry->size = size;
  undo->active = entry;
  return true;
}

bool ldk_editor_undo_end_change(LDKEditorUndo *undo, const void *final_value)
{
  LDKEditorUndoEntry *entry;
  LDKEditorUndoEntry *items;
  size_t capacity;
  if (undo == NULL || undo->active == NULL || final_value == NULL)
  {
    return false;
  }
  entry = undo->active;
  if (memcmp(entry->before, final_value, entry->size) == 0)
  {
    s_editor_undo_entry_free(entry);
    free(entry);
    undo->active = NULL;
    return true;
  }
  entry->after = (unsigned char *)malloc(entry->size);
  if (entry->after == NULL)
  {
    return false; /* Keep session active: retry or clear. */
  }
  memcpy(entry->after, final_value, entry->size);
  if (undo->position == undo->capacity)
  {
    if (undo->capacity > (SIZE_MAX / 2) ||
        (undo->capacity ? undo->capacity * 2 : 16) >
            SIZE_MAX / sizeof(*items))
    {
      free(entry->after);
      entry->after = NULL;
      return false;
    }
    capacity = undo->capacity ? undo->capacity * 2 : 16;
    items = (LDKEditorUndoEntry *)realloc(
        undo->entries, capacity * sizeof(*items));
    if (items == NULL)
    {
      free(entry->after);
      entry->after = NULL;
      return false;
    }
    undo->entries = items;
    undo->capacity = capacity;
  }
  for (size_t i = undo->position; i < undo->count; ++i)
  {
    s_editor_undo_entry_free(&undo->entries[i]);
  }
  undo->count = undo->position;
  undo->entries[undo->count++] = *entry;
  undo->position = undo->count;
  free(entry);
  undo->active = NULL;
  return true;
}

bool ldk_editor_undo_undo(LDKEditorUndo *undo)
{
  LDKEditorUndoEntry *entry;
  void *destination;
  if (undo == NULL || undo->active != NULL || undo->position == 0 ||
      undo->resolve == NULL)
  {
    return false;
  }
  entry = &undo->entries[undo->position - 1];
  destination = undo->resolve(undo->user, entry->key, entry->type, entry->size);
  if (destination == NULL)
  {
    return false;
  }
  memcpy(destination, entry->before, entry->size);
  --undo->position;
  return true;
}

bool ldk_editor_undo_redo(LDKEditorUndo *undo)
{
  LDKEditorUndoEntry *entry;
  void *destination;
  if (undo == NULL || undo->active != NULL || undo->position == undo->count ||
      undo->resolve == NULL)
  {
    return false;
  }
  entry = &undo->entries[undo->position];
  destination = undo->resolve(undo->user, entry->key, entry->type, entry->size);
  if (destination == NULL)
  {
    return false;
  }
  memcpy(destination, entry->after, entry->size);
  ++undo->position;
  return true;
}
