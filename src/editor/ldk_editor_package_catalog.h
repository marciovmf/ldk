#ifndef LDK_EDITOR_PACKAGE_CATALOG_H
#define LDK_EDITOR_PACKAGE_CATALOG_H

#include "ldk_editor_internal.h"

#define LDK_EDITOR_WINDOW_PACKAGE_CATALOG ((LDKEditorWindowId)0x4C444B0Au)

void ldki_editor_package_catalog_sync(LDKEditorContext *editor);
void ldki_editor_package_catalog_open(LDKEditorContext *editor);
void ldki_editor_package_catalog_show(LDKEditor *editor, void *data);

u32 ldki_editor_package_catalog_count(LDKEditorContext *editor);
const char *ldki_editor_package_catalog_name_get(
    LDKEditorContext *editor, u32 index);
bool ldki_editor_package_catalog_path_is_packageable(
    LDKEditorContext *editor, const XFSPath *path);
bool ldki_editor_package_catalog_add_path(LDKEditorContext *editor,
    u32 package_index, const XFSPath *path, bool is_directory);

#endif // LDK_EDITOR_PACKAGE_CATALOG_H
