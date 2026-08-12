#include "ldk_editor_internal.h"

//------------------------------------------------------------
// Console
//------------------------------------------------------------

static void s_editor_console(LDKEditorContext *editor)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->console_sb);

  static XSmallstr input = {0};
  static LDKUIPoint scroll = {0};
  static LDKUIRect window_rect = {150, 90, 200, 180};
  LDKUIContext *ui = &editor->ui;
  bool owns_window = ui->current_window == NULL;

  if (owns_window)
  {
    window_rect = ldk_ui_begin_window_fixed(
        ui, "CONSOLE", window_rect, LDK_UI_WINDOW_TOOL);
  }

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_WARNING];
  icon.color = LDK_EDITOR_COLOR_ICON_WARNING;
  ldk_ui_icon_label(ui, icon, "This is a warning.");
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_ERROR];
  icon.color = LDK_EDITOR_COLOR_ICON_ERROR;
  ldk_ui_icon_label(ui, icon, "This is an error.");
  ldk_ui_label(ui, x_strbuilder_to_string(editor->console_sb));
  ldk_ui_end_scrollview(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_input_box(ui, input.buf, X_SMALLSTR_MAX_LENGTH) &
      LDK_UI_INPUT_BOX_COMMITTED)
  {
    x_strbuilder_append_format(editor->console_sb, "%s\n", input.buf);
    ldk_editor_command_run(editor, input.buf);
    x_smallstr_clear(&input);
    scroll.y += 10000.0f;
  }

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

void ldk_editor_console_show(LDKEditor *editor)
{
  s_editor_console(editor);
}
