#ifndef LDK_EDITOR_INTERNAL
#define LDK_EDITOR_INTERNAL

#include <ldk_common.h>
#include <ldk_game.h>
#include <ldk_project.h>
#include <module/ldk_ui.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <editor/ldk_editor.h>
#include "ldk_editor_atlas.h"

#include <stdx/stdx_array.h>
#include <stdx/stdx_strbuilder.h>

typedef enum LDKEditorState
{
  LDK_EDITOR_STATE_STOPED = 0,
  LDK_EDITOR_STATE_PAUSED = 1,
  LDK_EDITOR_STATE_STEPPING = 2,
  LDK_EDITOR_STATE_PLAYING = 3
} LDKEditorState;

#ifndef LDK_EDITOR_COMMAND_MAX_LENGTH
#define LDK_EDITOR_COMMAND_MAX_LENGTH 32
#endif

#ifndef LDK_EDITOR_COMMAND_INITIAL_CAPACITY
#define LDK_EDITOR_COMMAND_INITIAL_CAPACITY 16
#endif

#ifndef LDK_EDITOR_DOCK_LAYOUT_CAPACITY
#define LDK_EDITOR_DOCK_LAYOUT_CAPACITY 16
#endif

#ifndef LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY
#define LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY 64
#endif

typedef struct LDKEditorCommand
{
  char name[LDK_EDITOR_COMMAND_MAX_LENGTH];
  u32 hash;
  LDKEditorCommandFn cmd_func;
  XSmallstr help;
} LDKEditorCommand;

typedef struct LDKEditorContext
{
  LDKWindow window;
  LDKUIContext ui;
  LDKAssetFont font;
  LDKFontInstance *font_instance;
  LDKRenderer *renderer;
  XArray *commands;

  LDKUITextInputState text_input_state;
  LDKProject project;
  bool initialized;
  LDKEditorState editor_state;
  XFSPath engine_runtree;
  LDKGameUpdateFunc original_game_update_fn;
  LDKResourceTexture ui_atlas;

  // Console output string builder
  XStrBuilder *console_sb;

  LDKUIRect input_window_rect;
  char input_window_buffer[X_SMALLSTR_MAX_LENGTH];
  bool show_input_window;

  //
  bool create_project_window_show;

  // config
  XFSPath editor_font;
  XSmallstr editor_theme;
  i32 editor_font_size;
} LDKEditorContext;

void ldk_editor_internal_menubar_show(LDKEditorContext *editor);
void ldk_editor_internal_toolbar_show(LDKEditorContext *editor);
u32 ldk_editor_internal_input_window(
    LDKEditorContext *editor, const char *title);
bool ldk_editor_internal_layout_save_as(LDKEditorContext *editor);
void ldk_editor_internal_theme_icons_set(
    LDKEditorContext *editor, LDKUITheme *theme);
void ldk_editor_internal_project_create_show(LDKEditorContext *editor);
void ldk_editor_internal_register_commands(LDKEditorContext *editor);
void ldk_editor_internal_confirm_quit(LDKEditorContext *editor);
bool ldk_editor_internal_show_open_project_dialog(
    LDKEditorContext *editor, XFSPath *project_path_out);

void ldk_editor_internal_log_error(LDKEditorContext *editor, const char* msg);
void ldk_editor_internal_log_warning(LDKEditorContext *editor, const char* msg);
void ldk_editor_internal_log_info(LDKEditorContext  *editor, const char* msg);


u32 ldk_editor_internal_dock_layout_count(void);
const char *ldk_editor_internal_dock_layout_name_get(u32 index);
const char *ldk_editor_internal_dock_layout_current_name_get(void);

/**
 * saves the dock tml representation to the dock file under a specific name
 */
bool ldk_editor_internal_dock_layout_save(XStrBuilder *out);

/**
 * loads the dock tml representation from the dock file under a specific name
 */
bool ldk_editor_internal_dock_layout_load(const char *layout_name);

/**
 * makes the dock layout identified by layout_name, the default layout
 */
bool ldk_editor_internal_dock_set_current(const char *layout_name);

bool ldk_editor_internal_dock_layout_create(const char *layout_name);
bool ldk_editor_internal_dock_layout_delete(const char *layout_name);

#endif // LDK_EDITOR_INTERNAL
