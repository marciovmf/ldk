#include "ldk_editor_internal.h"
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
  catalog->selected = catalog->count++;
  catalog->error[0] = 0;
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
  ldk_ui_label(ui, "Scene 0 is the initial scene for Play Project.");
  ldk_ui_label(
      ui, "Reordering changes numeric indices. Remove does not delete files.");
  ldk_ui_begin_disabled(ui, !editable);
  ldk_ui_begin_horizontal(ui);
  if (ldk_ui_button_flat(ui, "Add Scene..."))
  {
    XFSPath selected = {0}, relative = {0};
    if (ldk_os_dialog_show_open_file(editor->window, "Add Scene", "*.scene",
            selected.buf, sizeof(selected.buf)))
    {
      selected.length = (u32)strlen(selected.buf);
      x_fs_path_normalize(&selected);
      if (x_fs_path_common_prefix(
              editor->project.run_root_path.buf, selected.buf, &relative))
      {
        s_catalog_add(editor, &relative);
      }
      else
      {
        snprintf(catalog->error, sizeof(catalog->error),
            "Choose a scene inside this project's runtree.");
      }
    }
  }
  ldk_ui_set_next_disabled(ui, editor->current_scene_path.length == 0);
  if (ldk_ui_button_flat(ui, "Add Current Scene"))
  {
    s_catalog_add(editor, &editor->current_scene_path);
  }
  ldk_ui_end_horizontal(ui);

  ldk_ui_begin_horizontal(ui);
  bool selected = catalog->selected < catalog->count;
  ldk_ui_set_next_disabled(ui, !selected);
  if (ldk_ui_button_flat(ui, "Remove"))
  {
    memmove(&catalog->scenes[catalog->selected],
        &catalog->scenes[catalog->selected + 1],
        (catalog->count - catalog->selected - 1) * sizeof(*catalog->scenes));
    --catalog->count;
    catalog->selected = UINT32_MAX;
    selected = false;
  }
  ldk_ui_set_next_disabled(ui, !selected || catalog->selected == 0);
  if (ldk_ui_button_flat(ui, "Move Up"))
  {
    LDKScene entry = catalog->scenes[catalog->selected];
    catalog->scenes[catalog->selected] = catalog->scenes[catalog->selected - 1];
    catalog->scenes[--catalog->selected] = entry;
  }
  ldk_ui_set_next_disabled(
      ui, !selected || catalog->selected + 1 >= catalog->count);
  if (ldk_ui_button_flat(ui, "Move Down"))
  {
    LDKScene entry = catalog->scenes[catalog->selected];
    catalog->scenes[catalog->selected] = catalog->scenes[catalog->selected + 1];
    catalog->scenes[++catalog->selected] = entry;
  }
  ldk_ui_set_next_disabled(ui, !selected);
  if (ldk_ui_button_flat(ui, "Open"))
  {
    XFSPath full = {0};
    if (!s_catalog_full_path(
            editor, &catalog->scenes[catalog->selected].path, &full))
    {
      snprintf(catalog->error, sizeof(catalog->error),
          "Scene file is missing or invalid.");
    }
    else if (editor->current_scene_path.length == 0 ||
             ldk_os_dialog_show_ok_cancel(editor->window, "Open Scene",
                 "Opening replaces the current scene. Unsaved scene edits will "
                 "be lost."))
    {
      ldki_editor_scene_load(editor, &full);
    }
  }
  ldk_ui_end_horizontal(ui);
  ldk_ui_label(ui,
      "Index / selection        Friendly name        Runtree-relative path");
  ldk_ui_set_next_weight(ui, 1);
  catalog->scroll = ldk_ui_begin_scrollview(
      ui, catalog->scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  for (u32 i = 0; i < catalog->count; ++i)
  {
    LDKScene *entry = &catalog->scenes[i];
    char index[48];
    snprintf(index, sizeof(index), "%s%u%s", catalog->selected == i ? "> " : "",
        i, i == 0 ? " (Initial)" : "");
    ldk_ui_push_id_u32(ui, i);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(110));
    if (ldk_ui_button_flat(ui, index))
      catalog->selected = i;
    ldk_ui_set_next_width(ui, ldk_ui_px(190));
    ldk_ui_input_box(ui, entry->name.buf, sizeof(entry->name.buf));
    entry->name.length = (u32)strlen(entry->name.buf);
    ldk_ui_set_next_weight(ui, 1);
    ldk_ui_input_box(ui, entry->path.buf, sizeof(entry->path.buf));
    entry->path.length = (u32)strlen(entry->path.buf);
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
  }
  ldk_ui_end_scrollview(ui);
  ldk_ui_begin_horizontal(ui);
  bool applied = ldk_ui_button_flat(ui, "Apply") && s_catalog_apply(editor);
  ldk_ui_end_disabled(ui);
  bool canceled = ldk_ui_button_flat(ui, "Cancel");
  ldk_ui_end_horizontal(ui);
  if (catalog->error[0])
    ldk_ui_label(ui, catalog->error);
  if (!editable)
    ldk_ui_label(
        ui, "Catalog editing is available only in STOP, outside a build.");
  if (applied || canceled)
  {
    catalog->close_requested = true;
  }
  ldk_ui_spacer(ui);
}
