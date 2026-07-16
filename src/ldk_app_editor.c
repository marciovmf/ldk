#define X_IMPL_LOG
#include <stdx/stdx_log.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_STRBUILDER
#include <stdx/stdx_strbuilder.h>

#define X_IMPL_FILESYSTEM
#include <stdx/stdx_filesystem.h>

#define X_IMPL_INI
#include <stdx/stdx_ini.h>

#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#define X_IMPL_TML
#include <stdx/stdx_tml.h>

#define X_IMPL_HPOOL
#include <stdx/stdx_hpool.h>

#include <ldk_common.h>
#include <module/ldk_ecs.h>

#include <ldk_common.h>
#include <ldk_game.h>
#include <ldk_event.h>
#include <ldk_os.h>
#include <ldk_image.h>
#include <ldk_scene.h>
#include <ldk_project.h>
#include <module/ldk_ui.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>
#include "editor/ldk_editor_internal.h"

#ifndef LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY 256
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY 1024
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY 32
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY 16
#endif

#ifndef LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY
#define LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY 16
#endif

static void s_editor_update(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time);
static bool s_editor_state_set_play(LDKEditorContext *editor);
static void s_editor_state_set_stop(LDKEditorContext *editor);
static void s_editor_state_set_pause(LDKEditorContext *editor);
static void s_editor_state_set_step(LDKEditorContext *editor);
static bool s_project_load(
    LDKEditorContext *editor, const char *project_file_path);

/**
 * Tiny function to return a static editor instance.
 */
static LDKEditorContext *s_editor_instance(void)
{
  static LDKEditorContext editor = {0};
  return &editor;
}

void ldk_editor_internal_confirm_quit(LDKEditorContext *editor)
{
  if (ldk_os_dialog_show_yes_no(editor->window, "Quit editor ?",
          "Are you sure you want to quit the editor ?"))
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
  }
}

void ldk_editor_internal_theme_icons_set(
    LDKEditorContext *editor, LDKUITheme *theme)
{
  LDKUIIcon icon = {0};
  icon.color = 0xFFFFFFFF;
  icon.size = ldk_sizef(24, 24);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_RIGHT];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  editor->ui.theme.icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  theme->icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHEV_DOWN];
  theme->icons[LDK_UI_THEME_ICON_TREE_NODE_EXPANDED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_UNCHECKED];
  theme->icons[LDK_UI_THEME_ICON_TOGGLE_UNCHECKED] = icon;

  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_CHECKBOX_CHECKED];
  theme->icons[LDK_UI_THEME_ICON_TOGGLE_CHECKED] = icon;
}

bool ldk_editor_internal_show_open_project_dialog(
    LDKEditorContext *editor, XFSPath *project_path_out)
{
  XFSPath out;
  if (ldk_os_dialog_show_open_file(editor->window, "Open Project", "*.ldk",
          out.buf, X_SMALLSTR_MAX_LENGTH))
  {
    s_editor_state_set_stop(editor);
    ldk_project_unload(&editor->project);
    ldk_game_instance_unload();

    if (!s_project_load(editor, out.buf))
    {
      ldk_os_dialog_show_ok(editor->window, "Failed to load project", out.buf);
      return false;
    }
  }

  if (project_path_out)
  {
    strncpy(project_path_out->buf, out.buf, X_FS_PATH_MAX_LENGTH);
    project_path_out->length = out.length;
  }

  return true;
}

//----------------------------------------------------------
// Event Handlers
//----------------------------------------------------------

static bool on_event_keyboard(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    if (event->keyboard_event.ctrl_is_down &&
        event->keyboard_event.shift_is_down)
    {
      // CTRL+SHIFT+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        ldk_editor_state_set_stop(editor);
        return true;
      }
    }
    else if (event->keyboard_event.ctrl_is_down)
    {
      // CTRL+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        s_editor_state_set_play(editor);
        return true;
      }

      // CTRL+O
      if (event->keyboard_event.keyCode == LDK_KEYCODE_O)
      {
        XFSPath out = {0};
        ldk_editor_internal_show_open_project_dialog(editor, &out);
      }
    }
  }
  return false;
}

static bool on_event_text(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (event->text_event.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (editor->text_input_state.codepoint_count <
        LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      editor->text_input_state
          .codepoints[editor->text_input_state.codepoint_count++] =
          event->text_event.character;
      return true;
    }
  }
  return false;
}

static bool on_event_frame(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;
  if (event->type != LDK_EVENT_TYPE_FRAME ||
      event->frame_event.type != LDK_FRAME_EVENT_SUBMIT_AFTER)
    return false;

  // TODO: Replace this by widow event listener
  LDKSize size = ldk_os_window_client_area_size_get(editor->window);

  s_editor_update(editor, size.w, size.h, event->frame_event.delta_time);
  return true;
}

static bool on_event_window(const LDKEvent *event, void *state)
{
  LDKEditorContext *editor = (LDKEditorContext *)state;

  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldk_editor_internal_confirm_quit(editor);
    return true; // Do not propagate this message further
  }
  return false;
}

/*
 *  Test functions
 */

static void s_editor_test_treeview(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  static LDKUIPoint scroll = {0};
  static bool s_root_open[10] = {0};
  static bool s_child_open[10] = {0};
  static char const *s_root_labels[10] = {
      "Root 0",
      "Root 1",
      "Root 2",
      "Root 3",
      "Root 4",
      "Root 5",
      "Root 6",
      "Root 7",
      "Root 8",
      "Root 9",
  };

  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 i = 0; i < 10; i++)
  {
    ldk_ui_push_id_u32(ui, i);

    s_root_open[i] =
        ldk_ui_tree_node(ui, s_root_labels[i], s_root_open[i], 0, 0);
    if (s_root_open[i])
    {
      if (ldk_ui_tree_node(ui, "Position", false, 1, LDK_UI_TREE_NODE_LEAF))
      {
        ldk_log_info("Clicked!");
      }

      ldk_ui_tree_node(ui, "Rotation", false, 1, LDK_UI_TREE_NODE_LEAF);
      ldk_ui_tree_node(ui, "Scale", false, 1, LDK_UI_TREE_NODE_LEAF);

      s_child_open[i] = ldk_ui_tree_node(ui, "Nested", s_child_open[i], 1, 0);

      if (s_child_open[i])
      {
        ldk_ui_tree_node(
            ui, "Nested Position", false, 2, LDK_UI_TREE_NODE_LEAF);
        ldk_ui_tree_node(
            ui, "Nested Rotation", false, 2, LDK_UI_TREE_NODE_LEAF);
        ldk_ui_tree_node(ui, "Nested Scale", false, 2, LDK_UI_TREE_NODE_LEAF);
      }
    }

    ldk_ui_pop_id(ui);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

static void s_editor_save_scene(LDKEditor *editor)
{
  LDKGame *game = ldk_game_get();
  LDKSceneResult result;
  const char *path = "scene.tml";

  (void)editor;

  if (!game)
  {
    ldk_log_error("Failed to save scene: game is not available.");
    return;
  }

  if (!ldk_editor_scene_save_tml_file(path, game, &result))
  {
    ldk_log_error("Failed to save scene: %s", result.error);
    return;
  }

  ldk_log_info("Saved scene: %s", path);
}

static void s_editor_test_a(LDKEditor *editor)
{
  LDKUIContext *ui = &((LDKEditorContext *)editor)->ui;
  static LDKUIRect s_entity_list_rect = {150, 90, 200, 180};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);
  static LDKUIPoint scroll = {0};
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 i = 0; i < 10; i++)
  {
    ldk_ui_button(ui, "btn");
  }

  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

static void s_editor_test_b(LDKEditor *editor)
{
  static bool check = false;
  LDKUIContext *ui = &((LDKEditorContext *)editor)->ui;
  static LDKUIRect s_entity_list_rect = {10, 90, 100, 300};
  s_entity_list_rect = ldk_ui_begin_window_fixed(
      ui, "test A", s_entity_list_rect, LDK_UI_WINDOW_TOOL);

  static LDKUIPoint scroll = {0};
  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  static bool open_a = true;
  open_a = ldk_ui_begin_area(ui, "Transform", open_a);
  if (open_a)
  {
    ldk_ui_label(ui, "Hello, Sailor!");
    ldk_ui_button(ui, "Click me");
    check = ldk_ui_toggle(ui, check);
  }
  ldk_ui_end_area(ui);

  static bool open_b = true;
  static bool open_b1 = true;
  static bool open_b2 = true;
  open_b = ldk_ui_begin_area(ui, "Transform", open_b);
  if (open_b)
  {
    open_b1 = ldk_ui_begin_area(ui, "Transform", open_b1);
    if (open_b1)
    {
      ldk_ui_label(ui, "Hello, Sailor!");
      ldk_ui_button(ui, "Click me");
    }
    ldk_ui_end_area(ui);

    open_b2 = ldk_ui_begin_area(ui, "Transform", open_b2);
    if (open_b2)
    {
      ldk_ui_label(ui, "Hello, Sailor!");
      ldk_ui_button(ui, "Click me");
    }
    ldk_ui_end_area(ui);
  }

  ldk_ui_end_area(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_window(ui);
}

//----------------------------------------------------------
// Editor Udpate
//----------------------------------------------------------

static void s_draw_editor_ui(LDKEditorContext *editor, float delta_time)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  ldk_editor_internal_toolbar_show((LDKEditor *)editor);
  ldk_editor_hierarchy_show((LDKEditor *)editor, ecs);
  s_editor_test_b(editor);
  s_editor_test_treeview(editor);
  ldk_editor_internal_menubar_show(editor);
  ldk_editor_console_show(editor);
  ldk_editor_file_explorer_show(editor, "c:\\work\\ldk");
}

static void s_editor_update(LDKEditorContext *editor, i32 window_width,
    i32 window_height, float delta_time)
{
  LDKMouseState mouse_state;
  LDKKeyboardState kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f,
      .y = 0.0f,
      .w = (float)window_width,
      .h = (float)window_height};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  ldk_ui_begin_frame(&editor->ui, delta_time, &mouse_state, &kbd_state,
      &editor->text_input_state, ui_viewport);
  s_draw_editor_ui(editor, delta_time);
  ldk_ui_end_frame(&editor->ui);
  const LDKUIRenderData *ui_data = ldk_ui_get_render_data(&editor->ui);
  ldk_renderer_submit_ui(ldk_module_get(LDK_MODULE_RENDERER), ui_data);
  editor->text_input_state.codepoint_count = 0;
}

//----------------------------------------------------------
// Editor Initialization
//----------------------------------------------------------

static bool s_editor_config_load_from_ini(
    LDKEditorContext *editor, XIni *ini, LDKConfig *config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->renderer);
  LDK_ASSERT(!editor->initialized);

  // load .editor section
  const char *EDITOR = ".editor";
  editor->editor_font_size = x_ini_get_i32(ini, EDITOR, "font_size", 18);
  x_smallstr_from_cstr(
      &editor->editor_theme, x_ini_get(ini, EDITOR, "theme", "dark"));
  x_fs_path(&editor->editor_font, config->runtree_path,
      x_ini_get(ini, EDITOR, "font", "assets/InterDisplay-Regular.ttf"));

  // load a te texture atlas
  XFSPath atlas_path;

  x_fs_path(&atlas_path, config->runtree_path, "assets", "ui_atlas.png");
  LDKImage *image_atlas = ldk_image_create_from_memory(
      ldk_editor_icon_atlas_png, ldk_editor_icon_atlas_png_size);
  if (image_atlas == NULL)
  {
    ldk_log_error("Failed to load editor atas '%s'\n", atlas_path.buf);
    return false;
  }

  LDKRendererTextureOptions options = {0};
  ldk_renderer_texture_options_defaults(&options);
  options.generate_mipmaps = true;
  options.min_filter = LDK_RHI_FILTER_LINEAR;
  options.mag_filter = LDK_RHI_FILTER_LINEAR;
  options.mip_filter = LDK_RHI_FILTER_LINEAR;

  options.wrap_u = LDK_RHI_WRAP_REPEAT;
  options.wrap_v = LDK_RHI_WRAP_REPEAT;

  LDKResourceTexture texture_atlas = ldk_renderer_texture_create_from_image(
      ldk_module_get(LDK_MODULE_RENDERER), image_atlas, &options);

  if (ldk_renderer_texture_null().id == texture_atlas.id)
  {
    ldk_log_error(
        "Failed to create texture from image atas '%s'\n", atlas_path.buf);
    return false;
  }

  editor->ui_atlas = texture_atlas;
  editor->initialized = true;
  return true;
}

static bool s_editor_load_resources(LDKEditorContext *editor, LDKConfig *config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  LDKAssetManager *asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetManager *renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Load UI editor font
  editor->font =
      ldk_asset_manager_font_load(asset_manager, editor->editor_font.buf);
  LDKAssetFontData *editor_font_data =
      ldk_asset_manager_font_get(asset_manager, editor->font);
  if (!editor_font_data || !editor_font_data->face)
  {
    ldk_log_error(
        "Failed to load editor font '%s'.\n", editor->editor_font.buf);
    return false;
  }

  LDKFontAtlasDesc font_atlas_desc = {0};
  font_atlas_desc.padding = 1;
  font_atlas_desc.page_width = 256;
  font_atlas_desc.page_height = 256;

  editor->font_instance = ldk_ttf_get_instance(editor_font_data->face,
      (float)editor->editor_font_size, &font_atlas_desc);

  if (!editor->font_instance)
  {
    ldk_log_error("Failed to create editor font instance.\n");
    return false;
  }

  return true;
}

static bool s_editor_gui_initialize(
    LDKEditorContext *editor, LDKRenderer *renderer)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  // Editor UI Initialization
  LDKUIConfig ui_cfg = {0};
  ui_cfg.frame_arena_size = 1024 * 4;
  ui_cfg.initial_vertex_capacity = LDK_DEFAULT_UI_INITIAL_VERTEX_CAPACITY;
  ui_cfg.initial_index_capacity = LDK_DEFAULT_UI_INITIAL_INDEX_CAPACITY;
  ui_cfg.initial_command_capacity = LDK_DEFAULT_UI_INITIAL_COMMAND_CAPACITY;
  ui_cfg.initial_window_capacity = LDK_DEFAULT_UI_INITIAL_WINDOW_CAPACITY;
  ui_cfg.initial_id_stack_capacity = LDK_DEFAULT_UI_INITIAL_STACK_CAPACITY;
  ui_cfg.font = editor->font_instance;

  if (strncmp(editor->editor_theme.buf, "light", 5) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  else if (strncmp(editor->editor_theme.buf, "dark", 4) == 0)
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_DARK;
  else
  {
    ldk_log_warning("Unknown Editor theme name '%s'. Default to 'light'.",
        editor->editor_theme.buf);
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  }

  ui_cfg.font_texture_user = renderer;
  ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

  if (!ldk_ui_initialize(&editor->ui, &ui_cfg))
  {
    ldk_log_error("Failed to initialize module: UI System.");
    return false;
  }

  ldk_editor_internal_theme_icons_set(
      editor, &editor->ui.theme); // set theme icons
  return true;
}

//----------------------------------------------------------
// Play / Stop
//----------------------------------------------------------

/**
 * This function replaces the original game_update function.
 * It is prevents the engine from updating the game when
 * the editor is not on PLAY mode
 */
static inline void s_game_update(LDKGame *game, float delta_time)
{
  LDKEditorContext *editor = s_editor_instance();

  if (editor->editor_state == LDK_EDITOR_STATE_STEPPING)
  {
    editor->original_game_update_fn(game, delta_time);
    editor->editor_state = LDK_EDITOR_STATE_PAUSED;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    editor->original_game_update_fn(game, delta_time);
}

/**
 * Change Editor mode to PLAY
 */
static bool s_editor_state_set_play(LDKEditorContext *editor)
{
  if (!editor->project.loaded)
    return false;

  if (editor->editor_state == LDK_EDITOR_STATE_PAUSED)
  {
    editor->editor_state = LDK_EDITOR_STATE_PLAYING;
    return true;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    return true;

  LDKGame *game = ldk_game_get();
  if (!game->start(game))
  {
    return false;
  }

  editor->editor_state = LDK_EDITOR_STATE_PLAYING;
  return true;
}

/**
 * Change Editor mode to STOP
 */
static void s_editor_state_set_stop(LDKEditorContext *editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame *game = ldk_game_get();
  game->stop(game);
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
}

static void s_editor_state_set_pause(LDKEditorContext *editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_PAUSED;
}

static void s_editor_state_set_step(LDKEditorContext *editor)
{
  if (editor->editor_state != LDK_EDITOR_STATE_PAUSED)
    return;

  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STEPPING;
}

//----------------------------------------------------------
// Project handling
//----------------------------------------------------------

static bool s_project_load(
    LDKEditorContext *editor, const char *project_file_path)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!project_file_path)
    return false;

  if (editor->project.loaded)
    return false;

  if (!ldk_project_load(&editor->project, project_file_path))
    return false;

  if (!ldk_game_instance_load_from_shared_lib(
          editor->project.game_dll_path.buf))
    return false;

  if (!ldk_game_instance_initialize())
    return false;

  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  ldk_ecs_terminate();
  ldk_ecs_initialize(ecs, 64, 1);

  // we change the game update function to call us so we can
  // update the game only in PLAY mode.
  LDKGame *game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
  editor->original_game_update_fn = game->update;
  game->update = s_game_update;

  XSmallstr title = {0};
  x_smallstr_format(
      &title, "LDK Editor - %s - %s", editor->project.name, project_file_path);
  ldk_os_window_title_set(editor->window, title.buf);

  return true;
}

static void s_editor_terminate(LDKEditorContext *editor)
{
  LDKEventQueue *eq = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(eq, on_event_text);
  ldk_event_handler_remove(eq, on_event_frame);
  ldk_event_handler_remove(eq, on_event_keyboard);
  ldk_event_handler_remove(eq, on_event_window);
}

//----------------------------------------------------------
// Public API
//----------------------------------------------------------

LDKEditor *ldk_editor_get()
{
  return (LDKEditor *)s_editor_instance();
}

void ldk_editor_state_set_play(LDKEditor *editor)
{
  s_editor_state_set_play((LDKEditorContext *)editor);
}

void ldk_editor_state_set_stop(LDKEditor *editor)
{
  s_editor_state_set_stop((LDKEditorContext *)editor);
}

void ldk_editor_state_set_pause(LDKEditor *editor)
{
  s_editor_state_set_pause((LDKEditorContext *)editor);
}

void ldk_editor_state_play_one_frame(LDKEditor *editor)
{
  s_editor_state_set_step((LDKEditorContext *)editor);
}

bool ldk_editor_project_load(LDKEditor *editor, const char *project_path)
{
  return s_project_load((LDKEditorContext *)editor, project_path);
}

void ldk_editor_quit(LDKEditor *editor)
{
  ldk_editor_internal_confirm_quit((LDKEditorContext *)editor);
}

//----------------------------------------------------------
// Entrypoint
//----------------------------------------------------------

static i32 s_editor_main(const char *project_file_path)
{
  LDKEditorContext *editor = s_editor_instance();
  ldk_editor_internal_register_commands(editor);

  XIni ini;
  XIniError ini_error;
  LDKConfig config;
  XFSPath editor_ini_path;

  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  // Load editor.ini from engine runtree
  x_fs_path_from_executable(&editor->engine_runtree);
  x_fs_path_dirname(&editor->engine_runtree, &editor->engine_runtree);
  x_fs_path_join(&editor->engine_runtree, "..", "..", "runtree");
  x_fs_path(&editor_ini_path, &editor->engine_runtree, "editor.ini");

  if (!x_ini_load_file(editor_ini_path.buf, &ini, &ini_error))
  {
    ldk_log_error("Failed to load config file '%s'. Syntax error at %d:%d: %s",
        editor_ini_path.buf, ini_error.line, ini_error.column,
        ini_error.message ? ini_error.message : "Unknown error");
    return false;
  }

  if (!ldk_engine_config_from_ini(&config, &ini, editor_ini_path.buf))
    return 1;

  // Initialize engine. Must be initialized before editor and projects
  if (!ldk_engine_initialize_with_config(&config))
    return 1;

  // Listen to text events for editor UI
  LDKEventQueue *module_event = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(
      module_event, on_event_text, LDK_EVENT_TYPE_TEXT, editor);
  ldk_event_handler_add(
      module_event, on_event_keyboard, LDK_EVENT_TYPE_KEYBOARD, editor);
  ldk_event_handler_add(
      module_event, on_event_frame, LDK_EVENT_TYPE_FRAME, editor);
  ldk_event_handler_add(
      module_event, on_event_window, LDK_EVENT_TYPE_WINDOW, editor);

  editor->window = ldk_engine_main_window_get();
  editor->renderer = ldk_module_get(LDK_MODULE_RENDERER);
  editor->console_sb = x_strbuilder_create();


  // Initialize editor
  if (!s_editor_config_load_from_ini(editor, &ini, &config))
  {
    ldk_engine_terminate();
    return 1;
  }

  // Load editor resources
  if (!s_editor_load_resources(editor, &config))
  {
    ldk_engine_terminate();
    return 1;
  }

  if (!s_editor_gui_initialize(editor, ldk_module_get(LDK_MODULE_RENDERER)))
  {
    ldk_engine_terminate();
    return 1;
  }

  // If a project file was passed, loat that project
  if (project_file_path)
  {
    s_project_load(editor, project_file_path);
  }

  i32 exit_code = ldk_engine_run();
  s_editor_terminate(editor);
  ldk_engine_terminate();
  return exit_code;
}

int main(i32 argc, char **argv)
{
  char *project_file_path;

  if (argc == 1)
    project_file_path = NULL;
  else if (argc == 2)
    project_file_path = argv[1];
  else
  {
    printf("Usage:\n%s [project_file]\n", argv[0]);
    return 1;
  }

  return s_editor_main(project_file_path);
}
