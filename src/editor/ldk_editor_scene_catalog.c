#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include <module/ldk_scene_manager.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct LDKEditorTagCatalogState
{
  XFSPath project_path;
  char names[LDK_EDITOR_TAG_COUNT][LDK_EDITOR_TAG_NAME_CAPACITY];
  char draft[LDK_EDITOR_TAG_COUNT][LDK_EDITOR_TAG_NAME_CAPACITY];
  LDKUIPoint scroll;
  char error[256];
  bool cache_loaded;
  bool draft_loaded;
  bool close_requested;
} LDKEditorTagCatalogState;

static LDKEditorTagCatalogState s_tag_catalog = {0};
static bool s_tag_catalog_window_registered = false;

static void s_tag_catalog_defaults(
    char names[LDK_EDITOR_TAG_COUNT][LDK_EDITOR_TAG_NAME_CAPACITY])
{
  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    snprintf(names[i], LDK_EDITOR_TAG_NAME_CAPACITY, "tag_%u", i);
  }
}

static void s_tag_catalog_cache_clear(void)
{
  memset(&s_tag_catalog, 0, sizeof(s_tag_catalog));
}

static bool s_tag_catalog_same_project(const LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded || !s_tag_catalog.cache_loaded)
  {
    return false;
  }

  XFSPath current = editor->project.project_file_path;
  XFSPath cached = s_tag_catalog.project_path;
  x_fs_path_normalize(&current);
  x_fs_path_normalize(&cached);
  return x_fs_path_compare(&current, &cached) == 0;
}

static bool s_tag_catalog_cache_load(LDKEditorContext *editor)
{
  XIni ini = {0};
  XIniError ini_error = {0};

  if (!editor || !editor->project.loaded)
  {
    s_tag_catalog_cache_clear();
    return false;
  }

  if (s_tag_catalog_same_project(editor))
  {
    return true;
  }

  s_tag_catalog_cache_clear();
  s_tag_catalog.project_path = editor->project.project_file_path;
  s_tag_catalog_defaults(s_tag_catalog.names);
  s_tag_catalog.cache_loaded = true;

  if (!x_ini_load_file(
          editor->project.project_file_path.buf, &ini, &ini_error))
  {
    snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
        "Could not read tag names from project file: %s",
        ini_error.message ? ini_error.message : "invalid project file");
    return true;
  }

  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    char key[16];
    snprintf(key, sizeof(key), "tag_%u", i);
    const char *name = x_ini_get(&ini, ".flags", key, NULL);
    if (name && name[0])
    {
      snprintf(s_tag_catalog.names[i], sizeof(s_tag_catalog.names[i]),
          "%s", name);
    }
  }

  x_ini_free(&ini);
  return true;
}

static void s_tag_catalog_ini_string(FILE *out, const char *text)
{
  fputc('"', out);
  for (; text && *text; ++text)
  {
    switch (*text)
    {
    case '\\':
      fputs("\\\\", out);
      break;
    case '"':
      fputs("\\\"", out);
      break;
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      break;
    case '\t':
      fputs("\\t", out);
      break;
    default:
      fputc((unsigned char)*text, out);
      break;
    }
  }
  fputc('"', out);
}

static bool s_tag_catalog_section_is_flags(const char *line, const char *end)
{
  const char *p = line;
  const char *close;

  while (p < end && (*p == ' ' || *p == '\t'))
    ++p;
  if (p >= end || *p != '[')
    return false;

  close = memchr(p, ']', (size_t)(end - p));
  if (!close)
    return false;

  ++p;
  while (p < close && isspace((unsigned char)*p))
    ++p;
  while (close > p && isspace((unsigned char)close[-1]))
    --close;

  return close - p == 6 && memcmp(p, ".flags", 6) == 0;
}

static bool s_tag_catalog_line_is_section(const char *line, const char *end)
{
  while (line < end && (*line == ' ' || *line == '\t'))
    ++line;
  return line < end && *line == '[' &&
      memchr(line, ']', (size_t)(end - line)) != NULL;
}

static bool s_tag_catalog_file_path_with_suffix(
    const XFSPath *source, const char *suffix, XFSPath *out)
{
  size_t length;
  size_t suffix_length;
  char buffer[sizeof(out->buf)];

  if (!source || !suffix || !out)
    return false;

  length = strlen(source->buf);
  suffix_length = strlen(suffix);
  if (length + suffix_length >= sizeof(buffer))
    return false;

  memcpy(buffer, source->buf, length);
  memcpy(buffer + length, suffix, suffix_length + 1);
  x_fs_path_set(out, buffer);
  return true;
}

static bool s_tag_catalog_manifest_save(LDKEditorContext *editor,
    char names[LDK_EDITOR_TAG_COUNT][LDK_EDITOR_TAG_NAME_CAPACITY])
{
  XFSPath path;
  XFSPath temporary = {0};
  XFSPath backup = {0};
  FILE *in = NULL;
  FILE *out = NULL;
  char *source = NULL;
  bool skip = false;
  bool backed_up = false;
  bool installed = false;
  bool ok = false;
  long file_size;

  if (!editor || !editor->project.loaded)
    return false;

  path = editor->project.project_file_path;
  if (!s_tag_catalog_file_path_with_suffix(&path, ".flags.tmp", &temporary) ||
      !s_tag_catalog_file_path_with_suffix(&path, ".flags.bak", &backup))
  {
    snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
        "Project path is too long to save the tag catalog.");
    return false;
  }

  if (x_fs_path_exists_cstr(temporary.buf) || x_fs_path_exists_cstr(backup.buf))
  {
    snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
        "Tag catalog save blocked by stale .flags.tmp/.flags.bak files.");
    return false;
  }

  in = fopen(path.buf, "rb");
  if (!in || fseek(in, 0, SEEK_END) != 0)
    goto done;

  file_size = ftell(in);
  if (file_size < 0 || fseek(in, 0, SEEK_SET) != 0)
    goto done;

  source = malloc((size_t)file_size + 1u);
  if (!source || fread(source, 1, (size_t)file_size, in) != (size_t)file_size)
    goto done;
  source[file_size] = 0;
  fclose(in);
  in = NULL;

  out = fopen(temporary.buf, "wbx");
  if (!out)
    goto done;

  const char *begin = source;
  if ((size_t)file_size >= 3 && memcmp(begin, "\xef\xbb\xbf", 3) == 0)
  {
    if (fwrite(begin, 1, 3, out) != 3)
      goto done;
    begin += 3;
  }

  for (const char *line = begin; *line;)
  {
    const char *end = strchr(line, '\n');
    end = end ? end + 1 : line + strlen(line);

    if (s_tag_catalog_line_is_section(line, end))
      skip = s_tag_catalog_section_is_flags(line, end);

    if (!skip && fwrite(line, 1, (size_t)(end - line), out) !=
                     (size_t)(end - line))
      goto done;

    line = end;
  }

  fputs("\n[.flags]\n", out);
  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    fprintf(out, "tag_%u = ", i);
    s_tag_catalog_ini_string(out, names[i]);
    fputc('\n', out);
  }

  if (ferror(out) || fclose(out) != 0)
  {
    out = NULL;
    goto done;
  }
  out = NULL;

  {
    XIni check = {0};
    XIniError check_error = {0};
    if (!x_ini_load_file(temporary.buf, &check, &check_error))
    {
      snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
          "Generated project file is invalid: %s",
          check_error.message ? check_error.message : "INI parse error");
      goto done;
    }
    x_ini_free(&check);
  }

  if (!x_fs_file_rename(path.buf, backup.buf))
    goto done;
  backed_up = true;

  if (!x_fs_file_rename(temporary.buf, path.buf))
    goto done;
  installed = true;

  if (!x_fs_file_delete(backup.buf))
  {
    ldki_editor_log_warning(editor,
        "Tag catalog saved, but its temporary backup could not be deleted.");
  }

  ok = true;

done:
  if (in)
    fclose(in);
  if (out)
    fclose(out);
  free(source);

  if (!ok)
  {
    if (installed)
      x_fs_file_delete(path.buf);
    if (backed_up)
      x_fs_file_rename(backup.buf, path.buf);
    x_fs_file_delete(temporary.buf);

    if (!s_tag_catalog.error[0])
    {
      snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
          "Failed to save tag catalog.");
    }
  }

  return ok;
}

static bool s_tag_catalog_validate(void)
{
  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    const char *begin = s_tag_catalog.draft[i];
    const char *end = begin + strlen(begin);
    while (*begin && isspace((unsigned char)*begin))
      ++begin;
    while (end > begin && isspace((unsigned char)end[-1]))
      --end;

    if (begin == end)
    {
      snprintf(s_tag_catalog.error, sizeof(s_tag_catalog.error),
          "Bit %u must have a tag name.", i);
      return false;
    }
  }
  return true;
}

static bool s_tag_catalog_apply(LDKEditorContext *editor)
{
  if (!s_tag_catalog_validate())
    return false;

  if (!s_tag_catalog_manifest_save(editor, s_tag_catalog.draft))
    return false;

  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    snprintf(s_tag_catalog.names[i], sizeof(s_tag_catalog.names[i]), "%s",
        s_tag_catalog.draft[i]);
  }

  s_tag_catalog.error[0] = 0;
  ldki_editor_log_info(editor, "Tag catalog saved.");
  return true;
}

static void s_tag_catalog_draft_load(LDKEditorContext *editor)
{
  if (!s_tag_catalog_cache_load(editor) || s_tag_catalog.draft_loaded)
    return;

  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    snprintf(s_tag_catalog.draft[i], sizeof(s_tag_catalog.draft[i]), "%s",
        s_tag_catalog.names[i]);
  }
  s_tag_catalog.error[0] = 0;
  s_tag_catalog.draft_loaded = true;
}

static void s_tag_catalog_window_register(LDKEditorContext *editor)
{
  if (s_tag_catalog_window_registered || !editor)
    return;

  LDKEditorWindow window = {.id = LDK_EDITOR_WINDOW_TAG_CATALOG,
      .title = "Tag Catalog",
      .function = ldki_editor_tag_catalog_show,
      .data = NULL};

  if (ldk_editor_window_add((LDKEditor *)editor, &window))
  {
    s_tag_catalog_window_registered = true;
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_TAG_CATALOG);
  }
}

void ldki_editor_tag_catalog_open(LDKEditorContext *editor)
{
  if (!editor)
    return;

  s_tag_catalog_window_register(editor);
  ldki_editor_window_show(LDK_EDITOR_WINDOW_TAG_CATALOG);
}

const char *ldki_editor_tag_name_get(LDKEditorContext *editor, u32 bit)
{
  static const char *fallback = "tag";

  if (bit >= LDK_EDITOR_TAG_COUNT || !s_tag_catalog_cache_load(editor))
    return fallback;

  return s_tag_catalog.names[bit];
}

void ldki_editor_tag_catalog_sync(LDKEditorContext *editor)
{
  if (!editor)
    return;

  s_tag_catalog_window_register(editor);

  if (!editor->project.loaded)
  {
    s_tag_catalog_cache_clear();
    return;
  }

  s_tag_catalog_cache_load(editor);

  if (s_tag_catalog.close_requested)
  {
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_TAG_CATALOG);
    s_tag_catalog.close_requested = false;
  }

  if (!ldki_editor_window_is_open(LDK_EDITOR_WINDOW_TAG_CATALOG))
  {
    s_tag_catalog.draft_loaded = false;
  }
}

void ldki_editor_tag_catalog_show(LDKEditor *instance, void *data)
{
  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)instance;
  LDKUIContext *ui = &editor->ui;

  if (!editor->project.loaded)
  {
    ldk_ui_label(ui, "Open a project to edit its tag catalog.");
    return;
  }

  s_tag_catalog_draft_load(editor);
  if (!s_tag_catalog.draft_loaded)
  {
    ldk_ui_label(ui, "Tag catalog is unavailable.");
    return;
  }

  bool editable = !editor->project_build.active &&
      editor->editor_state == LDK_EDITOR_STATE_STOPED;

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui,
      "Names below are editor-only aliases for LDKEntityInfo::flags bits 0-15.\n"
      "Runtime/game code continues to use the u16 bit mask directly.");

  ldk_ui_begin_disabled(ui, !editable);
  s_tag_catalog.scroll = ldk_ui_begin_scrollview(
      ui, s_tag_catalog.scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 i = 0; i < LDK_EDITOR_TAG_COUNT; ++i)
  {
    char bit_label[16];
    snprintf(bit_label, sizeof(bit_label), "Bit %u", i);

    ldk_ui_push_id_u32(ui, i);
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(64.0f));
    ldk_ui_label(ui, bit_label);
    ldk_ui_input_box(ui, s_tag_catalog.draft[i],
        (u32)sizeof(s_tag_catalog.draft[i]));
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
  }

  ldk_ui_end_scrollview(ui);

  if (s_tag_catalog.error[0])
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, s_tag_catalog.error);
  }

  if (!editable)
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui,
        "Tag catalog editing is available only in STOP, outside a build.");
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  bool saved = ldk_ui_button(ui, "Save") && s_tag_catalog_apply(editor);
  ldk_ui_end_disabled(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  bool canceled = ldk_ui_button(ui, "Cancel");
  ldk_ui_end_horizontal(ui);

  if (saved || canceled)
    s_tag_catalog.close_requested = true;
}

void ldki_editor_scene_catalog_close(LDKEditorContext *editor)
{
  if (editor)
  {
    free(editor->scene_catalog.scenes);
    memset(&editor->scene_catalog, 0, sizeof(editor->scene_catalog));
  }
}

static void s_catalog_draft_load(LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED ||
      editor->scene_catalog.open)
  {
    return;
  }
  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (!manager || !manager->is_initialized)
  {
    return;
  }
  LDKEditorSceneCatalog *catalog = &editor->scene_catalog;
  catalog->count = manager->scene_count;
  if (catalog->count)
  {
    catalog->scenes = malloc(sizeof(*catalog->scenes) * catalog->count);
    if (!catalog->scenes)
    {
      catalog->count = 0;
      ldki_editor_log_error(editor, "Could not allocate scene catalog draft.");
      return;
    }
    memcpy(catalog->scenes, manager->scenes,
           sizeof(*catalog->scenes) * catalog->count);
  }
  catalog->selected = UINT32_MAX;
  catalog->open = true;
}

static bool s_catalog_full_path(
  LDKEditorContext *editor, const XFSPath *relative, XFSPath *full)
{
  LDKScene entry = {0};
  entry.path = *relative;
  size_t length = strlen(relative->buf);
  if (!ldk_scene_manager_catalog_validate(&entry, 1) || length < 6 ||
      strcmp(relative->buf + length - 6, ".scene") != 0 ||
      strlen(editor->project.run_root_path.buf) + length + 2 >=
      sizeof(full->buf))
  {
    return false;
  }
  x_fs_path(full, editor->project.run_root_path.buf, relative->buf);
  x_fs_path_normalize(full);
  return x_fs_path_is_file(full);
}

static void s_catalog_add(LDKEditorContext *editor, const XFSPath *path)
{
  LDKEditorSceneCatalog *catalog = &editor->scene_catalog;
  XFSPath full = {0};
  if (!s_catalog_full_path(editor, path, &full))
  {
    snprintf(catalog->error, sizeof(catalog->error),
             "Choose an existing .scene inside this project's runtree.");
    return;
  }
  XFSPath normalized = *path;
  x_fs_path_normalize(&normalized);
  for (u32 i = 0; i < catalog->count; ++i)
  {
    XFSPath other = catalog->scenes[i].path;
    x_fs_path_normalize(&other);
    if (x_fs_path_compare(&other, &normalized) == 0)
    {
      snprintf(catalog->error, sizeof(catalog->error),
               "This scene is already in the catalog.");
      return;
    }
  }
  if (catalog->count == UINT32_MAX ||
      (size_t)catalog->count + 1 > SIZE_MAX / sizeof(*catalog->scenes))
  {
    return;
  }
  LDKScene *scenes =
    realloc(catalog->scenes, ((size_t)catalog->count + 1) * sizeof(*scenes));
  if (!scenes)
  {
    snprintf(catalog->error, sizeof(catalog->error), "Allocation failed.");
    return;
  }
  catalog->scenes = scenes;
  LDKScene *entry = &scenes[catalog->count];
  memset(entry, 0, sizeof(*entry));
  entry->path = normalized;
  const char *filename = normalized.buf;
  for (const char *p = filename; *p; ++p)
  {
    if (*p == '/' || *p == '\\')
      filename = p + 1;
  }
  x_smallstr_from_cstr(&entry->name, filename);
  if (entry->name.length >= 6)
  {
    entry->name.length -= 6;
    entry->name.buf[entry->name.length] = 0;
  }
  catalog->selected = UINT32_MAX;
  ++catalog->count;
  catalog->error[0] = 0;
}

static void s_catalog_add_full_path(
  LDKEditorContext *editor, const XFSPath *path)
{
  LDKEditorSceneCatalog *catalog = &editor->scene_catalog;
  XFSPath normalized = *path;
  XFSPath relative = {0};

  x_fs_path_normalize(&normalized);
  if (!x_fs_path_common_prefix(
        editor->project.run_root_path.buf, normalized.buf, &relative))
  {
    snprintf(catalog->error, sizeof(catalog->error),
             "Choose a scene inside this project's runtree.");
    return;
  }

  s_catalog_add(editor, &relative);
}

static void s_catalog_add_dialog(LDKEditorContext *editor)
{
  XFSPath selected = {0};
  if (!ldk_os_dialog_show_open_file(editor->window, "Add Scene", "*.scene",
                                    selected.buf, sizeof(selected.buf)))
  {
    return;
  }

  selected.length = (u32)strlen(selected.buf);
  s_catalog_add_full_path(editor, &selected);
}

static void s_catalog_remove(LDKEditorSceneCatalog *catalog, u32 index)
{
  if (!catalog || index >= catalog->count)
  {
    return;
  }

  if (index + 1 < catalog->count)
  {
    memmove(&catalog->scenes[index], &catalog->scenes[index + 1],
            (catalog->count - index - 1) * sizeof(*catalog->scenes));
  }
  --catalog->count;
  catalog->selected = UINT32_MAX;
  catalog->error[0] = 0;
}

static void s_catalog_move(LDKEditorSceneCatalog *catalog, u32 from, u32 to)
{
  if (!catalog || from >= catalog->count || to >= catalog->count || from == to)
  {
    return;
  }

  LDKScene entry = catalog->scenes[from];
  catalog->scenes[from] = catalog->scenes[to];
  catalog->scenes[to] = entry;
  catalog->selected = UINT32_MAX;
  catalog->error[0] = 0;
}

static void s_catalog_drop(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  if (!ui->mouse || !ui->current_window || ui->active_id == 0 ||
      ui->active_window_id == ui->current_window->id ||
      ui->hovered_window_id != ui->current_window->id ||
      !ldk_os_mouse_button_up(
        (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return;
  }

  u32 type = 0;
  XSmallstr payload = {0};
  if (!ldk_ui_drag_n_drop_payload_get_and_remove(&type, &payload) ||
      type != LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH)
  {
    return;
  }

  XFSPath path = {0};
  x_fs_path_set(&path, payload.buf);
  s_catalog_add_full_path(editor, &path);
}

static bool s_catalog_apply(LDKEditorContext *editor)
{
  LDKEditorSceneCatalog *catalog = &editor->scene_catalog;
  if (!ldk_scene_manager_catalog_validate(catalog->scenes, catalog->count))
  {
    snprintf(catalog->error, sizeof(catalog->error),
             "Invalid or duplicate scene paths. Use paths relative to runtree.");
    return false;
  }
  for (u32 i = 0; i < catalog->count; ++i)
  {
    XFSPath full = {0};
    if (!s_catalog_full_path(editor, &catalog->scenes[i].path, &full))
    {
      snprintf(catalog->error, sizeof(catalog->error),
               "Scene %u is missing or is not a valid runtree .scene path.", i);
      return false;
    }
  }
  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  if (!manager || !manager->is_initialized || ldk_game_instance_is_started() ||
      ldk_game_instance_is_updating())
  {
    return false;
  }
  LDKSceneResult result;
  if (!ldk_project_scene_catalog_save(
        &editor->project, catalog->scenes, catalog->count, &result))
  {
    snprintf(catalog->error, sizeof(catalog->error), "%s", result.error);
    return false;
  }
  /* Validated above; no allocation/callback can fail between save and swap. */
  if (!ldk_scene_manager_catalog_exchange(
        manager, &catalog->scenes, &catalog->count))
  {
    snprintf(catalog->error, sizeof(catalog->error),
             "Files saved, but catalog activation failed. Reopen the project.");
    return false;
  }
  ldki_editor_log_info(editor, "Scene catalog saved and applied.");
  return true;
}

void ldki_editor_scene_catalog_open(LDKEditorContext *editor)
{
  if (editor)
    ldki_editor_window_show(LDK_EDITOR_WINDOW_SCENE_CATALOG);
}

void ldki_editor_scene_catalog_sync(LDKEditorContext *editor)
{
  ldki_editor_tag_catalog_sync(editor);

  if (editor->scene_catalog.close_requested)
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_SCENE_CATALOG);
  if (!ldki_editor_window_is_open(LDK_EDITOR_WINDOW_SCENE_CATALOG))
    ldki_editor_scene_catalog_close(editor);
}

void ldki_editor_scene_catalog_show(LDKEditor *instance, void *data)
{
  enum
    {
      ACTION_NONE,
      ACTION_REMOVE,
      ACTION_MOVE_UP,
      ACTION_MOVE_DOWN,
    };

  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)instance;
  LDKUIContext *ui = &editor->ui;
  if (!editor->project.loaded)
  {
    ldk_ui_label(ui, "Open a project to edit its scene catalog.");
    return;
  }
  s_catalog_draft_load(editor);
  LDKEditorSceneCatalog *catalog = &editor->scene_catalog;
  if (!catalog->open)
  {
    ldk_ui_label(ui, "Scene catalog is unavailable. Stop playback to edit it.");
    return;
  }

  bool editable = editor->project.loaded && !editor->project_build.active &&
    editor->editor_state == LDK_EDITOR_STATE_STOPED;
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, "Scene 0 is the initial scene for Play Project.\n"
               "Reordering changes numeric indices.\n"
               "Remove does not delete files.\n"
               "Drop .scene files here to add it to the bottom of the list.");
  ldk_ui_begin_disabled(ui, !editable);

  u32 action = ACTION_NONE;
  u32 action_index = UINT32_MAX;

  catalog->scroll = ldk_ui_begin_scrollview(
    ui, catalog->scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  for (u32 i = 0; i < catalog->count; ++i)
  {
    LDKScene *entry = &catalog->scenes[i];
    char index[16];
    snprintf(index, sizeof(index), "%u", i);

    ldk_ui_push_id_u32(ui, i);
    ldk_ui_set_next_weight(ui, 0.0f);

    ldk_ui_horizontal_line(ui);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_label(ui, index);
    ldk_ui_label(ui, entry->name.buf);
    ldk_ui_label(ui, entry->path.buf);

    if (ldk_ui_button(ui, "remove"))
    {
      action = ACTION_REMOVE;
      action_index = i;
    }

    ldk_ui_set_next_disabled(ui, i == 0);
    if (ldk_ui_button(ui, "up"))
    {
      action = ACTION_MOVE_UP;
      action_index = i;
    }

    ldk_ui_set_next_disabled(ui, i + 1 >= catalog->count);
    if (ldk_ui_button(ui, "down"))
    {
      action = ACTION_MOVE_DOWN;
      action_index = i;
    }

    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
  }

  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button(ui, "+ Add scene"))
  {
    s_catalog_add_dialog(editor);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_end_scrollview(ui);

  if (action == ACTION_REMOVE)
  {
    s_catalog_remove(catalog, action_index);
  }
  else if (action == ACTION_MOVE_UP && action_index > 0)
  {
    s_catalog_move(catalog, action_index, action_index - 1);
  }
  else if (action == ACTION_MOVE_DOWN && action_index + 1 < catalog->count)
  {
    s_catalog_move(catalog, action_index, action_index + 1);
  }

  if (editable)
  {
    s_catalog_drop(editor);
  }

  if (catalog->error[0])
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, catalog->error);
  }
  if (!editable)
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui,
                 "Catalog editing is available only in STOP, outside a build.");
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  bool applied = ldk_ui_button(ui, "Save") && s_catalog_apply(editor);
  ldk_ui_end_disabled(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  bool canceled = ldk_ui_button(ui, "Cancel");

  ldk_ui_end_horizontal(ui);
  if (applied || canceled)
  {
    catalog->close_requested = true;
  }
}
