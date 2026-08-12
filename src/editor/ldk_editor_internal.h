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
#include "../ldk_stdx.h"


#define LDK_EDITOR_COLOR_FILE 0xFFFFFFFF
#define LDK_EDITOR_COLOR_FOLDER 0xFAD460FF
#define LDK_EDITOR_COLOR_ICON_ERROR 0xE71A2DFF
#define LDK_EDITOR_COLOR_ICON_WARNING 0xF7B217FF

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
  XFSPath current_scene_path;
  LDKEntity selected_entity;
  XArray *hierarchy_expanded_entities;
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

void ldki_editor_menubar_show(LDKEditorContext *editor);
void ldki_editor_toolbar_show(LDKEditorContext *editor);
void ldki_editor_inspector_show(LDKEditorContext *editor);
u32 ldki_editor_input_window(LDKEditorContext *editor, const char *title);
bool ldki_editor_layout_save_as(LDKEditorContext *editor);
void ldki_editor_theme_icons_set(LDKEditorContext *editor, LDKUITheme *theme);
void ldki_editor_project_create_show(LDKEditorContext *editor);
void ldki_editor_register_commands(LDKEditorContext *editor);
void ldki_editor_confirm_quit(LDKEditorContext *editor);
bool ldki_editor_show_open_project_dialog(
    LDKEditorContext *editor, XFSPath *project_path_out);

void ldki_editor_log_error(LDKEditorContext *editor, const char *msg);
void ldki_editor_log_warning(LDKEditorContext *editor, const char *msg);
void ldki_editor_log_info(LDKEditorContext *editor, const char *msg);

u32 ldki_editor_dock_layout_count(void);
const char *ldki_editor_dock_layout_name_get(u32 index);
const char *ldki_editor_dock_layout_current_name_get(void);

/**
 * saves the dock tml representation to the dock file under a specific name
 */
bool ldki_editor_dock_layout_save(XStrBuilder *out);

/**
 * loads the dock tml representation from the dock file under a specific name
 */
bool ldki_editor_dock_layout_load(const char *layout_name);

/**
 * makes the dock layout identified by layout_name, the default layout
 */
bool ldki_editor_dock_set_current(const char *layout_name);

bool ldki_editor_dock_layout_create(const char *layout_name);
bool ldki_editor_dock_layout_delete(const char *layout_name);

bool ldki_editor_entity_equal(LDKEntity a, LDKEntity b);
void ldki_editor_entity_display_name(
    const LDKEntityInfo *info, LDKEntity entity, char *out, size_t out_size);

void ldki_editor_entity_display_name(
    const LDKEntityInfo *info, LDKEntity entity, char *out, size_t out_size);
bool ldki_editor_selected_entity_get(
    LDKEditorContext *editor, LDKECS *ecs, LDKEntity *out_entity);

void ldki_editor_scene_state_sync(LDKEditorContext *editor);
bool ldk_editor_scene_internal_path_is_scene(const XFSPath *path);
bool ldki_editor_scene_save(LDKEditorContext *editor);
bool ldki_editor_scene_load(LDKEditorContext *editor, const XFSPath *path);
bool ldki_editor_scene_new(LDKEditorContext *editor);

bool ldki_editor_scene_add_primitive(
    LDKEditorContext *editor, LDKMeshPrimitive primitive, const char *name);


// Editor window IDs are stored in the docking layout and must therefore be
// stable across runs. The value is intentionally just an application-defined
// integer. It must be non-zero and unique among the windows registered by the
// editor and its tools. Do not use an address as an ID.

typedef void (*LDKEditorWindowFunction)(LDKEditor *editor, void *data);
typedef u32 LDKEditorWindowId;

typedef struct LDKEditorWindow
{
  LDKEditorWindowId id;
  const char *title;
  LDKEditorWindowFunction function;
  void *data;
} LDKEditorWindow;

// Stable IDs reserved by the editor. User tools should define their own
// persistent non-zero values outside this range.

#define LDK_EDITOR_WINDOW_PROJECT_EXPLORER ((LDKEditorWindowId)0x4C444B01u)
#define LDK_EDITOR_WINDOW_SCENE ((LDKEditorWindowId)0x4C444B02u)
#define LDK_EDITOR_WINDOW_INSPECTOR ((LDKEditorWindowId)0x4C444B03u)
#define LDK_EDITOR_WINDOW_CONSOLE ((LDKEditorWindowId)0x4C444B04u)

#endif // LDK_EDITOR_INTERNAL
