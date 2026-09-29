#include "ldk_editor_project_create.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_project_options.h"
#include "ldk_os.h"
#include <module/ldk_ui.h>

static void s_editor_project_create_show(LDKEditorContext *editor)
{
  static bool need_clean = false;
  static XSmallstr s_project_name = {0};
  static XFSPath s_project_path = {0};
  static u32 s_generator = 0;
  LDKUIContext *ui;
  const LDKEditorProjectGenerator *generator;
  const char *cmake_arch;

  if (editor == NULL)
  {
    return;
  }

  bool is_busy = editor->project_build.active;
  if (need_clean && !is_busy)
  {
    x_smallstr_clear(&s_project_name);
    x_smallstr_clear(&s_project_path);
    s_generator = 0;
    need_clean = false;
  }
 
  ui = &editor->ui;
  ldk_ui_begin_disabled(ui, is_busy);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project name");
    ldk_ui_input_box(ui, s_project_name.buf, (u32)sizeof(s_project_name.buf));
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Generator");
    s_generator = ldk_ui_combo_box(ui, ldki_editor_project_generator_labels_get(),
        ldki_editor_project_generator_count(),
        s_generator);
  }
  ldk_ui_end_horizontal(ui);

  if (s_generator >= ldki_editor_project_generator_count())
  {
    s_generator = 0;
  }
  generator = ldki_editor_project_generator_get(s_generator);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_set_next_width(ui, ldk_ui_px(100.0f));
    ldk_ui_label(ui, "Project path");

    ldk_ui_set_next_disabled(ui, true);
    ldk_ui_input_box(ui, s_project_path.buf, (u32)sizeof(s_project_path.buf));

    ldk_ui_set_next_width(ui, ldk_ui_px(32.0f));
    if (ldk_ui_button(ui, "..."))
    {
      ldk_os_dialog_show_open_folder(editor->window, "Project Location", "",
          s_project_path.buf, (u32)sizeof(s_project_path.buf));
    }
  }
  ldk_ui_end_horizontal(ui);

  //----------------------------------------------------------------------
  // Actions
  //----------------------------------------------------------------------

  ldk_ui_spacer(ui);
  ldk_ui_horizontal_line(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  {
    ldk_ui_spacer(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));

    if (ldk_ui_button(ui, "OK"))
    {
      need_clean = true;
      cmake_arch = generator->uses_platform
                       ? ldki_editor_cmake_native_arch_get()
                       : "";

      if (!ldki_editor_project_create_request(editor, s_project_name.buf,
              s_project_path.buf, generator->cmake_generator, cmake_arch))
      {
        ldki_editor_log_error(editor, "Failed to queue project creation.");
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
    
    if (ldk_ui_button(ui, "CANCEL"))
    {
      need_clean = true;
      editor->create_project_window_close_requested = true;
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_end_disabled(ui);
}

void ldki_editor_project_create_window(LDKEditor *opaque_editor, void *data)
{
  (void)data;
  s_editor_project_create_show((LDKEditorContext *)opaque_editor);
}

