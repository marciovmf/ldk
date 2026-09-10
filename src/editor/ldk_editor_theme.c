#include "ldk_editor_internal.h"
#include "ldk_editor_theme.h"
#include <module/ldk_ui.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_io.h>
#include <stdx/stdx_string.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_THEME_SELECTION_FILE "theme.txt"
#define LDK_EDITOR_THEME_FILE_PREFIX "file:"

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
  XFSPath selection_path;
  XArray *entries;
  XSmallstr active;
};

static bool s_editor_theme_filename_valid(const char *filename)
{
  size_t length;

  if (filename == NULL)
  {
    return false;
  }

  length = strlen(filename);
  if (length < 5 || length + sizeof(LDK_EDITOR_THEME_FILE_PREFIX) >
                        X_SMALLSTR_MAX_LENGTH)
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
  int length;

  if (out == NULL || !s_editor_theme_filename_valid(filename))
  {
    return false;
  }

  length = snprintf(out->buf, sizeof(out->buf),
      LDK_EDITOR_THEME_FILE_PREFIX "%s", filename);
  if (length < 0 || (size_t)length >= sizeof(out->buf))
  {
    return false;
  }

  out->length = (size_t)length;
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

static bool s_editor_theme_selection_read(
    const XFSPath *path, XSmallstr *out)
{
  XFile *file;
  char text[X_SMALLSTR_MAX_LENGTH + 1];
  char *first;
  char *last;
  size_t size;
  size_t length;
  bool ok;

  if (path == NULL || out == NULL)
  {
    return false;
  }

  file = x_io_open(path->buf, "rb");
  if (file == NULL)
  {
    return false;
  }

  size = x_io_read(file, text, sizeof(text));
  ok = !x_io_error(file) && size < sizeof(text);
  x_io_close(file);
  if (!ok || memchr(text, 0, size) != NULL)
  {
    return false;
  }

  text[size] = 0;
  first = text;
  last = text + size;
  while (first < last && isspace((unsigned char)*first))
  {
    ++first;
  }
  while (last > first && isspace((unsigned char)last[-1]))
  {
    --last;
  }

  length = (size_t)(last - first);
  if (length == 0 || length >= sizeof(out->buf))
  {
    return false;
  }

  memcpy(out->buf, first, length);
  out->buf[length] = 0;
  out->length = length;
  return true;
}

static bool s_editor_theme_selection_write(
    const XFSPath *path, const char *identifier)
{
  char text[X_SMALLSTR_MAX_LENGTH + 2];
  int length;

  if (path == NULL || identifier == NULL)
  {
    return false;
  }

  length = snprintf(text, sizeof(text), "%s\n", identifier);
  if (length < 0 || (size_t)length >= sizeof(text))
  {
    return false;
  }

  return x_io_write_text(path->buf, text);
}

static bool s_editor_theme_apply(LDKEditorContext *editor,
    const char *identifier, XSmallstr *out_identifier)
{
  LDKUIThemeFile loaded;
  LDKUITheme theme;
  XFSPath path;
  XSmallstr canonical = {0};
  const char *filename;
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
    filename = identifier;
    if (strncmp(identifier, LDK_EDITOR_THEME_FILE_PREFIX,
            sizeof(LDK_EDITOR_THEME_FILE_PREFIX) - 1) == 0)
    {
      filename += sizeof(LDK_EDITOR_THEME_FILE_PREFIX) - 1;
    }

    if (!s_editor_theme_path_make(&catalog->directory, filename, &path) ||
        !s_editor_theme_identifier_make(filename, &canonical))
    {
      s_editor_theme_message(editor, identifier, "Invalid theme filename.", true);
      return false;
    }

    if (!ldk_ui_theme_tml_load(path.buf, &loaded, diagnostic, sizeof(diagnostic)))
    {
      s_editor_theme_message(editor, filename, diagnostic, true);
      return false;
    }
    theme = loaded.theme;
  }

  ldki_editor_theme_icons_set(editor, &theme);
  if (!ldk_ui_theme_set(&editor->ui, &theme))
  {
    s_editor_theme_message(editor, identifier, "Could not apply the theme.", true);
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

  if (persist && !s_editor_theme_selection_write(
                     &catalog->selection_path, canonical.buf))
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
  length = stem.length < sizeof(entry.name) - 1
               ? stem.length : sizeof(entry.name) - 1;
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

    if (!x_fs_path(&source, editor->engine_runtree.buf,
            "themes", filenames[i]) ||
        !s_editor_theme_path_make(&catalog->directory, filenames[i], &destination))
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

bool ldki_editor_theme_initialize(
    LDKEditorContext *editor, const char *config_directory)
{
  LDKEditorThemeCatalog *catalog;
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
      !x_fs_path(&catalog->selection_path, config_directory,
          LDK_EDITOR_THEME_SELECTION_FILE))
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
  if (x_fs_path_exists(&catalog->selection_path))
  {
    if (!s_editor_theme_selection_read(&catalog->selection_path, &requested))
    {
      ldki_editor_log_warning(editor,
          "Invalid saved theme selection. Using the configured default.");
      requested = editor->editor_theme;
    }
  }

  if (!ldki_editor_theme_select(editor, requested.buf, false))
  {
    ldki_editor_log_warning(editor, "Using the built-in Dark theme.");
    if (!ldki_editor_theme_select(editor, "dark", false))
    {
      ldki_editor_theme_terminate(editor);
      return false;
    }
  }

  return true;
}

void ldki_editor_theme_terminate(LDKEditorContext *editor)
{
  if (editor == NULL || editor->theme_catalog == NULL)
  {
    return;
  }

  if (editor->theme_catalog->entries != NULL)
  {
    x_array_destroy(editor->theme_catalog->entries);
  }
  free(editor->theme_catalog);
  editor->theme_catalog = NULL;
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
        LDK_UI_DEFAULT_CONTROL_HEIGHT * (float)(count + 4) +
        2.0f +
        LDK_UI_DEFAULT_SPACING * (float)(count + 5);

    const float fixed_scroll_menu_height =
        LDK_UI_DEFAULT_PADDING * 2.0f +
        LDK_UI_DEFAULT_CONTROL_HEIGHT * 4.0f +
        2.0f +
        LDK_UI_DEFAULT_SPACING * 6.0f;

    const float minimum_scroll_height =
        LDK_UI_DEFAULT_CONTROL_HEIGHT +
        LDK_UI_DEFAULT_PADDING * 2.0f;

    scroll_height =
        popup_available_height - fixed_scroll_menu_height;

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
        const LDKEditorThemeEntry *entry =
            x_array_get(catalog->entries, i);
        XSmallstr identifier = {0};
        char label[192];
        char button_label[512];

        if (entry == NULL ||
            !s_editor_theme_identifier_make(
                entry->filename.buf, &identifier))
        {
          continue;
        }

        snprintf(label, sizeof(label), "%s%s", entry->name,
            entry->valid ? "" : " (invalid)");

        bool selected =
            strcmp(catalog->active.buf, identifier.buf) == 0;

        snprintf(button_label, sizeof(button_label), "%s%s",
            selected ? "* " : "  ", label);

        LDKTextSize text_size =
            ldk_ttf_measure_text_cstr(ui->font, button_label);

        float width =
            text_size.w +
            LDK_UI_DEFAULT_SPACING * 4.0f +
            LDK_UI_DEFAULT_PADDING * 2.0f +
            LDK_UI_SCROLLBAR_SIZE;

        if (width > scroll_width)
        {
          scroll_width = width;
        }
      }
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(scroll_width));
    ldk_ui_set_next_height(ui, ldk_ui_px(scroll_height));

    scroll = ldk_ui_begin_scrollview(ui, scroll,
        LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  }
  else
  {
    scroll = (LDKUIPoint){0};
  }

  for (u32 i = 0; i < count; ++i)
  {
    const LDKEditorThemeEntry *entry =
        x_array_get(catalog->entries, i);
    XSmallstr identifier = {0};
    char label[192];

    if (entry == NULL ||
        !s_editor_theme_identifier_make(
            entry->filename.buf, &identifier))
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
