#include "ldk_editor_internal.h"
#include "ldk_editor_theme.h"
#include "ldk_editor_color_picker.h"
#include <ldk.h>
#include <module/ldk_eventqueue.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_ini.h>
#include <stdx/stdx_string.h>

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_CONFIG_FILE "editor.ini"
#define LDK_EDITOR_THEME_METRIC_COUNT 10u

#define S_EDITOR_THEME_COLOR(name) {#name, LDK_UI_COLOR_##name}

typedef struct LDKEditorThemeColorEntry
{
  const char *name;
  LDKUIColorSlot slot;
} LDKEditorThemeColorEntry;

static const LDKEditorThemeColorEntry s_editor_theme_colors[] = {
    S_EDITOR_THEME_COLOR(TEXT),
    S_EDITOR_THEME_COLOR(TEXT_DISABLED),
    S_EDITOR_THEME_COLOR(WINDOW_BG),
    S_EDITOR_THEME_COLOR(PANEL_BG),
    S_EDITOR_THEME_COLOR(CONTROL_BG),
    S_EDITOR_THEME_COLOR(CONTROL_BG_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_BG_ACTIVE),
    S_EDITOR_THEME_COLOR(CONTROL_BG_ACTIVE_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_TEXT),
    S_EDITOR_THEME_COLOR(CONTROL_TEXT_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_TEXT_ACTIVE),
    S_EDITOR_THEME_COLOR(CONTROL_TEXT_ACTIVE_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_TEXT_DISABLED),
    S_EDITOR_THEME_COLOR(CONTROL_BORDER),
    S_EDITOR_THEME_COLOR(CONTROL_BORDER_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_BORDER_ACTIVE),
    S_EDITOR_THEME_COLOR(CONTROL_BORDER_ACTIVE_HOVERED),
    S_EDITOR_THEME_COLOR(CONTROL_BORDER_DISABLED),
    S_EDITOR_THEME_COLOR(BORDER),
    S_EDITOR_THEME_COLOR(FOCUS),
    S_EDITOR_THEME_COLOR(SLIDER_TRACK),
    S_EDITOR_THEME_COLOR(SLIDER_TRACK_HOVERED),
    S_EDITOR_THEME_COLOR(SLIDER_TRACK_ACTIVE),
    S_EDITOR_THEME_COLOR(SLIDER_FILL),
    S_EDITOR_THEME_COLOR(SLIDER_THUMB),
    S_EDITOR_THEME_COLOR(SLIDER_THUMB_HOVERED),
    S_EDITOR_THEME_COLOR(SLIDER_THUMB_ACTIVE),
    S_EDITOR_THEME_COLOR(TITLE),
    S_EDITOR_THEME_COLOR(TITLE_BAR),
    S_EDITOR_THEME_COLOR(TITLE_BAR_FOCUSED),
    S_EDITOR_THEME_COLOR(SCROLLBAR_TRACK),
    S_EDITOR_THEME_COLOR(SCROLLBAR_THUMB),
    S_EDITOR_THEME_COLOR(SCROLLBAR_THUMB_HOVERED),
    S_EDITOR_THEME_COLOR(SCROLLBAR_THUMB_ACTIVE),
    S_EDITOR_THEME_COLOR(TAB_BAR_BG),
    S_EDITOR_THEME_COLOR(TAB_BAR_SEPARATOR),
    S_EDITOR_THEME_COLOR(TAB_BG),
    S_EDITOR_THEME_COLOR(TAB_BG_HOVERED),
    S_EDITOR_THEME_COLOR(TAB_TEXT),
    S_EDITOR_THEME_COLOR(TAB_TEXT_HOVERED),
    S_EDITOR_THEME_COLOR(TAB_BORDER),
    S_EDITOR_THEME_COLOR(TAB_BORDER_HOVERED),
    S_EDITOR_THEME_COLOR(TAB_ACTIVE_BG),
    S_EDITOR_THEME_COLOR(TAB_ACTIVE_TEXT),
    S_EDITOR_THEME_COLOR(TAB_ACTIVE_BORDER),
    S_EDITOR_THEME_COLOR(SEPARATOR),
    S_EDITOR_THEME_COLOR(INPUT_BORDER),
    S_EDITOR_THEME_COLOR(INPUT_BG),
    S_EDITOR_THEME_COLOR(INPUT_BG_HOVERED),
    S_EDITOR_THEME_COLOR(INPUT_BG_ACTIVE),
    S_EDITOR_THEME_COLOR(INPUT_BG_ACTIVE_HOVERED),
};

#undef S_EDITOR_THEME_COLOR

typedef char LDKEditorThemeColorSlotCoverage[
    sizeof(s_editor_theme_colors) / sizeof(s_editor_theme_colors[0]) ==
            LDK_UI_COLOR_COUNT
        ? 1
        : -1];

typedef struct LDKEditorThemeMetricEntry
{
  const char *name;
  size_t offset;
  bool positive;
} LDKEditorThemeMetricEntry;

#define S_EDITOR_THEME_METRIC(name, positive)                                \
  {#name, offsetof(LDKUITheme, name), positive}

static const LDKEditorThemeMetricEntry s_editor_theme_metrics[] = {
    S_EDITOR_THEME_METRIC(control_border_size, false),
    S_EDITOR_THEME_METRIC(input_border_size, false),
    S_EDITOR_THEME_METRIC(window_border_size, false),
    S_EDITOR_THEME_METRIC(window_interaction_border_size, false),
    S_EDITOR_THEME_METRIC(slider_track_height, false),
    S_EDITOR_THEME_METRIC(slider_thumb_width, false),
    S_EDITOR_THEME_METRIC(text_cursor_blink_interval, true),
    S_EDITOR_THEME_METRIC(text_cursor_width, false),
    S_EDITOR_THEME_METRIC(text_cursor_padding_y, false),
    S_EDITOR_THEME_METRIC(panel_padding, false),
};

#undef S_EDITOR_THEME_METRIC

typedef char LDKEditorThemeMetricCoverage[
    sizeof(s_editor_theme_metrics) / sizeof(s_editor_theme_metrics[0]) ==
            LDK_EDITOR_THEME_METRIC_COUNT
        ? 1
        : -1];

typedef struct LDKEditorThemeEntry
{
  XFSPath path;
  XSmallstr filename;
  char name[128];
  bool valid;
} LDKEditorThemeEntry;

struct LDKEditorThemeCatalog
{
  XFSPath directory;
  XFSPath config_path;
  XArray *entries;
  XSmallstr active;
  bool event_handler_registered;
};

typedef struct LDKEditorThemeEditorState
{
  LDKUITheme theme;
  LDKUITheme snapshot;
  XSmallstr source;
  LDKUIPoint scroll;
  char name[128];
  char filename[X_SMALLSTR_MAX_LENGTH];
  char metric_text[LDK_EDITOR_THEME_METRIC_COUNT][32];
  char status[256];
  bool initialized;
  bool dirty;
} LDKEditorThemeEditorState;

static LDKEditorThemeEditorState s_editor_theme_editor;

static void s_editor_theme_editor_show(LDKEditorContext *editor);
static void s_editor_theme_editor_close(LDKEditorContext *editor);
static bool s_editor_theme_event(const LDKEvent *event, void *optional);

static bool s_editor_theme_filename_valid(const char *filename)
{
  size_t length;

  if (filename == NULL)
  {
    return false;
  }

  length = strlen(filename);
  if (length < 5 || length + sizeof("themes/") > X_SMALLSTR_MAX_LENGTH)
  {
    return false;
  }

  for (size_t i = 0; i < length; ++i)
  {
    unsigned char c = (unsigned char)filename[i];
    if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' ||
        c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*')
    {
      return false;
    }
  }

  return filename[length - 4] == '.' &&
         tolower((unsigned char)filename[length - 3]) == 't' &&
         tolower((unsigned char)filename[length - 2]) == 'm' &&
         tolower((unsigned char)filename[length - 1]) == 'l';
}

static bool s_editor_theme_path_make(
    const XFSPath *directory, const char *filename, XFSPath *out)
{
  if (directory == NULL || out == NULL ||
      !s_editor_theme_filename_valid(filename))
  {
    return false;
  }

  if (directory->length + 1 + strlen(filename) >= sizeof(out->buf))
  {
    return false;
  }

  *out = *directory;
  return x_fs_path_join(out, filename) != 0;
}

static bool s_editor_theme_identifier_make(
    const char *filename, XSmallstr *out)
{
  XFSPath path = {0};

  if (out == NULL || !s_editor_theme_filename_valid(filename) ||
      !x_fs_path(&path, "themes", filename))
  {
    return false;
  }

  *out = path;
  return true;
}

static void s_editor_theme_message(LDKEditorContext *editor,
    const char *filename, const char *diagnostic, bool error)
{
  char message[1024];
  snprintf(message, sizeof(message), "Theme '%s': %s",
      filename != NULL ? filename : "", diagnostic != NULL ? diagnostic : "");

  if (error)
  {
    ldki_editor_log_error(editor, message);
  }
  else
  {
    ldki_editor_log_warning(editor, message);
  }
}

static bool s_editor_theme_selection_write(
    const XFSPath *path, const char *identifier)
{
  XIni ini = {0};
  XIniError error = {0};
  bool result;

  if (path == NULL || identifier == NULL)
  {
    return false;
  }

  if (!x_ini_load_file(path->buf, &ini, &error))
  {
    return false;
  }

  result = x_ini_set(&ini, ".editor", "theme", identifier) &&
           x_ini_write_file(path->buf, &ini, &error);
  x_ini_free(&ini);
  return result;
}

static bool s_editor_theme_apply(LDKEditorContext *editor,
    const char *identifier, XSmallstr *out_identifier)
{
  LDKUIThemeFile loaded;
  LDKUITheme theme;
  XFSPath path;
  XFSPath relative_path = {0};
  XSmallstr canonical = {0};
  char diagnostic[512] = {0};
  LDKEditorThemeCatalog *catalog;

  if (editor == NULL || identifier == NULL || out_identifier == NULL ||
      editor->theme_catalog == NULL)
  {
    return false;
  }

  catalog = editor->theme_catalog;
  if (strcmp(identifier, "dark") == 0 || strcmp(identifier, "light") == 0)
  {
    LDKUIThemeType type = strcmp(identifier, "light") == 0
                              ? LDK_UI_THEME_DEFAULT_LIGHT
                              : LDK_UI_THEME_DEFAULT_DARK;
    if (!ldk_ui_theme_get(type, &theme))
    {
      return false;
    }
    x_smallstr_from_cstr(&canonical, identifier);
  }
  else
  {
    if (!x_fs_path(&relative_path, identifier) ||
        !x_fs_path_is_relative(&relative_path))
    {
      s_editor_theme_message(editor, identifier,
          "Theme path must be relative to editor.ini.", true);
      return false;
    }

    x_fs_path_normalize(&relative_path);
    x_fs_path_dirname(&catalog->config_path, &path);
    if (!x_fs_path_join(&path, relative_path.buf))
    {
      s_editor_theme_message(editor, identifier, "Invalid theme path.", true);
      return false;
    }

    canonical = relative_path;
    if (!ldk_ui_theme_tml_load(
            path.buf, &loaded, diagnostic, sizeof(diagnostic)))
    {
      s_editor_theme_message(editor, identifier, diagnostic, true);
      return false;
    }
    theme = loaded.theme;
  }

  ldki_editor_theme_icons_set(editor, &theme);
  if (!ldk_ui_theme_set(&editor->ui, &theme))
  {
    s_editor_theme_message(
        editor, identifier, "Could not apply the theme.", true);
    return false;
  }

  *out_identifier = canonical;
  return true;
}

bool ldki_editor_theme_select(
    LDKEditorContext *editor, const char *identifier, bool persist)
{
  XSmallstr canonical = {0};
  LDKEditorThemeCatalog *catalog;

  if (editor == NULL || editor->theme_catalog == NULL || identifier == NULL)
  {
    return false;
  }

  catalog = editor->theme_catalog;
  if (!s_editor_theme_apply(editor, identifier, &canonical))
  {
    return false;
  }

  catalog->active = canonical;
  editor->editor_theme = canonical;
  memset(&s_editor_theme_editor, 0, sizeof(s_editor_theme_editor));

  if (persist &&
      !s_editor_theme_selection_write(&catalog->config_path, canonical.buf))
  {
    ldki_editor_log_warning(editor,
        "Theme applied, but the selection could not be saved.");
  }

  return true;
}

static bool s_editor_theme_entry_add(XArray *entries,
    const XFSPath *directory, const char *filename)
{
  LDKEditorThemeEntry entry = {0};
  LDKUIThemeFile loaded;
  XSlice stem;
  char diagnostic[512] = {0};
  size_t length;

  if (!s_editor_theme_path_make(directory, filename, &entry.path))
  {
    // A filename that cannot fit in XFSPath is not a catalog failure.
    return true;
  }

  x_smallstr_from_cstr(&entry.filename, filename);
  stem = x_fs_path_stem_cstr(filename);
  length = stem.length < sizeof(entry.name) - 1 ? stem.length
                                                : sizeof(entry.name) - 1;
  memcpy(entry.name, stem.ptr, length);
  entry.name[length] = 0;

  entry.valid = ldk_ui_theme_tml_load(
      entry.path.buf, &loaded, diagnostic, sizeof(diagnostic));
  if (entry.valid && loaded.name[0] != 0)
  {
    memcpy(entry.name, loaded.name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
  }

  return x_array_add(entries, &entry) == XARRAY_OK;
}

static void s_editor_theme_entries_sort(XArray *entries)
{
  u32 count = x_array_count(entries);

  for (u32 i = 1; i < count; ++i)
  {
    for (u32 j = i; j > 0; --j)
    {
      LDKEditorThemeEntry *a = x_array_get(entries, j - 1);
      LDKEditorThemeEntry *b = x_array_get(entries, j);
      if (strcmp(a->filename.buf, b->filename.buf) <= 0)
      {
        break;
      }
      LDKEditorThemeEntry temp = *a;
      *a = *b;
      *b = temp;
    }
  }
}

bool ldki_editor_theme_refresh(LDKEditorContext *editor)
{
  LDKEditorThemeCatalog *catalog;
  XArray *entries;
  XFSDireEntry entry = {0};
  XFSDireHandle *handle;
  bool ok = true;

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return false;
  }

  catalog = editor->theme_catalog;
  entries = x_array_create(sizeof(LDKEditorThemeEntry), 16);
  if (entries == NULL)
  {
    ldki_editor_log_error(editor, "Failed to allocate the theme catalog.");
    return false;
  }

  if (!x_fs_path_is_directory(&catalog->directory) &&
      !x_fs_directory_create_recursive(catalog->directory.buf))
  {
    x_array_destroy(entries);
    ldki_editor_log_error(editor, "Failed to create the theme directory.");
    return false;
  }

  handle = x_fs_find_first_file(catalog->directory.buf, &entry);
  if (handle != NULL)
  {
    do
    {
      if (!entry.is_directory && s_editor_theme_filename_valid(entry.name))
      {
        if (!s_editor_theme_entry_add(entries, &catalog->directory, entry.name))
        {
          ok = false;
          break;
        }
      }
    } while (x_fs_find_next_file(handle, &entry));
    x_fs_find_close(handle);
  }
  else if (!x_fs_path_is_directory(&catalog->directory))
  {
    ok = false;
  }

  if (!ok)
  {
    x_array_destroy(entries);
    ldki_editor_log_error(editor, "Failed to refresh the theme catalog.");
    return false;
  }

  s_editor_theme_entries_sort(entries);
  if (catalog->entries != NULL)
  {
    x_array_destroy(catalog->entries);
  }
  catalog->entries = entries;
  return true;
}

static void s_editor_theme_defaults_install(LDKEditorContext *editor)
{
  static const char *filenames[] = {"ldk_dark.tml", "ldk_light.tml"};
  LDKEditorThemeCatalog *catalog = editor->theme_catalog;

  for (u32 i = 0; i < sizeof(filenames) / sizeof(filenames[0]); ++i)
  {
    XFSPath source = {0};
    XFSPath destination = {0};

    if (!x_fs_path(
            &source, editor->engine_runtree.buf, "themes", filenames[i]) ||
        !s_editor_theme_path_make(
            &catalog->directory, filenames[i], &destination))
    {
      continue;
    }

    if (!x_fs_path_exists(&destination) && x_fs_path_is_file(&source) &&
        !x_fs_file_copy(source.buf, destination.buf))
    {
      s_editor_theme_message(
          editor, filenames[i], "Could not install the bundled theme.", false);
    }
  }
}

static void s_editor_theme_editor_window(LDKEditor *opaque_editor, void *data)
{
  (void)data;
  s_editor_theme_editor_show((LDKEditorContext *)opaque_editor);
}

static void s_editor_theme_editor_register(LDKEditorContext *editor)
{
  LDKEditorWindow window = {.id = LDK_EDITOR_WINDOW_THEME_EDITOR,
      .title = "Theme Editor",
      .function = s_editor_theme_editor_window,
      .data = NULL};

  if (!ldk_editor_window_add((LDKEditor *)editor, &window))
  {
    ldki_editor_log_warning(
        editor, "Could not register the Theme Editor window.");
    return;
  }

  if (!ldki_editor_window_hide(LDK_EDITOR_WINDOW_THEME_EDITOR))
  {
    ldki_editor_log_warning(editor,
        "Theme Editor was registered but could not be hidden at startup.");
  }
}

bool ldki_editor_theme_initialize(
    LDKEditorContext *editor, const char *config_directory)
{
  LDKEditorThemeCatalog *catalog;
  LDKEventQueue *events;
  XSmallstr requested = {0};

  if (editor == NULL || config_directory == NULL || config_directory[0] == 0 ||
      editor->theme_catalog != NULL)
  {
    return false;
  }

  catalog = calloc(1, sizeof(*catalog));
  if (catalog == NULL)
  {
    return false;
  }

  if (!x_fs_path(&catalog->directory, config_directory, "themes") ||
      !x_fs_path(&catalog->config_path, config_directory,
          LDK_EDITOR_CONFIG_FILE))
  {
    free(catalog);
    return false;
  }

  editor->theme_catalog = catalog;
  if (ldki_editor_theme_refresh(editor))
  {
    s_editor_theme_defaults_install(editor);
    ldki_editor_theme_refresh(editor);
  }
  // Filesystem errors do not prevent the built-in themes from working.

  requested = editor->editor_theme;

  if (!ldki_editor_theme_select(editor, requested.buf, false))
  {
    ldki_editor_log_warning(editor, "Using the built-in Dark theme.");
    if (!ldki_editor_theme_select(editor, "dark", false))
    {
      ldki_editor_theme_terminate(editor);
      return false;
    }
  }

  s_editor_theme_editor_register(editor);

  events = ldk_module_get(LDK_MODULE_EVENT);
  if (events != NULL)
  {
    ldk_event_handler_add(
        events, s_editor_theme_event, LDK_EVENT_TYPE_CUSTOM, editor);
    catalog->event_handler_registered = true;
  }
  else
  {
    ldki_editor_log_warning(
        editor, "Theme Editor could not register its event handler.");
  }

  return true;
}

void ldki_editor_theme_terminate(LDKEditorContext *editor)
{
  LDKEventQueue *events;

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return;
  }

  s_editor_theme_editor_close(editor);

  if (editor->theme_catalog->event_handler_registered)
  {
    events = ldk_module_get(LDK_MODULE_EVENT);
    if (events != NULL)
    {
      ldk_event_handler_remove(events, s_editor_theme_event);
    }
  }

  (void)ldki_editor_window_remove(LDK_EDITOR_WINDOW_THEME_EDITOR);

  if (editor->theme_catalog->entries != NULL)
  {
    x_array_destroy(editor->theme_catalog->entries);
  }
  free(editor->theme_catalog);
  editor->theme_catalog = NULL;
  memset(&s_editor_theme_editor, 0, sizeof(s_editor_theme_editor));
}

static bool s_editor_theme_menu_button(LDKEditorContext *editor,
    const char *identifier, const char *name)
{
  char label[512];
  LDKUIContext *ui = &editor->ui;
  bool selected = strcmp(editor->theme_catalog->active.buf, identifier) == 0;

  snprintf(label, sizeof(label), "%s%s", selected ? "* " : "  ", name);
  ldk_ui_push_id_cstr(ui, identifier);
  bool clicked = ldk_ui_button_flat(ui, label);
  ldk_ui_pop_id(ui);

  if (clicked && ldki_editor_theme_select(editor, identifier, true))
  {
    ldk_ui_close_current_popup(ui);
    return true;
  }

  return false;
}

void ldki_editor_theme_menu_show(LDKEditorContext *editor)
{
  static LDKUIPoint scroll = {0};

  LDKEditorThemeCatalog *catalog;
  LDKUIContext *ui;
  u32 count;
  bool use_scroll = false;
  float scroll_height = 0.0f;
  float scroll_width = 160.0f;

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return;
  }

  catalog = editor->theme_catalog;
  ui = &editor->ui;

  s_editor_theme_menu_button(editor, "dark", "Dark");
  s_editor_theme_menu_button(editor, "light", "Light");

  count = x_array_count(catalog->entries);
  if (count > 0)
  {
    const float popup_top =
        LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING;
    const float popup_available_height =
        ui->viewport.h - popup_top - LDK_UI_DEFAULT_PADDING;

    const float full_menu_height =
        LDK_UI_DEFAULT_PADDING * 2.0f +
        LDK_UI_DEFAULT_CONTROL_HEIGHT * (float)(count + 4) + 2.0f +
        LDK_UI_DEFAULT_SPACING * (float)(count + 5);

    const float fixed_scroll_menu_height =
        LDK_UI_DEFAULT_PADDING * 2.0f +
        LDK_UI_DEFAULT_CONTROL_HEIGHT * 4.0f + 2.0f +
        LDK_UI_DEFAULT_SPACING * 6.0f;

    const float minimum_scroll_height =
        LDK_UI_DEFAULT_CONTROL_HEIGHT + LDK_UI_DEFAULT_PADDING * 2.0f;

    scroll_height = popup_available_height - fixed_scroll_menu_height;

    if (scroll_height < minimum_scroll_height)
    {
      scroll_height = minimum_scroll_height;
    }

    use_scroll = full_menu_height > popup_available_height;

    ldk_ui_horizontal_line(ui);
  }

  if (use_scroll)
  {
    if (ui->font != NULL)
    {
      for (u32 i = 0; i < count; ++i)
      {
        const LDKEditorThemeEntry *entry = x_array_get(catalog->entries, i);
        XSmallstr identifier = {0};
        char label[192];
        char button_label[512];

        if (entry == NULL ||
            !s_editor_theme_identifier_make(entry->filename.buf, &identifier))
        {
          continue;
        }

        snprintf(label, sizeof(label), "%s%s", entry->name,
            entry->valid ? "" : " (invalid)");

        bool selected = strcmp(catalog->active.buf, identifier.buf) == 0;

        snprintf(button_label, sizeof(button_label), "%s%s",
            selected ? "* " : "  ", label);

        LDKTextSize text_size =
            ldk_ttf_measure_text_cstr(ui->font, button_label);

        float width = text_size.w + LDK_UI_DEFAULT_SPACING * 4.0f +
                      LDK_UI_DEFAULT_PADDING * 2.0f + LDK_UI_SCROLLBAR_SIZE;

        if (width > scroll_width)
        {
          scroll_width = width;
        }
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(scroll_width));
    ldk_ui_set_next_height(ui, ldk_ui_px(scroll_height));

    scroll = ldk_ui_begin_scrollview(
        ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  }
  else
  {
    scroll = (LDKUIPoint){0};
  }

  for (u32 i = 0; i < count; ++i)
  {
    const LDKEditorThemeEntry *entry = x_array_get(catalog->entries, i);
    XSmallstr identifier = {0};
    char label[192];

    if (entry == NULL ||
        !s_editor_theme_identifier_make(entry->filename.buf, &identifier))
    {
      continue;
    }

    snprintf(label, sizeof(label), "%s%s", entry->name,
        entry->valid ? "" : " (invalid)");

    s_editor_theme_menu_button(editor, identifier.buf, label);
  }

  if (use_scroll)
  {
    ldk_ui_end_scrollview(ui);
  }

  ldk_ui_horizontal_line(ui);

  if (ldk_ui_button_flat(ui, "Edit themes..."))
  {
    ldki_editor_window_show(LDK_EDITOR_WINDOW_THEME_EDITOR);
    ldk_ui_close_current_popup(ui);
  }

  if (ldk_ui_button_flat(ui, "Refresh themes"))
  {
    ldki_editor_theme_refresh(editor);
  }

  if (ldk_ui_button_flat(ui, "Reload current theme"))
  {
    XSmallstr current = catalog->active;
    ldki_editor_theme_select(editor, current.buf, false);
  }
}

static const LDKEditorThemeEntry *s_editor_theme_active_entry_get(
    LDKEditorThemeCatalog *catalog)
{
  if (catalog == NULL || catalog->entries == NULL)
  {
    return NULL;
  }

  u32 count = x_array_count(catalog->entries);
  for (u32 i = 0; i < count; ++i)
  {
    const LDKEditorThemeEntry *entry = x_array_get(catalog->entries, i);
    XSmallstr identifier = {0};

    if (entry != NULL &&
        s_editor_theme_identifier_make(entry->filename.buf, &identifier) &&
        strcmp(identifier.buf, catalog->active.buf) == 0)
    {
      return entry;
    }
  }

  return NULL;
}

static void s_editor_theme_metric_text_reset(LDKEditorThemeEditorState *state)
{
  if (state == NULL)
  {
    return;
  }

  for (u32 i = 0; i < LDK_EDITOR_THEME_METRIC_COUNT; ++i)
  {
    const LDKEditorThemeMetricEntry *metric = &s_editor_theme_metrics[i];
    const float *value =
        (const float *)((const char *)&state->theme + metric->offset);
    snprintf(state->metric_text[i], sizeof(state->metric_text[i]), "%.9g",
        (double)*value);
  }
}

static void s_editor_theme_filename_suggest(
    LDKEditorThemeEditorState *state, LDKEditorThemeCatalog *catalog)
{
  const LDKEditorThemeEntry *entry;

  if (state == NULL || catalog == NULL)
  {
    return;
  }

  if (strcmp(catalog->active.buf, "dark") == 0)
  {
    snprintf(state->filename, sizeof(state->filename), "dark_custom.tml");
    return;
  }

  if (strcmp(catalog->active.buf, "light") == 0)
  {
    snprintf(state->filename, sizeof(state->filename), "light_custom.tml");
    return;
  }

  entry = s_editor_theme_active_entry_get(catalog);
  if (entry != NULL)
  {
    XSlice stem = x_fs_path_stem_cstr(entry->filename.buf);
    int length = stem.length < sizeof(state->filename) - sizeof("_copy.tml")
                     ? (int)stem.length
                     : (int)(sizeof(state->filename) - sizeof("_copy.tml"));
    snprintf(state->filename, sizeof(state->filename), "%.*s_copy.tml", length,
        stem.ptr);
    return;
  }

  snprintf(state->filename, sizeof(state->filename), "theme_copy.tml");
}

static void s_editor_theme_editor_sync(LDKEditorContext *editor)
{
  LDKEditorThemeCatalog *catalog;
  const LDKEditorThemeEntry *entry;
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return;
  }

  catalog = editor->theme_catalog;
  memset(state, 0, sizeof(*state));
  state->theme = editor->ui.theme;
  state->snapshot = editor->ui.theme;
  state->source = catalog->active;

  if (strcmp(catalog->active.buf, "dark") == 0)
  {
    snprintf(state->name, sizeof(state->name), "LDK Dark");
  }
  else if (strcmp(catalog->active.buf, "light") == 0)
  {
    snprintf(state->name, sizeof(state->name), "LDK Light");
  }
  else
  {
    entry = s_editor_theme_active_entry_get(catalog);
    snprintf(state->name, sizeof(state->name), "%s",
        entry != NULL && entry->name[0] != 0 ? entry->name : "Custom Theme");
  }

  s_editor_theme_filename_suggest(state, catalog);
  s_editor_theme_metric_text_reset(state);
  state->initialized = true;
}

static bool s_editor_theme_editor_apply(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;

  if (editor == NULL)
  {
    return false;
  }

  ldki_editor_theme_icons_set(editor, &state->theme);
  if (!ldk_ui_theme_set(&editor->ui, &state->theme))
  {
    snprintf(state->status, sizeof(state->status),
        "Could not apply the edited theme.");
    return false;
  }

  state->dirty = true;
  state->status[0] = 0;
  return true;
}

static bool s_editor_theme_editor_snapshot_restore(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKUITheme theme;

  if (editor == NULL || !state->initialized)
  {
    return false;
  }

  theme = state->snapshot;
  ldki_editor_theme_icons_set(editor, &theme);
  if (!ldk_ui_theme_set(&editor->ui, &theme))
  {
    snprintf(state->status, sizeof(state->status),
        "Could not restore the active theme.");
    return false;
  }

  return true;
}

static void s_editor_theme_editor_close(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;

  if (editor == NULL || !state->initialized)
  {
    return;
  }

  if (state->dirty && !s_editor_theme_editor_snapshot_restore(editor))
  {
    ldki_editor_log_warning(
        editor, "Theme Editor could not restore the active theme on close.");
  }

  memset(state, 0, sizeof(*state));
}

static bool s_editor_theme_event(const LDKEvent *event, void *optional)
{
  LDKEditorContext *editor = (LDKEditorContext *)optional;

  if (event == NULL || editor == NULL || editor->theme_catalog == NULL ||
      event->type != LDK_EVENT_TYPE_CUSTOM ||
      event->custom_event.tag != (u32)LDK_EDITOR_EVENT_WINDOW_CLOSED ||
      event->custom_event.sender != (u32)LDK_EDITOR_WINDOW_THEME_EDITOR)
  {
    return false;
  }

  if (!ldki_editor_window_is_open(LDK_EDITOR_WINDOW_THEME_EDITOR))
  {
    s_editor_theme_editor_close(editor);
  }

  return false;
}

static bool s_editor_theme_number_parse(
    const char *text, bool positive, float *out_value)
{
  char *end = NULL;
  float value;

  if (text == NULL || out_value == NULL)
  {
    return false;
  }

  value = strtof(text, &end);
  if (end == text)
  {
    return false;
  }

  while (*end != 0 && isspace((unsigned char)*end))
  {
    ++end;
  }

  if (*end != 0 || !isfinite(value) || value < 0.0f ||
      (positive && value <= 0.0f))
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_editor_theme_string_write(FILE *file, const char *value)
{
  if (file == NULL || value == NULL || fputc('"', file) == EOF)
  {
    return false;
  }

  for (const unsigned char *cursor = (const unsigned char *)value; *cursor != 0;
      ++cursor)
  {
    switch (*cursor)
    {
    case '\\':
      if (fputs("\\\\", file) == EOF)
      {
        return false;
      }
      break;
    case '"':
      if (fputs("\\\"", file) == EOF)
      {
        return false;
      }
      break;
    case '\n':
      if (fputs("\\n", file) == EOF)
      {
        return false;
      }
      break;
    case '\r':
      if (fputs("\\r", file) == EOF)
      {
        return false;
      }
      break;
    case '\t':
      if (fputs("\\t", file) == EOF)
      {
        return false;
      }
      break;
    default:
      if (*cursor < 32 || *cursor == 127 || fputc(*cursor, file) == EOF)
      {
        return false;
      }
      break;
    }
  }

  return fputc('"', file) != EOF;
}

static bool s_editor_theme_file_write(
    const XFSPath *path, const char *name, const LDKUITheme *theme)
{
  FILE *file;
  bool ok = true;

  if (path == NULL || name == NULL || name[0] == 0 || theme == NULL)
  {
    return false;
  }

  file = fopen(path->buf, "wb");
  if (file == NULL)
  {
    return false;
  }

  ok = fprintf(file,
           "# LDK UI theme format, version 1.\n"
           "# Generated by the LDK Theme Editor.\n"
           "# This is a resolved theme: COLOR_* values are written "
           "explicitly.\n"
           "ldk_editor_theme:\n"
           "    version: 1\n"
           "    base: \"dark\"\n"
           "    theme_name: ") >= 0 &&
       s_editor_theme_string_write(file, name) && fputs("\n\n", file) != EOF;

  for (u32 i = 0;
       ok && i <
                 sizeof(s_editor_theme_colors) /
                     sizeof(s_editor_theme_colors[0]);
       ++i)
  {
    const LDKEditorThemeColorEntry *entry = &s_editor_theme_colors[i];
    ok = fprintf(file, "    COLOR_%s: 0x%08X\n", entry->name,
             (unsigned int)theme->colors[entry->slot]) >= 0;
  }

  if (ok)
  {
    ok = fputs("\n", file) != EOF;
  }

  for (u32 i = 0; ok && i < LDK_EDITOR_THEME_METRIC_COUNT; ++i)
  {
    const LDKEditorThemeMetricEntry *metric = &s_editor_theme_metrics[i];
    const float *value =
        (const float *)((const char *)theme + metric->offset);
    ok = fprintf(file, "    %s: %.9g\n", metric->name, (double)*value) >= 0;
  }

  if (ok)
  {
    ok = fprintf(file, "    text_cursor_blink: %s\n",
             theme->text_cursor_blink ? "true" : "false") >= 0;
  }

  if (fclose(file) != 0)
  {
    ok = false;
  }

  if (!ok)
  {
    remove(path->buf);
  }

  return ok;
}

static bool s_editor_theme_filename_normalize(
    const char *filename, char *out, size_t out_size)
{
  size_t length;

  if (filename == NULL || out == NULL || out_size == 0)
  {
    return false;
  }

  length = strlen(filename);
  if (length == 0 || length >= out_size)
  {
    return false;
  }

  snprintf(out, out_size, "%s", filename);
  if (length < 4 || out[length - 4] != '.' ||
      tolower((unsigned char)out[length - 3]) != 't' ||
      tolower((unsigned char)out[length - 2]) != 'm' ||
      tolower((unsigned char)out[length - 1]) != 'l')
  {
    if (length + 4 >= out_size)
    {
      return false;
    }
    memcpy(out + length, ".tml", 5);
  }

  return s_editor_theme_filename_valid(out);
}

static bool s_editor_theme_editor_save_as(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKEditorThemeCatalog *catalog;
  XFSPath path = {0};
  XSmallstr identifier = {0};
  char filename[X_SMALLSTR_MAX_LENGTH];

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return false;
  }

  catalog = editor->theme_catalog;
  if (state->name[0] == 0)
  {
    snprintf(
        state->status, sizeof(state->status), "Theme name cannot be empty.");
    return false;
  }

  for (u32 i = 0; i < LDK_EDITOR_THEME_METRIC_COUNT; ++i)
  {
    const LDKEditorThemeMetricEntry *metric = &s_editor_theme_metrics[i];
    float value;
    if (!s_editor_theme_number_parse(
            state->metric_text[i], metric->positive, &value))
    {
      snprintf(state->status, sizeof(state->status),
          "%s must be a finite %snumber before saving.", metric->name,
          metric->positive ? "positive " : "nonnegative ");
      return false;
    }

    float *target = (float *)((char *)&state->theme + metric->offset);
    *target = value;
  }

  if (!s_editor_theme_filename_normalize(
          state->filename, filename, sizeof(filename)) ||
      !s_editor_theme_path_make(&catalog->directory, filename, &path))
  {
    snprintf(state->status, sizeof(state->status), "Invalid theme filename.");
    return false;
  }

  if (x_fs_path_exists(&path))
  {
    snprintf(state->status, sizeof(state->status),
        "A theme named '%s' already exists. Save As never overwrites.",
        filename);
    return false;
  }

  if (!s_editor_theme_file_write(&path, state->name, &state->theme))
  {
    snprintf(state->status, sizeof(state->status),
        "Could not write theme '%s'.", filename);
    s_editor_theme_message(
        editor, filename, "Could not write theme file.", true);
    return false;
  }

  if (!ldki_editor_theme_refresh(editor) ||
      !s_editor_theme_identifier_make(filename, &identifier) ||
      !ldki_editor_theme_select(editor, identifier.buf, true))
  {
    snprintf(state->status, sizeof(state->status),
        "Theme was written, but could not be activated.");
    return false;
  }

  s_editor_theme_editor_sync(editor);
  snprintf(
      state->status, sizeof(state->status), "Saved as themes/%s", filename);
  ldki_editor_log_info(editor, state->status);
  return true;
}

static void s_editor_theme_row_begin(LDKUIContext *ui, const char *label)
{
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(230.0f));
  ldk_ui_label(ui, label != NULL ? label : "");
}

static void s_editor_theme_editor_identity_show(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKUIContext *ui = &editor->ui;

  s_editor_theme_row_begin(ui, "Source");
  ldk_ui_label(ui, state->source.buf);
  ldk_ui_end_horizontal(ui);

  s_editor_theme_row_begin(ui, "Theme name");
  if ((ldk_ui_input_box(ui, state->name, (u32)sizeof(state->name)) &
          LDK_UI_INPUT_BOX_CHANGED) != 0)
  {
    state->dirty = true;
  }
  ldk_ui_end_horizontal(ui);

  s_editor_theme_row_begin(ui, "Save as");
  ldk_ui_input_box(ui, state->filename, (u32)sizeof(state->filename));
  ldk_ui_end_horizontal(ui);

  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Revert"))
  {
    XSmallstr source = state->source;
    if (s_editor_theme_editor_snapshot_restore(editor))
    {
      s_editor_theme_editor_sync(editor);
      snprintf(
          state->status, sizeof(state->status), "Reverted to %s", source.buf);
    }
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "Save As"))
  {
    s_editor_theme_editor_save_as(editor);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);

  if (state->dirty)
  {
    ldk_ui_label(ui, "Unsaved preview changes");
  }
  if (state->status[0] != 0)
  {
    ldk_ui_label(ui, state->status);
  }
}

static void s_editor_theme_editor_colors_show(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKUIContext *ui = &editor->ui;

  for (u32 i = 0;
       i < sizeof(s_editor_theme_colors) / sizeof(s_editor_theme_colors[0]);
       ++i)
  {
    const LDKEditorThemeColorEntry *entry = &s_editor_theme_colors[i];
    rgba32 *color = &state->theme.colors[entry->slot];
    rgba32 previous = *color;

    ldk_ui_push_id_u32(ui, i + 1u);
    s_editor_theme_row_begin(ui, entry->name);
    (void)ldki_editor_color_field(editor, color, false);
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);

    if (*color != previous)
    {
      s_editor_theme_editor_apply(editor);
    }
  }
}

static void s_editor_theme_editor_metrics_show(LDKEditorContext *editor)
{
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKUIContext *ui = &editor->ui;

  for (u32 i = 0; i < LDK_EDITOR_THEME_METRIC_COUNT; ++i)
  {
    const LDKEditorThemeMetricEntry *metric = &s_editor_theme_metrics[i];
    u32 result;

    ldk_ui_push_id_u32(ui, i + 1u);
    s_editor_theme_row_begin(ui, metric->name);
    result = ldk_ui_input_box(
        ui, state->metric_text[i], (u32)sizeof(state->metric_text[i]));
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);

    if ((result & LDK_UI_INPUT_BOX_CHANGED) != 0)
    {
      float value;
      if (s_editor_theme_number_parse(
              state->metric_text[i], metric->positive, &value))
      {
        float *target = (float *)((char *)&state->theme + metric->offset);
        if (*target != value)
        {
          *target = value;
          s_editor_theme_editor_apply(editor);
        }
      }
    }

    if ((result & LDK_UI_INPUT_BOX_COMMITTED) != 0)
    {
      float value;
      if (!s_editor_theme_number_parse(
              state->metric_text[i], metric->positive, &value))
      {
        const float *target =
            (const float *)((const char *)&state->theme + metric->offset);
        snprintf(state->metric_text[i], sizeof(state->metric_text[i]), "%.9g",
            (double)*target);
        snprintf(state->status, sizeof(state->status),
            "%s must be a finite %snumber.", metric->name,
            metric->positive ? "positive " : "nonnegative ");
      }
      else
      {
        snprintf(state->metric_text[i], sizeof(state->metric_text[i]), "%.9g",
            (double)value);
      }
    }
  }

  ldk_ui_push_id_cstr(ui, "text_cursor_blink");
  s_editor_theme_row_begin(ui, "text_cursor_blink");
  ldk_ui_set_next_weight(ui, 0.0f);
  bool blink = ldk_ui_toggle(ui, state->theme.text_cursor_blink);
  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_pop_id(ui);

  if (blink != state->theme.text_cursor_blink)
  {
    state->theme.text_cursor_blink = blink;
    s_editor_theme_editor_apply(editor);
  }
}

static void s_editor_theme_editor_show(LDKEditorContext *editor)
{
  static bool colors_expanded = true;
  static bool metrics_expanded = true;
  LDKEditorThemeEditorState *state = &s_editor_theme_editor;
  LDKUIContext *ui;

  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return;
  }

  if (!state->initialized ||
      strcmp(state->source.buf, editor->theme_catalog->active.buf) != 0)
  {
    s_editor_theme_editor_sync(editor);
  }

  ui = &editor->ui;
  state->scroll = ldk_ui_begin_scrollview(
      ui, state->scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  s_editor_theme_editor_identity_show(editor);

  ldk_ui_spacer(ui);
  colors_expanded = ldk_ui_tree_node(ui, "Colors", colors_expanded, 0, 0);
  if (colors_expanded)
  {
    s_editor_theme_editor_colors_show(editor);
  }

  ldk_ui_spacer(ui);
  metrics_expanded = ldk_ui_tree_node(ui, "Metrics", metrics_expanded, 0, 0);
  if (metrics_expanded)
  {
    s_editor_theme_editor_metrics_show(editor);
  }

  ldk_ui_end_scrollview(ui);
}
