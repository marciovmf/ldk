#include <ldk.h>
#include <ldk_os.h>
#include <ldk_geom.h>
#include <module/ldk_editor.h>
#include <module/ldk_eventqueue.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_arena.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_string.h>
#include <stdx/stdx_math.h>
#include <inttypes.h>

#ifdef LDK_EDITOR

void s_run_commnd(LDKUIContext* ui, XStrBuilder* output, const char* cmd)
{
  size_t cmd_len = strlen(cmd);
  if (strncmp(cmd, "cls", cmd_len) == 0)
  {
    x_strbuilder_clear(output);
  }
  else if (strncmp(cmd, "fullscreen", cmd_len) == 0)
  {
    LDKWindow main_window = ldk_main_window();
    ldk_os_window_fullscreen_set(main_window, true);
  }
  else if (strncmp(cmd, "nofullscreen", cmd_len) == 0)
  {
    LDKWindow main_window = ldk_main_window();
    ldk_os_window_fullscreen_set(main_window, false);
  }
  else
  {
    x_strbuilder_append_format(output, "'%s' is not a know command.\n", cmd);
  }
}

static bool s_on_text_event(const LDKEvent* event, void* state)
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

static void s_editor_entity_list_window(LDKUIContext* ui, LDKECS* ecs)
{
  static LDKUIRect s_entity_list_rect = {0};
  s_entity_list_rect.w = ui->viewport.w * 0.2f;
  s_entity_list_rect.h = ui->viewport.h * 0.8f;

  s_entity_list_rect = ldk_ui_begin_window(ui, "Entities", s_entity_list_rect);
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

void s_console_toggle(LDKConsole* console)
{
  if (console->state == CONSOLE_IS_OPENED || console->state == CONSOLE_IS_OPENING)
  {
    console->state = CONSOLE_IS_CLOSING;
  }
  else if (console->state == CONSOLE_IS_CLOSED || console->state == CONSOLE_IS_CLOSING)
  {
    console->state = CONSOLE_IS_OPENING;
  }
}

void s_console_initialize(LDKConsole* console, LDKUIContext* ui)
{
  printf("%fx%f\n", ui->viewport.w, ui->viewport.h);
  console->state = CONSOLE_IS_CLOSED;
  console->rect.w = ui->viewport.w;
  console->rect.h = ui->viewport.h / 3;
  console->rect.x = 0;
  console->rect.y = -console->rect.h;
  console->output = x_strbuilder_create();
  console->scroll_pos = (LDKUIPoint){0.0f, 0.0f};
  x_strbuilder_append_cstr(console->output, "Hello, Sailor!");
}

void s_console_draw(LDKConsole* console, LDKUIContext* ui,  LDKKeyboardState* kbd_state, float delta_time)
{
  if (console->rect.w <= 0 || console->rect.h <= 0)
  {
    s_console_initialize(console, ui);
  }

  const float step = 600.0f * delta_time;

  if (console->state == CONSOLE_IS_CLOSING)
  {
    console->rect.y -= step;
    if (console->rect.y < -console->rect.h)
    {
      console->rect.y = -console->rect.h;
      console->state = CONSOLE_IS_CLOSED;
    }
  }
  else if (console->state == CONSOLE_IS_OPENING)
  {
    console->rect.y += step;
    if (console->rect.y >= 0.0f)
    {
      console->rect.y = 0.0f;
      console->state = CONSOLE_IS_OPENED;
    }
  }

  if (ldk_os_keyboard_key_down(kbd_state, LDK_KEYCODE_F2))
    s_console_toggle(console);

  if (console->state != CONSOLE_IS_CLOSED)
  { // console
    console->rect.w = ui->viewport.w;
    console->rect.h = ui->viewport.h / 3;
    ldk_ui_begin_pane(ui, console->rect);

    console->scroll_pos = ldk_ui_begin_vertical_scroll_area(ui, console->scroll_pos);

    ldk_ui_set_next_expand_height(ui, true);
    ldk_ui_label(ui, x_strbuilder_to_string(console->output));
    ldk_ui_end_vertical_scroll_area(ui);

    if (ldk_ui_text_box(ui, console->input.buf, X_SMALLSTR_MAX_LENGTH))
    {
      i32 len = (i32) strlen(console->input.buf);
      if (ldk_os_keyboard_key_down(kbd_state, LDK_KEYCODE_RETURN) && (len > 0))
      {
        x_strbuilder_append_format(console->output, "%s\n", console->input.buf);
        s_run_commnd(ui, console->output, console->input.buf);
        x_smallstr_clear(&console->input);
      }
    }

    ldk_ui_end_pane(ui);
  }
}

void s_draw_editor_ui(LDKEditor* editor, float delta_time)
{
  LDKECS* ecs = ldk_module_get(LDK_MODULE_ECS);
  s_editor_entity_list_window(&editor->ui, ecs);
}

#if 0


float r = 0.0f;
float g = 0.0f;
float b = 0.0f;
bool bool_value = false;
char* msg = "Hello, Sailor!";


LDKUIRect w1 = { 10, 205, 400, 300 };
LDKUIRect w2 = { 10, 100, 400, 400 };
LDKUIRect w3 = {0};


bool theme = false;

#define RGBA_U32(r, g, b) \
  (((u32)(r) & 0xFFu) << 24 | \
   ((u32)(g) & 0xFFu) << 16 | \
   ((u32)(b) & 0xFFu) << 8  | \
   0xFFu)

LDKUIPoint scroll_pos = {0};



static void s_editor_entity_list_window(LDKUIContext* ui, LDKEntityRegistry* er)
{
  static LDKUIRect s_entity_list_rect = {0};
  s_entity_list_rect.w = ui->viewport.w * 0.2f;
  s_entity_list_rect.h = ui->viewport.h * 0.8f;

  s_entity_list_rect = ldk_ui_begin_window(ui, "Entities", s_entity_list_rect);
  LDKEntityIterator it = ldk_entity_iterator_begin(er);
  LDKEntity e;
  while (ldk_entity_iterator_next(&it, &e))
  {
    LDKEntityInfo* info = ldk_entity_info_get(er, e);
    u64 id = *((u64*) &e);
    snprintf((char* const) &info->name, LDK_ENTITY_NAME_MAX_LEN, "0x%08" PRIu64 "(%d)", id, info->components.component_count);

    ldk_ui_label(ui, (const char*) info->name);
    for(u32 i = 0; i < info->components.component_count; i++)
    {
      const char* name = ldk_component_name_get(&g_engine.ecs.component, info->components.component_type[i]);
      ldk_ui_label(ui, name);
    }
  }
  ldk_entity_iterator_end(&it);
  ldk_ui_end_window(ui);
}

void s_draw_ui(LDKUIContext* ui, float delta_time)
{
  s_editor_entity_list_window(ui, &g_engine.ecs.entity);

  const float step = 1000.0f * delta_time;
  if (console_state == CONSOLE_IS_CLOSING)
  {
    w3.y -= step;
    if (w3.y < -w3.h)
    {
      w3.y = -w3.h;
      console_state = CONSOLE_IS_CLOSED;
    }
  }
  else if (console_state == CONSOLE_IS_OPENING)
  {
    w3.y += step;
    if (w3.y >= 0.0f)
    {
      w3.y = 0.0f;
      console_state = CONSOLE_IS_OPENED;
    }
  }

  {
    w1 = ldk_ui_begin_window(ui, "Window 1", w1);

    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_expand_height(ui, true);
    if (ldk_ui_button(ui, "Toggle Console"))
    {
      if (console_state == CONSOLE_IS_OPENED || console_state == CONSOLE_IS_OPENING) {
        console_state = CONSOLE_IS_CLOSING;
      }
      else if (console_state == CONSOLE_IS_CLOSED || console_state == CONSOLE_IS_CLOSING) {
        console_state = CONSOLE_IS_OPENING;
      }
    }

    ldk_ui_set_next_expand_height(ui, true);
    if (ldk_ui_button(ui, "Theme"))
    {
      theme = !theme;
      ldk_ui_set_theme(ui, theme ? LDK_UI_THEME_DEFAULT_LIGHT : LDK_UI_THEME_DEFAULT_DARK, NULL);
    }

    ldk_ui_end_horizontal(ui);
    ldk_ui_set_next_expand_height(ui, true);
    bool_value = ldk_ui_toggle_button(ui, "Toggle", bool_value);

    ldk_ui_begin_horizontal(ui);

    ldk_ui_begin_vertical(ui);
    ldk_ui_set_next_expand_height(ui, true);
    r = ldk_ui_slider(ui, "Slider", r, 0.0f, 255.0f);
    ldk_ui_set_next_expand_height(ui, true);
    g = ldk_ui_slider(ui, "Slider", g, 0.0f, 255.0f);
    ldk_ui_set_next_expand_height(ui, true);
    b = ldk_ui_slider(ui, "Slider", b, 0.0f, 255.0f);
    ldk_ui_end_vertical(ui);

    ldk_ui_set_next_expand_height(ui, true);
    ldk_ui_color_view(ui, RGBA_U32(r, g, b));
    ldk_ui_end_horizontal(ui);


    ldk_ui_label(ui, msg);
    ldk_ui_end_window(ui);
  }
#if 0
  if (bool_value)
  {
    w2 = ldk_ui_begin_window(ui, "Scroll Window", w2);
    scroll_pos = ldk_ui_begin_vertical_scroll_area(ui, scroll_pos);


    ldk_ui_set_next_expand_height(ui, true);
    ui->theme.slider_track_height = ldk_ui_slider(ui, "Slider", ui->theme.slider_track_height, 0.0f, 1.0f);

    ldk_ui_set_next_expand_height(ui, true);
    ui->theme.slider_thumb_width = ldk_ui_slider(ui, "Slider", ui->theme.slider_thumb_width, 0.0f, 1.0f);

    for(u32 i = 0; i < 30; i++)
    {
      ldk_ui_button(ui, "Button");
    }
    ldk_ui_end_vertical_scroll_area(ui);
    ldk_ui_end_window(ui);
  }
#endif


  if (bool_value)
  {
    w2 = ldk_ui_begin_window(ui, "Scroll Window", w2);
    ldk_ui_label(ui, "Box 1");
    ldk_ui_text_box(ui, text_box1.buf, X_SMALLSTR_MAX_LENGTH);

    ldk_ui_begin_disabled(ui, true);
    ldk_ui_label(ui, "Box 2");
    ldk_ui_text_box(ui, text_box2.buf, X_SMALLSTR_MAX_LENGTH);
    ldk_ui_end_disabled(ui);

    if (ldk_ui_button(ui, "copy"))
    {
      memcpy((char*) &text_box2.buf, (char*) &text_box1.buf, strlen(text_box1.buf));
    }
    ldk_ui_end_window(ui);
  }


  if (console_state != CONSOLE_IS_CLOSED)
  { // console
    w3.w = ui->viewport.w;
    w3.h = ui->viewport.h / 3;
    ldk_ui_begin_pane(ui, w3);
    ldk_ui_label(ui, "This is a console with text\nIt suport multiple lines!\nHello, Sailor!");
    ldk_ui_end_pane(ui);
  }

  ui_text_input.codepoint_count = 0;
}
#endif

bool ldk_editor_initialize(LDKEditor* editor, LDKConfig* config)
{
  LDKAssetManager* asset_manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetManager* renderer = ldk_module_get(LDK_MODULE_RENDERER);

  // Load UI editor font
  editor->font = ldk_asset_manager_font_load(asset_manager, config->editor_font.buf);
  LDKAssetFontData* editor_font_data = ldk_asset_manager_font_get(asset_manager, editor->font);
  if (!editor_font_data || !editor_font_data->face)
  {
    ldk_log_error("Failed to load editor font '%s'.", config->editor_font.buf);
    return false;
  }

  LDKFontAtlasDesc font_atlas_desc = {0};
  font_atlas_desc.padding = 1;
  font_atlas_desc.page_width = 256;
  font_atlas_desc.page_height = 256;

  LDKFontInstance* editor_font = ldk_font_get_instance(editor_font_data->face,
      (float)config->editor_font_size, &font_atlas_desc);

  if (!editor_font)
  {
    ldk_log_error("Failed to create editor font instance.");
    return false;
  }

  { // Editor UI Initialization
    LDKUIConfig ui_cfg = {0};
    ui_cfg.frame_arena_size = 1024 * 4;
    ui_cfg.initial_vertex_capacity = LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY;
    ui_cfg.initial_index_capacity = LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY;
    ui_cfg.initial_command_capacity = LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY;
    ui_cfg.initial_window_capacity = LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY;
    ui_cfg.initial_id_stack_capacity = LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY;
    ui_cfg.font = editor_font;

    if (strncmp(config->editor_theme.buf, "light", 5) == 0)
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
    else if (strncmp(config->editor_theme.buf, "dark", 4) == 0)
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_DARK;
    else
    {
      ldk_log_warning("Unknown Editor theme name '%s'. Default to 'light'.", config->editor_theme.buf);
      ui_cfg.theme = LDK_UI_THEME_DEFAULT_LIGHT;
    }

    ui_cfg.font_texture_user = renderer;
    ui_cfg.get_font_page_texture = ldk_renderer_get_font_page_texture_callback;

    if (!ldk_ui_initialize(&editor->ui, &ui_cfg))
    {
      ldk_log_error("Failed to initialize module: UI System.");
      return false;
    }

  }

  // Listen to text events for editor UI
  ldk_event_handler_add(ldk_module_get(LDK_MODULE_EVENT), s_on_text_event, LDK_EVENT_TYPE_TEXT, editor);
  s_console_initialize(&editor->console, &editor->ui);

  return true;
}

void ldk_editor_update(LDKEditor* editor, i32 window_width, i32 window_height, float delta_time)
{
  LDKMouseState     mouse_state;
  LDKKeyboardState  kbd_state;
  LDKUIRect ui_viewport = (LDKUIRect){.x = 0.0f, .y = 0.0f, .w = (float) window_width, .h = (float) window_height};
  ldk_os_mouse_state_get(&mouse_state);
  ldk_os_keyboard_state_get(&kbd_state);

  ldk_ui_begin_frame(&editor->ui, &mouse_state, &kbd_state, &editor->text_input_state, ui_viewport);
  s_draw_editor_ui(editor, delta_time);
  s_console_draw(&editor->console, &editor->ui, &kbd_state, delta_time);
  ldk_ui_end_frame(&editor->ui);
  const LDKUIRenderData* ui_data = ldk_ui_get_render_data(&editor->ui);
  ldk_renderer_submit_ui(ldk_module_get(LDK_MODULE_RENDERER), ui_data);
  editor->text_input_state.codepoint_count = 0;
}

void ldk_editor_terminate(LDKEditor* editor)
{
  ldk_event_handler_remove(ldk_module_get(LDK_MODULE_EVENT), s_on_text_event);
}

#endif
