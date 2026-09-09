#ifndef LDK_EDITOR_THEME_H
#define LDK_EDITOR_THEME_H

#include <stdbool.h>

typedef struct LDKEditorContext LDKEditorContext;
typedef struct LDKEditorThemeCatalog LDKEditorThemeCatalog;

bool ldki_editor_theme_initialize(
    LDKEditorContext *editor, const char *config_directory);
void ldki_editor_theme_terminate(LDKEditorContext *editor);
bool ldki_editor_theme_refresh(LDKEditorContext *editor);
bool ldki_editor_theme_select(
    LDKEditorContext *editor, const char *identifier, bool persist);
void ldki_editor_theme_menu_show(LDKEditorContext *editor);

#endif // LDK_EDITOR_THEME_H
