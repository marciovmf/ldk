#ifndef LDK_EDITOR_SCENE_OPS_H
#define LDK_EDITOR_SCENE_OPS_H

#include <ldk_mesh.h>
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

#endif // LDK_EDITOR_SCENE_OPS_H
