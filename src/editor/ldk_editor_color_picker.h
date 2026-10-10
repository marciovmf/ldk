#ifndef LDK_EDITOR_COLOR_PICKER_H
#define LDK_EDITOR_COLOR_PICKER_H

#include <ldk_common.h>
#include <module/ldk_ui.h>

struct LDKEditorContext;

bool ldki_editor_color_field(
    struct LDKEditorContext *editor, rgba32 *color, bool readonly);

/* Compact swatch for explicitly-positioned editor rows. Uses the same HSV/RGB
 * picker as the Inspector; the caller owns the field value and widget ID. */
bool ldki_editor_color_swatch_widget(struct LDKEditorContext *editor,
    LDKUIId id, rgba32 *color, bool readonly, LDKUIRect rect);

#endif // LDK_EDITOR_COLOR_PICKER_H
