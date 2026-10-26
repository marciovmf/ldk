#ifndef LDK_EDITOR_UI_H
#define LDK_EDITOR_UI_H

#include "ldk_editor_internal.h"

/* Optional undo description for a reflected, restorable property.
 * The value is read at the beginning of interaction, not retained as a pointer. */
typedef struct LDKEditorUIUndoProperty
{
  LDKEditorUndoKey key;
  LDKFieldType type;
  const void *value;
  size_t size;
} LDKEditorUIUndoProperty;

/* Editor-specific property rows. The caller owns persistent values. */
void ldk_editor_ui_row_column_update(LDKEditorContext *editor);
void ldk_editor_ui_row_begin(
    LDKEditorContext *editor, const char *label, u32 group_depth);
void ldk_editor_ui_row_label(LDKEditorContext *editor, const char *label,
    u32 group_depth, const char *value);
bool ldk_editor_ui_row_bool(LDKEditorContext *editor, const char *label,
    u32 group_depth, bool value, bool readonly, const LDKEditorUIUndoProperty *undo_property);

float ldk_editor_ui_row_slider_float(LDKEditorContext *editor,
    const char *label, u32 group_depth, float value, float minimum,
    float maximum, bool readonly, const LDKEditorUIUndoProperty *undo_property);
u32 ldk_editor_ui_row_text(LDKEditorContext *editor, const char *label,
    u32 group_depth, char *value, u32 capacity, bool readonly, const LDKEditorUIUndoProperty *undo_property);

/* Selection and color rows. Values are committed by their callers. */
u32 ldk_editor_ui_row_enum(LDKEditorContext *editor, const char *label,
    u32 group_depth, const char *const *options, u32 count, u32 selected,
    bool readonly, const LDKEditorUIUndoProperty *undo_property);
bool ldk_editor_ui_row_color(LDKEditorContext *editor, const char *label,
    u32 group_depth, rgba32 *color, bool readonly, const LDKEditorUIUndoProperty *undo_property);

/* The callback keeps Inspector-specific per-field input state and validation. */
typedef u32 (*LDKEditorUIRowInputFn)(LDKUIContext *ui, void *user,
    u32 index, char *buffer, u32 capacity, const float *float_value);
typedef bool (*LDKEditorUIRowFloatFn)(LDKUIContext *ui, void *user,
    u32 index, float *value, bool readonly);

u32 ldk_editor_ui_row_int(LDKEditorContext *editor, const char *label,
    u32 group_depth, char *buffer, u32 capacity, bool readonly,
    LDKEditorUIRowInputFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
u32 ldk_editor_ui_row_uint(LDKEditorContext *editor, const char *label,
    u32 group_depth, char *buffer, u32 capacity, bool readonly,
    LDKEditorUIRowInputFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
u32 ldk_editor_ui_row_double(LDKEditorContext *editor, const char *label,
    u32 group_depth, char *buffer, u32 capacity, bool readonly,
    LDKEditorUIRowInputFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
u32 ldk_editor_ui_row_float(LDKEditorContext *editor, const char *label,
    u32 group_depth, char *buffer, u32 capacity, const float *value,
    bool readonly, LDKEditorUIRowInputFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
bool ldk_editor_ui_row_vec2(LDKEditorContext *editor, const char *label,
    u32 group_depth, float **values, bool readonly,
    LDKEditorUIRowFloatFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
bool ldk_editor_ui_row_vec3(LDKEditorContext *editor, const char *label,
    u32 group_depth, float **values, bool readonly,
    LDKEditorUIRowFloatFn input, void *user, const LDKEditorUIUndoProperty *undo_property);
bool ldk_editor_ui_row_vec4(LDKEditorContext *editor, const char *label,
    u32 group_depth, float **values, bool readonly,
    LDKEditorUIRowFloatFn input, void *user, const LDKEditorUIUndoProperty *undo_property);

/* Composite Inspector controls retain their editing logic in the caller.
 * The callback returns the focused input belonging to this row, or zero. */
typedef LDKUIId (*LDKEditorUIRowEulerFn)(LDKUIContext *ui, void *user);
void ldk_editor_ui_row_euler(LDKEditorContext *editor, const char *label,
    u32 group_depth, LDKEditorUIRowEulerFn draw, void *user, const LDKEditorUIUndoProperty *undo_property);

/* Atlas region IDs are resolved by the Inspector; the row edits an index. */
u32 ldk_editor_ui_row_atlas_region(LDKEditorContext *editor,
    const char *label, u32 group_depth, const char *const *names,
    u32 count, u32 selected, bool readonly, const LDKEditorUIUndoProperty *undo_property);

/* Asset slot controls return an intent; asset validation/loading stays with the caller. */
typedef enum LDKEditorUIRowSlotAction
{
  LDK_EDITOR_UI_ROW_SLOT_NONE = 0,
  LDK_EDITOR_UI_ROW_SLOT_ASSIGN,
  LDK_EDITOR_UI_ROW_SLOT_CLEAR
} LDKEditorUIRowSlotAction;

LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_mesh(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_material(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, bool optional, bool has_value, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_animation(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_audio(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_font(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_skybox(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, bool optional, bool has_value, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);
/* Image slots may also show an expandable material-texture settings row. */
LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_image(LDKEditorContext *editor,
    const char *label, const char *dialog_title, u32 group_depth,
    const LDKAssetInfo *info, bool readonly, bool optional, bool has_value, bool *expanded,
    XFSPath *path, const LDKEditorUIUndoProperty *undo_property);

LDKEditorUIRowSlotAction ldk_editor_ui_row_slot_texture_atlas(LDKEditorContext *editor,
    const char *label, u32 group_depth, const LDKAssetInfo *info,
    bool readonly, bool has_value, XFSPath *path, const LDKEditorUIUndoProperty *undo_property);

#endif
