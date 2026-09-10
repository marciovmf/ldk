#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include <module/ldk_scene_manager.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
