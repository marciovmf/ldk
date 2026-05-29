#include <ldk_common.h>
#include <module/ldk_ecs.h>

#define X_IMPL_LOG
#include <stdx/stdx_log.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_FILESYSTEM
#include <stdx/stdx_filesystem.h>

#define X_IMPL_INI
#include <stdx/stdx_ini.h>

#include <ldk_game.h>
#include <ldk_event.h>
#include <ldk_os.h>
#include <ldk_project.h>
#include <module/ldk_ui.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>

#include <inttypes.h> // for PRIu64 

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

typedef enum LDKEditorState
{
  LDK_EDITOR_STATE_STOPED   = 0,
  LDK_EDITOR_STATE_PAUSED   = 1,
  LDK_EDITOR_STATE_STEPPING = 2,
  LDK_EDITOR_STATE_PLAYING  = 3
} LDKEditorState;

typedef struct LDKEditor
{
  LDKWindow             window;
  LDKUIContext          ui;
  LDKAssetFont          font;
  LDKFontInstance*      font_instance;

  LDKUITextInputState   text_input_state;
  LDKProject            project;
  bool                  initialized;
  LDKEditorState        editor_state;
  XFSPath               engine_runtree;
  LDKGameUpdateFunc     original_game_update_fn;

  // config
  XFSPath               editor_font;
  XSmallstr             editor_theme;
  i32                   editor_font_size;
} LDKEditor;

static void s_editor_update(LDKEditor* editor, i32 window_width, i32 window_height, float delta_time);
static bool s_editor_state_set_play(LDKEditor* editor);
static void s_editor_state_set_stop(LDKEditor* editor);
static void s_editor_state_set_pause(LDKEditor* editor);
static void s_editor_state_set_step(LDKEditor* editor);
static bool s_project_load(LDKEditor* editor, const char* project_file_path);

/**
 * Tiny function to return a static editor instance.
 */
static LDKEditor* s_editor_instance(void)
{
  static LDKEditor editor = {0};
  return &editor;
}

static inline void s_editor_command_quit(LDKEditor * editor)
{
  if (ldk_os_dialog_show_yes_no(editor->window, "Quit editor ?", "Are you sure you want to quit the editor ?"))
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
  }
}

//----------------------------------------------------------
// Event Handlers
//----------------------------------------------------------

static bool on_event_keyboard(const LDKEvent* event, void* state)
{
  LDKEditor* editor = (LDKEditor*) state;
  if (event->keyboard_event.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    if (event->keyboard_event.ctrl_is_down && event->keyboard_event.shift_is_down)
    {
      // CTRL+SHIFT+P
      if (event->keyboard_event.keyCode == LDK_KEYCODE_P)
      {
        s_editor_state_set_stop(editor);
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
        if (ldk_os_dialog_show_open_file(editor->window, "Open Project", "*.ldk", out.buf, X_SMALLSTR_MAX_LENGTH))
        {
          s_editor_state_set_stop(editor);
          ldk_project_unload(&editor->project);
          ldk_game_instance_unload();

          if (!s_project_load(editor, out.buf))
          {
            ldk_os_dialog_show_ok(editor->window, "Failed to load project", out.buf);
          }
        }
        return true;
      }
    }
  }
  return false;
}

static bool on_event_text(const LDKEvent* event, void* state)
{
  LDKEditor* editor = (LDKEditor*) state;
  if (event->text_event.type == LDK_TEXT_EVENT_CHARACTER_INPUT)
  {
    if (editor->text_input_state.codepoint_count < LDK_UI_INPUT_CODEPOINTS_CAPACITY)
    {
      editor->text_input_state.codepoints[editor->text_input_state.codepoint_count++] = event->text_event.character;
      return true;
    }
  }
  return false;
}

static bool on_event_frame(const LDKEvent* event, void* state)
{
  LDKEditor* editor = (LDKEditor*) state;
  if (event->type != LDK_EVENT_TYPE_FRAME || event->frame_event.type != LDK_FRAME_EVENT_SUBMIT_AFTER)
    return false;

  //TODO: Replace this by widow event listener
  LDKSize size = ldk_os_window_client_area_size_get(editor->window);

  s_editor_update(editor, size.w, size.h, event->frame_event.delta_time);
  return true;
}

static bool on_event_window(const LDKEvent* event, void* state)
{
  LDKEditor* editor = (LDKEditor*) state;

  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    s_editor_command_quit(editor);
    return true; // Do not propagate this message further
  }
  return false;
}

//----------------------------------------------------------
// Editor Udpate
//----------------------------------------------------------

static void s_editor_menu_bar(LDKEditor* editor)
{
  LDKUIContext* ui = &editor->ui;

  static LDKUIRect s_toolbar_rect = {0, 0, 0, 0};
  static LDKUIRect s_file_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_edit_popup_rect = {0, 0, 1024, 1024};
  static LDKUIRect s_theme_popup_rect = {0, 0, 1024, 1024};

  s_toolbar_rect.w = ui->viewport.w;
  s_toolbar_rect.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  s_toolbar_rect = ldk_ui_begin_window_fixed(ui, "toolbar", s_toolbar_rect, 0);

  ldk_ui_begin_horizontal(ui, ldk_ui_fill(), ldk_ui_px(24.0f));
  if (ldk_ui_button_flat(ui, "File")) { ldk_ui_open_popup(ui, "file_menu"); }
  LDKUIRect file_button_rect = ldk_ui_last_rect(ui);

  if (ldk_ui_button_flat(ui, "Edit")) { ldk_ui_open_popup(ui, "edit_menu"); }
  LDKUIRect edit_button_rect = ldk_ui_last_rect(ui);

  if (ldk_ui_button_flat(ui, "Theme")) { ldk_ui_open_popup(ui, "theme_menu"); }
  LDKUIRect theme_button_rect = ldk_ui_last_rect(ui);
  ldk_ui_end_horizontal(ui);

  LDKUIRect popup_pos = {
   file_button_rect.x,
   file_button_rect.y + file_button_rect.h,
   120, 10};

  if (ldk_ui_begin_popup(ui, "file_menu", popup_pos))
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    if (ldk_ui_button_flat(ui, "New"))
    {
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Open"))
    {
      XFSPath out = {0};

      if (ldk_os_dialog_show_open_file(editor->window, "Open Project", "*.ldk", out.buf, X_SMALLSTR_MAX_LENGTH))
      {
        s_editor_state_set_stop(editor);
        ldk_project_unload(&editor->project);
        ldk_game_instance_unload();

        if (!s_project_load(editor, out.buf))
        {
          ldk_os_dialog_show_error(editor->window, "Failed to load project", out.buf);
        }
      }

      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Exit"))
    {
      s_editor_command_quit(editor);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);
    ldk_ui_end_popup(ui);
  }

  popup_pos.x = edit_button_rect.x;
  popup_pos.y = edit_button_rect.y + edit_button_rect.h;


  if (ldk_ui_begin_popup(ui, "edit_menu", popup_pos))
  {
    LDKUIMark mark = ldk_ui_mark(ui);

    if (ldk_ui_button_flat(ui, "Undo"))
    {
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Redo"))
    {
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);

    ldk_ui_end_popup(ui);
  }

  popup_pos.x = theme_button_rect.x;
  popup_pos.y = theme_button_rect.y + theme_button_rect.h;

  if (ldk_ui_begin_popup(ui, "theme_menu", popup_pos))
  {
    LDKUIMark mark = ldk_ui_mark(ui);
    if (ldk_ui_button_flat(ui, "Dark"))
    {
      ldk_ui_set_theme(ui, LDK_UI_THEME_DEFAULT_DARK, NULL);
      ldk_ui_close_current_popup(ui);
    }

    ldk_ui_set_next_width(ui, ldk_ui_fill());
    if (ldk_ui_button_flat(ui, "Light---------------------!!"))
    {
      ldk_ui_set_theme(ui, LDK_UI_THEME_DEFAULT_LIGHT, NULL);
      ldk_ui_close_current_popup(ui);
    }

    LDKUIRect content_rect = ldk_ui_measure_from(ui, mark);

    ldk_ui_end_popup(ui);
  }

  ldk_ui_end_window(ui);
}

static void s_editor_tool_bar(LDKEditor* editor)
{
  LDKUIContext* ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = {0, LDK_UI_DEFAULT_CONTROL_HEIGHT, 0, 0};
  s_entity_list_rect.w = ui->viewport.w;
  s_entity_list_rect.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;

  s_entity_list_rect = ldk_ui_begin_window_fixed(ui, "Editor Commands", s_entity_list_rect, 0);
  ldk_ui_begin_horizontal(&editor->ui, ldk_ui_fill(), ldk_ui_fill());

  if (editor->project.loaded)
  {
    ldk_ui_set_next_disabled(ui, (editor->editor_state == LDK_EDITOR_STATE_PLAYING));
    //if (editor->editor_state != LDK_EDITOR_STATE_PLAYING)
    {
      // Play
      if (ldk_ui_button_flat(ui, "Play"))
      {
        s_editor_state_set_play(editor);
      }
    }

    //if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    ldk_ui_set_next_disabled(ui, (editor->editor_state != LDK_EDITOR_STATE_PLAYING));
    {
      if (ldk_ui_button_flat(ui, "Pause"))
      {
        s_editor_state_set_pause(editor);
      }
    }

    ldk_ui_set_next_disabled(ui, (editor->editor_state == LDK_EDITOR_STATE_STOPED));
    //if (editor->editor_state != LDK_EDITOR_STATE_STOPED)
    {
      if (ldk_ui_button_flat(ui, "Stop"))
      {
        s_editor_state_set_stop(editor);
      }
    }

    //if(editor->editor_state == LDK_EDITOR_STATE_PAUSED)
    ldk_ui_set_next_disabled(ui, (editor->editor_state != LDK_EDITOR_STATE_PAUSED));
    {
      if (ldk_ui_button_flat(ui, "Step"))
      {
        s_editor_state_set_step(editor);
      }
    }
  }

  ldk_ui_end_horizontal(&editor->ui);
  ldk_ui_end_window(ui);
}

static void s_editor_entity_list_window(LDKEditor* editor, LDKECS* ecs)
{
  LDKUIContext* ui = &editor->ui;
  static LDKUIRect s_entity_list_rect = { 10, 90, 100, 100};

  s_entity_list_rect = ldk_ui_begin_window(ui, "Entities",
      s_entity_list_rect, LDK_UI_WINDOW_TOOL);
  LDKEntityIterator it = ldk_entity_iterator_begin(&ecs->entity);
  LDKEntity e;
  while (ldk_entity_iterator_next(&it, &e))
  {
    LDKEntityInfo* info = ldk_entity_info_get(&ecs->entity, e);
    u64 id = *((u64*) &e);
    snprintf((char* const) &info->name, LDK_ENTITY_NAME_MAX_LEN, "0x%08" PRIu64 "(%d)", id, info->components.component_count);

    ldk_ui_label(ui, (const char*) info->name);
    for(u32 i = 0; i < info->components.component_count; i++)
    {
      const char* name = ldk_component_name_get(&ecs->component, info->components.component_type[i]);
      ldk_ui_label(ui, name);
    }
  }
  ldk_entity_iterator_end(&it);
  ldk_ui_end_window(ui);
}

static void s_draw_editor_ui(LDKEditor* editor, float delta_time)
{
  LDKECS* ecs = ldk_module_get(LDK_MODULE_ECS);
  s_editor_menu_bar(editor);
  s_editor_tool_bar(editor);
  s_editor_entity_list_window(editor, ecs);
}

static void s_editor_update(LDKEditor* editor, i32 window_width, i32 window_height, float delta_time)
{
  LDKMouseState     mouse_state;
  LDKKeyboardState  kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f, .y = 0.0f, .w = (float) window_width, .h = (float) window_height};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  ldk_ui_begin_frame(&editor->ui, &mouse_state, &kbd_state, &editor->text_input_state, ui_viewport);
  s_draw_editor_ui(editor, delta_time);
  ldk_ui_end_frame(&editor->ui);
  const LDKUIRenderData* ui_data = ldk_ui_get_render_data(&editor->ui);
  ldk_renderer_submit_ui(ldk_module_get(LDK_MODULE_RENDERER), ui_data);
  editor->text_input_state.codepoint_count = 0;
}

//----------------------------------------------------------
// Editor Initialization
//----------------------------------------------------------

static bool s_editor_config_load_from_ini(LDKEditor* editor, XIni* ini, LDKConfig* config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(!editor->initialized);

  // load .editor section
  const char* EDITOR = ".editor";
  editor->editor_font_size = x_ini_get_i32(ini, EDITOR, "font_size", 18);
  x_smallstr_from_cstr(&editor->editor_theme,
      x_ini_get(ini, EDITOR, "theme", "dark"));
  x_fs_path(&editor->editor_font, config->runtree_path,
      x_ini_get(ini, EDITOR, "font", "assets/InterDisplay-Regular.ttf"));

  editor->initialized = true;
  return true;
}

static bool s_editor_load_resources(LDKEditor* editor, LDKConfig* config)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  LDKAssetManager* asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetManager* renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Load UI editor font
  editor->font = ldk_asset_manager_font_load(asset_manager, editor->editor_font.buf);
  LDKAssetFontData* editor_font_data = ldk_asset_manager_font_get(asset_manager, editor->font);
  if (!editor_font_data || !editor_font_data->face)
  {
    ldk_log_error("Failed to load editor font '%s'.\n", editor->editor_font.buf);
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

static bool s_editor_gui_initialize(LDKEditor* editor, LDKRenderer* renderer)
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
    ldk_log_warning("Unknown Editor theme name '%s'. Default to 'light'.", editor->editor_theme.buf);
    ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
  }

  ui_cfg.font_texture_user = renderer;
  ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

  if (!ldk_ui_initialize(&editor->ui, &ui_cfg))
  {
    ldk_log_error("Failed to initialize module: UI System.");
    return false;
  }

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
static inline void s_game_update(LDKGame* game, float delta_time)
{
  LDKEditor* editor = s_editor_instance();

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
static bool s_editor_state_set_play(LDKEditor* editor)
{
  if(!editor->project.loaded)
    return false;

  if (editor->editor_state == LDK_EDITOR_STATE_PAUSED)
  {
    editor->editor_state = LDK_EDITOR_STATE_PLAYING;
    return true;
  }

  if (editor->editor_state == LDK_EDITOR_STATE_PLAYING)
    return true;

  LDKGame* game = ldk_game_get();
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
static void s_editor_state_set_stop(LDKEditor* editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame* game = ldk_game_get();
  game->stop(game);
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
}

static void s_editor_state_set_pause(LDKEditor* editor)
{
  if (editor->editor_state == LDK_EDITOR_STATE_STOPED)
    return;

  LDKGame* game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_PAUSED;
}

static void s_editor_state_set_step(LDKEditor* editor)
{
  if (editor->editor_state != LDK_EDITOR_STATE_PAUSED)
    return;

  LDKGame* game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STEPPING;
}

//----------------------------------------------------------
// Project handling
//----------------------------------------------------------

static bool s_project_load(LDKEditor* editor, const char* project_file_path)
{
  LDK_ASSERT(editor);
  LDK_ASSERT(editor->initialized);

  if (!project_file_path)
    return false;

  if (editor->project.loaded)
    return false;

  if (!ldk_project_load(&editor->project, project_file_path))
    return false;

  if (!ldk_game_instance_load_from_shared_lib(editor->project.game_dll_path.buf))
    return false;

  if (!ldk_game_instance_initialize())
    return false;

  LDKECS* ecs = ldk_module_get(LDK_MODULE_ECS);
  ldk_ecs_terminate();
  ldk_ecs_initialize(ecs, 64, 1);

  // we change the game update function to call us so we can
  // update the game only in PLAY mode.
  LDKGame* game = ldk_game_get();
  editor->editor_state = LDK_EDITOR_STATE_STOPED;
  editor->original_game_update_fn = game->update;
  game->update = s_game_update;

  XSmallstr title = {0};
  x_smallstr_format(&title, "LDK Editor - %s - %s", editor->project.name, project_file_path);
  ldk_os_window_title_set(editor->window, title.buf);

  return true;
}

static void s_editor_terminate(LDKEditor* editor)
{
  LDKEventQueue* eq = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_remove(eq, on_event_text);
  ldk_event_handler_remove(eq, on_event_frame);
  ldk_event_handler_remove(eq, on_event_keyboard);
  ldk_event_handler_remove(eq, on_event_window);

}

//----------------------------------------------------------
// Entrypoint
//----------------------------------------------------------

static i32 s_editor_main(const char* project_file_path)
{
  LDKEditor *editor = s_editor_instance();
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
    ldk_log_error(
        "Failed to load config file '%s'. Syntax error at %d:%d: %s",
        editor_ini_path.buf,
        ini_error.line,
        ini_error.column,
        ini_error.message ? ini_error.message : "Unknown error");
    return false;
  }

  if (!ldk_engine_config_from_ini(&config, &ini, editor_ini_path.buf))
    return 1;

  // Initialize engine. Must be initialized before editor and projects
  if (!ldk_engine_initialize_with_config(&config))
    return 1;

  // Listen to text events for editor UI
  LDKEventQueue* module_event = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(module_event, on_event_text, LDK_EVENT_TYPE_TEXT, editor);
  ldk_event_handler_add(module_event, on_event_keyboard, LDK_EVENT_TYPE_KEYBOARD, editor);
  ldk_event_handler_add(module_event, on_event_frame, LDK_EVENT_TYPE_FRAME, editor);
  ldk_event_handler_add(module_event, on_event_window, LDK_EVENT_TYPE_WINDOW, editor);

  editor->window = ldk_engine_main_window_get();

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

int main(i32 argc, char** argv)
{
  char* project_file_path;

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
