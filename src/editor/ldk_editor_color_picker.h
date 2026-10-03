#ifndef LDK_EDITOR_COLOR_PICKER_H
#define LDK_EDITOR_COLOR_PICKER_H

#include <ldk_common.h>

struct LDKEditorContext;

bool ldki_editor_color_field(
    struct LDKEditorContext *editor, rgba32 *color, bool readonly);

#endif // LDK_EDITOR_COLOR_PICKER_H
