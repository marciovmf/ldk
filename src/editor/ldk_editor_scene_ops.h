#ifndef LDK_EDITOR_SCENE_OPS_H
#define LDK_EDITOR_SCENE_OPS_H

#include <ldk_mesh.h>
#include <module/ldk_entity.h>
#include <stdbool.h>
#include <stdx/stdx_filesystem.h>

struct LDKEditorContext;

void ldki_editor_scene_state_sync(struct LDKEditorContext *editor);
bool ldki_editor_scene_clear(struct LDKEditorContext *editor);
bool ldki_editor_scene_save(struct LDKEditorContext *editor);
bool ldki_editor_scene_load(
    struct LDKEditorContext *editor, const XFSPath *path);
bool ldki_editor_scene_path_is_scene(const XFSPath *path);
bool ldki_editor_scene_new(struct LDKEditorContext *editor);
bool ldki_editor_scene_new_at_path(
    struct LDKEditorContext *editor, const XFSPath *path);
bool ldki_editor_scene_add_primitive(struct LDKEditorContext *editor,
    LDKMeshPrimitive primitive, const char *name);

/* Apply to Scene records selected serializable values during Play Current
 * Scene and copies them onto the editor scene after Play is stopped.
 */
bool ldki_editor_scene_play_apply_begin(struct LDKEditorContext *editor);
bool ldki_editor_scene_play_apply_restore(struct LDKEditorContext *editor);
void ldki_editor_scene_play_apply_discard(struct LDKEditorContext *editor);
bool ldki_editor_scene_play_apply_available(
    const struct LDKEditorContext *editor);
bool ldki_editor_scene_play_component_can_apply(
    const struct LDKEditorContext *editor, LDKEntity entity);
bool ldki_editor_scene_play_apply_component(
    struct LDKEditorContext *editor, LDKEntity entity, u32 component_type);
bool ldki_editor_scene_play_apply_system(
    struct LDKEditorContext *editor, u64 system_id);

#endif // LDK_EDITOR_SCENE_OPS_H
