#ifndef LDK_EDITOR_UNDO_H
#define LDK_EDITOR_UNDO_H

#include <editor/ldk_component_metadata.h>
#include <stddef.h>
#include <stdint.h>

/* A stable, caller-defined property identity. Neither value is an address. */
typedef struct LDKEditorUndoKey
{
  uint64_t owner;
  uint64_t property;
} LDKEditorUndoKey;

/* Return NULL when the original property no longer exists. */
typedef void *(*LDKEditorUndoResolveFn)(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size);

typedef struct LDKEditorUndoEntry LDKEditorUndoEntry;

typedef struct LDKEditorUndo
{
  LDKEditorUndoEntry *entries;
  size_t count;
  size_t capacity;
  size_t position;
  LDKEditorUndoEntry *active;
  LDKEditorUndoResolveFn resolve;
  void *user;
} LDKEditorUndo;

void ldk_editor_undo_initialize(
    LDKEditorUndo *undo, LDKEditorUndoResolveFn resolve, void *user);
void ldk_editor_undo_terminate(LDKEditorUndo *undo);
void ldk_editor_undo_clear(LDKEditorUndo *undo);

/* size describes the stored representation, including enums and asset handles.
 * Only trivially copyable values with a stable byte representation are supported.
 * The value must remain valid through this call, not through the session. */
bool ldk_editor_undo_begin_change(LDKEditorUndo *undo,
    LDKEditorUndoKey key, LDKFieldType type, const void *initial, size_t size);
/* Returns true for a successfully ended session, including no-op sessions. */
bool ldk_editor_undo_end_change(LDKEditorUndo *undo, const void *final_value);
bool ldk_editor_undo_undo(LDKEditorUndo *undo);
bool ldk_editor_undo_redo(LDKEditorUndo *undo);

#endif
