/**
 * @file ldk_editor_grouping_catalog.c
 * @brief Editor UI for project-defined ECS groupings.
 */

#include "ldk_editor_internal.h"
#include "module/ldk_ui.h"
#include <ldk_scene.h>
#include <module/ldk_ecs.h>
#include <stdx/stdx_ini.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_EDITOR_GROUPING_NAME_CAPACITY 64
#define LDK_EDITOR_GROUPING_ID_BASE UINT64_C(0x8000000000000000)

typedef struct LDKEditorGroupingDraft
{
  u64 id;
  char name[LDK_EDITOR_GROUPING_NAME_CAPACITY];
  u32 component_types[LDK_ENTITY_MAX_COMPONENTS];
  u32 component_count;
} LDKEditorGroupingDraft;

typedef struct LDKEditorGroupingCatalogState
{
  XFSPath project_path;
  LDKEditorGroupingDraft *groups;
  u32 count;
  u32 capacity;
  u64 next_id;
  LDKUIPoint scroll;
  char error[256];
  bool loaded;
} LDKEditorGroupingCatalogState;

static LDKEditorGroupingCatalogState s_grouping_catalog = {0};
static bool s_grouping_catalog_window_registered = false;

static void s_grouping_catalog_clear(void)
{
  free(s_grouping_catalog.groups);
  memset(&s_grouping_catalog, 0, sizeof(s_grouping_catalog));
}

static bool s_grouping_catalog_reserve(u32 min_capacity)
{
  LDKEditorGroupingDraft *groups;
  u32 capacity;

  if (s_grouping_catalog.capacity >= min_capacity)
  {
    return true;
  }

  capacity = s_grouping_catalog.capacity ? s_grouping_catalog.capacity : 8u;
  while (capacity < min_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = min_capacity;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*groups))
  {
    return false;
  }

  groups = (LDKEditorGroupingDraft *)realloc(s_grouping_catalog.groups,
      sizeof(*groups) * (size_t)capacity);
  if (!groups)
  {
    return false;
  }

  s_grouping_catalog.groups = groups;
  s_grouping_catalog.capacity = capacity;
  return true;
}

static bool s_grouping_catalog_same_project(const LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded || !s_grouping_catalog.loaded)
  {
    return false;
  }

  XFSPath current = editor->project.project_file_path;
  XFSPath cached = s_grouping_catalog.project_path;
  x_fs_path_normalize(&current);
  x_fs_path_normalize(&cached);
  return x_fs_path_compare(&current, &cached) == 0;
}

static bool s_grouping_id_parse(const char *text, u64 *out_id)
{
  unsigned long long value;
  char *end;

  if (!text || !text[0] || !out_id || text[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoull(text, &end, 0);
  if (errno == ERANGE || end == text || *end != 0 || value == 0)
  {
    return false;
  }

  *out_id = (u64)value;
  return true;
}

static bool s_grouping_catalog_load(LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded)
  {
    s_grouping_catalog_clear();
    return false;
  }

  if (s_grouping_catalog_same_project(editor))
  {
    return true;
  }

  s_grouping_catalog_clear();
  s_grouping_catalog.project_path = editor->project.project_file_path;
  s_grouping_catalog.next_id = LDK_EDITOR_GROUPING_ID_BASE;

  {
    XIni ini = {0};
    XIniError error = {0};
    if (x_ini_load_file(editor->project.project_file_path.buf, &ini, &error))
    {
      u64 persisted_next_id;
      const char *next_id_text =
          x_ini_get(&ini, "groupings", "next_id", NULL);
      if (s_grouping_id_parse(next_id_text, &persisted_next_id) &&
          persisted_next_id >= LDK_EDITOR_GROUPING_ID_BASE)
      {
        s_grouping_catalog.next_id = persisted_next_id;
      }
      x_ini_free(&ini);
    }
  }

  u32 grouping_count = ldk_ecs_grouping_count();
  for (u32 i = 0; i < grouping_count; ++i)
  {
    LDKGroupingDesc desc = {0};
    if (!ldk_ecs_grouping_at(i, &desc) ||
        !ldk_ecs_grouping_is_config_defined(desc.id))
    {
      continue;
    }

    if (!s_grouping_catalog_reserve(s_grouping_catalog.count + 1u))
    {
      snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
          "Failed to allocate grouping catalog.");
      return false;
    }

    LDKEditorGroupingDraft *draft =
        &s_grouping_catalog.groups[s_grouping_catalog.count++];
    memset(draft, 0, sizeof(*draft));
    draft->id = desc.id;
    snprintf(draft->name, sizeof(draft->name), "%s", desc.name);
    draft->component_count = desc.component_count;
    if (desc.component_count)
    {
      memcpy(draft->component_types, desc.component_types,
          sizeof(u32) * (size_t)desc.component_count);
    }

    if (desc.id >= LDK_EDITOR_GROUPING_ID_BASE &&
        desc.id >= s_grouping_catalog.next_id)
    {
      s_grouping_catalog.next_id =
          desc.id == UINT64_MAX ? 0 : desc.id + 1u;
    }
  }

  s_grouping_catalog.loaded = true;
  return true;
}

static bool s_grouping_draft_has_id(u64 id)
{
  for (u32 i = 0; i < s_grouping_catalog.count; ++i)
  {
    if (s_grouping_catalog.groups[i].id == id)
    {
      return true;
    }
  }
  return false;
}

static u64 s_grouping_new_id(void)
{
  LDKGroupingDesc desc;
  u64 id = s_grouping_catalog.next_id;

  while (id != 0)
  {
    if (!s_grouping_draft_has_id(id) &&
        !ldk_ecs_grouping_find_by_id(id, &desc))
    {
      s_grouping_catalog.next_id = id == UINT64_MAX ? 0 : id + 1u;
      return id;
    }
    ++id;
  }

  s_grouping_catalog.next_id = 0;
  return 0;
}

static void s_grouping_unique_name(char *out, size_t out_size)
{
  u32 suffix = 1;

  for (;;)
  {
    bool used = false;
    if (suffix == 1)
    {
      snprintf(out, out_size, "Grouping");
    }
    else
    {
      snprintf(out, out_size, "Grouping %u", suffix);
    }

    for (u32 i = 0; i < s_grouping_catalog.count; ++i)
    {
      if (strcmp(s_grouping_catalog.groups[i].name, out) == 0)
      {
        used = true;
        break;
      }
    }

    if (!used)
    {
      u32 count = ldk_ecs_grouping_count();
      for (u32 i = 0; i < count; ++i)
      {
        LDKGroupingDesc desc = {0};
        if (ldk_ecs_grouping_at(i, &desc) &&
            !ldk_ecs_grouping_is_config_defined(desc.id) &&
            strcmp(desc.name, out) == 0)
        {
          used = true;
          break;
        }
      }
    }

    if (!used)
    {
      return;
    }
    ++suffix;
  }
}

static bool s_grouping_add(void)
{
  u64 id = s_grouping_new_id();
  if (id == 0 || !s_grouping_catalog_reserve(s_grouping_catalog.count + 1u))
  {
    return false;
  }

  LDKEditorGroupingDraft *draft =
      &s_grouping_catalog.groups[s_grouping_catalog.count];
  memset(draft, 0, sizeof(*draft));
  draft->id = id;
  s_grouping_unique_name(draft->name, sizeof(draft->name));
  s_grouping_catalog.count += 1u;
  return true;
}

static bool s_grouping_has_component(
    const LDKEditorGroupingDraft *draft, u32 component_type)
{
  if (!draft)
  {
    return false;
  }

  for (u32 i = 0; i < draft->component_count; ++i)
  {
    if (draft->component_types[i] == component_type)
    {
      return true;
    }
  }
  return false;
}

static bool s_grouping_catalog_validate(void)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  u32 registered_count = ldk_ecs_grouping_count();

  s_grouping_catalog.error[0] = 0;
  if (!ecs)
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "ECS is unavailable.");
    return false;
  }

  for (u32 i = 0; i < s_grouping_catalog.count; ++i)
  {
    LDKEditorGroupingDraft *draft = &s_grouping_catalog.groups[i];
    if (draft->id == 0 || draft->name[0] == 0)
    {
      snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
          "Every grouping needs a non-empty name and id.");
      return false;
    }

    for (u32 j = 0; j < i; ++j)
    {
      if (draft->id == s_grouping_catalog.groups[j].id ||
          strcmp(draft->name, s_grouping_catalog.groups[j].name) == 0)
      {
        snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
            "Grouping names and ids must be unique.");
        return false;
      }
    }

    for (u32 component_index = 0;
         component_index < draft->component_count; ++component_index)
    {
      if (!ldk_component_is_registered(
              &ecs->component, draft->component_types[component_index]))
      {
        snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
            "Grouping '%s' references an unregistered component.",
            draft->name);
        return false;
      }
    }

    for (u32 registered_index = 0; registered_index < registered_count;
         ++registered_index)
    {
      LDKGroupingDesc desc = {0};
      if (!ldk_ecs_grouping_at(registered_index, &desc) ||
          ldk_ecs_grouping_is_config_defined(desc.id))
      {
        continue;
      }

      if (desc.id == draft->id || strcmp(desc.name, draft->name) == 0)
      {
        snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
            "Grouping '%s' conflicts with a grouping registered by code.",
            draft->name);
        return false;
      }
    }
  }

  return true;
}

static void s_ini_string(FILE *out, const char *text)
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

static bool s_line_is_section(const char *line, const char *end)
{
  while (line < end && (*line == ' ' || *line == '\t'))
  {
    ++line;
  }
  return line < end && *line == '[' &&
         memchr(line, ']', (size_t)(end - line)) != NULL;
}

static bool s_section_is_groupings(const char *line, const char *end)
{
  const char *p = line;
  const char *close;

  while (p < end && (*p == ' ' || *p == '\t'))
  {
    ++p;
  }
  if (p >= end || *p != '[')
  {
    return false;
  }

  close = memchr(p, ']', (size_t)(end - p));
  if (!close)
  {
    return false;
  }

  ++p;
  while (p < close && (*p == ' ' || *p == '\t'))
  {
    ++p;
  }
  while (close > p && (close[-1] == ' ' || close[-1] == '\t'))
  {
    --close;
  }

  return close - p == 9 && memcmp(p, "groupings", 9) == 0;
}

static bool s_path_suffix(
    const XFSPath *source, const char *suffix, XFSPath *out)
{
  char buffer[sizeof(out->buf)];
  size_t length;
  size_t suffix_length;

  if (!source || !suffix || !out)
  {
    return false;
  }

  length = strlen(source->buf);
  suffix_length = strlen(suffix);
  if (length + suffix_length >= sizeof(buffer))
  {
    return false;
  }

  memcpy(buffer, source->buf, length);
  memcpy(buffer + length, suffix, suffix_length + 1u);
  x_fs_path_set(out, buffer);
  return true;
}

static bool s_grouping_catalog_file_save(LDKEditorContext *editor)
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
  {
    return false;
  }

  path = editor->project.project_file_path;
  if (!s_path_suffix(&path, ".groupings.tmp", &temporary) ||
      !s_path_suffix(&path, ".groupings.bak", &backup) ||
      x_fs_path_exists_cstr(temporary.buf) || x_fs_path_exists_cstr(backup.buf))
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "Could not create grouping catalog temporary files.");
    return false;
  }

  in = fopen(path.buf, "rb");
  if (!in || fseek(in, 0, SEEK_END) != 0)
  {
    goto done;
  }

  file_size = ftell(in);
  if (file_size < 0 || fseek(in, 0, SEEK_SET) != 0)
  {
    goto done;
  }

  source = (char *)malloc((size_t)file_size + 1u);
  if (!source || fread(source, 1, (size_t)file_size, in) != (size_t)file_size)
  {
    goto done;
  }
  source[file_size] = 0;
  fclose(in);
  in = NULL;

  out = fopen(temporary.buf, "wbx");
  if (!out)
  {
    goto done;
  }

  const char *begin = source;
  if ((size_t)file_size >= 3 && memcmp(begin, "\xef\xbb\xbf", 3) == 0)
  {
    if (fwrite(begin, 1, 3, out) != 3)
    {
      goto done;
    }
    begin += 3;
  }

  for (const char *line = begin; *line;)
  {
    const char *end = strchr(line, '\n');
    end = end ? end + 1 : line + strlen(line);

    if (s_line_is_section(line, end))
    {
      skip = s_section_is_groupings(line, end);
    }

    if (!skip &&
        fwrite(line, 1, (size_t)(end - line), out) != (size_t)(end - line))
    {
      goto done;
    }
    line = end;
  }

  fputs("\n[groupings]\n", out);
  fprintf(out, "count = %u\n", s_grouping_catalog.count);
  fprintf(out, "next_id = \"0x%016" PRIx64 "\"\n",
      s_grouping_catalog.next_id);
  for (u32 i = 0; i < s_grouping_catalog.count; ++i)
  {
    const LDKEditorGroupingDraft *draft = &s_grouping_catalog.groups[i];
    fprintf(out, "%u.id = \"0x%016" PRIx64 "\"\n", i, draft->id);
    fprintf(out, "%u.name = ", i);
    s_ini_string(out, draft->name);
    fputc('\n', out);
    fprintf(out, "%u.component_count = %u\n", i, draft->component_count);
    for (u32 component_index = 0;
         component_index < draft->component_count; ++component_index)
    {
      fprintf(out, "%u.component_%u = \"0x%08" PRIx32 "\"\n", i,
          component_index, draft->component_types[component_index]);
    }
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
      snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
          "Generated project file is invalid: %s",
          check_error.message ? check_error.message : "INI parse error");
      goto done;
    }
    x_ini_free(&check);
  }

  if (!x_fs_file_rename(path.buf, backup.buf))
  {
    goto done;
  }
  backed_up = true;

  if (!x_fs_file_rename(temporary.buf, path.buf))
  {
    goto done;
  }
  installed = true;

  if (!ldk_project_write_runtime_ini(&editor->project))
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "The grouping catalog was saved, but game.ini could not be updated.");
    goto done;
  }

  if (!ldk_ecs_grouping_configure_file(path.buf))
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "The grouping catalog was written but could not be loaded by the ECS.");
    goto done;
  }

  if (!x_fs_file_delete(backup.buf))
  {
    ldki_editor_log_warning(editor,
        "Grouping catalog saved, but its backup could not be deleted.");
  }

  backed_up = false;
  ok = true;

done:
  if (in)
  {
    fclose(in);
  }
  if (out)
  {
    fclose(out);
  }
  free(source);

  if (!ok)
  {
    if (installed)
    {
      x_fs_file_delete(path.buf);
    }
    if (backed_up)
    {
      x_fs_file_rename(backup.buf, path.buf);
      ldk_project_write_runtime_ini(&editor->project);
    }
    x_fs_file_delete(temporary.buf);
  }

  if (!ok && !s_grouping_catalog.error[0])
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "Failed to save grouping catalog.");
  }
  return ok;
}

static bool s_grouping_catalog_apply(LDKEditorContext *editor)
{
  if (!s_grouping_catalog_validate() || !s_grouping_catalog_file_save(editor))
  {
    return false;
  }

  s_grouping_catalog.loaded = false;
  s_grouping_catalog.error[0] = 0;
  s_grouping_catalog_load(editor);
  ldki_editor_log_info(editor, "Grouping catalog saved.");
  return true;
}

static void s_grouping_catalog_window_register(LDKEditorContext *editor)
{
  if (s_grouping_catalog_window_registered || !editor)
  {
    return;
  }

  LDKEditorWindow window = {.id = LDK_EDITOR_WINDOW_GROUPING_CATALOG,
      .title = "Grouping Catalog",
      .function = ldki_editor_grouping_catalog_show,
      .data = NULL};

  if (ldk_editor_window_add((LDKEditor *)editor, &window))
  {
    s_grouping_catalog_window_registered = true;
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_GROUPING_CATALOG);
  }
}

void ldki_editor_grouping_catalog_open(LDKEditorContext *editor)
{
  if (!editor)
  {
    return;
  }

  s_grouping_catalog_window_register(editor);
  s_grouping_catalog_load(editor);
  ldki_editor_window_show(LDK_EDITOR_WINDOW_GROUPING_CATALOG);
}

static const char *s_component_name(LDKGame *game, u32 component_type,
    char *fallback, size_t fallback_size)
{
  const LDKComponentMeta *meta =
      ldk_scene_component_meta_find_by_type(game, component_type);
  if (meta && meta->name)
  {
    return meta->name;
  }

  snprintf(fallback, fallback_size, "0x%08" PRIx32, component_type);
  return fallback;
}

void ldki_editor_grouping_catalog_show(LDKEditor *instance, void *data)
{
  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)instance;
  LDKUIContext *ui = &editor->ui;
  LDKGame *game = ldk_game_get();
  bool editable = editor->project.loaded && !editor->project_build.active &&
                  editor->editor_state == LDK_EDITOR_STATE_STOPED;

  if (!editor->project.loaded)
  {
    ldk_ui_label(ui, "Open a project to edit groupings.");
    return;
  }

  if (!s_grouping_catalog_load(editor))
  {
    ldk_ui_label(ui, s_grouping_catalog.error[0]
                         ? s_grouping_catalog.error
                         : "Grouping catalog is unavailable.");
    return;
  }

  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  u32 metadata_count =
      game && game->metadata_count ? game->metadata_count() : 0;
  if (metadata_count > UINT32_MAX - 4u)
  {
    ldk_ui_label(ui, "Component metadata count is invalid.");
    return;
  }
  u32 component_capacity = metadata_count + 4u;
  u32 component_count = 1u;
  const char **component_labels =
      (const char **)calloc(component_capacity, sizeof(*component_labels));
  u32 *component_types =
      (u32 *)calloc(component_capacity, sizeof(*component_types));

  if (!component_labels || !component_types)
  {
    free(component_labels);
    free(component_types);
    ldk_ui_label(ui, "Failed to allocate component list.");
    return;
  }

  component_labels[0] = "+ Add Component";

  if (ecs)
  {
    static const u32 builtin_types[] = {LDK_COMPONENT_TYPE_TRANSFORM,
        LDK_COMPONENT_TYPE_CAMERA, LDK_COMPONENT_TYPE_MESH_SOURCE};

    for (u32 i = 0; i < sizeof(builtin_types) / sizeof(builtin_types[0]); ++i)
    {
      u32 type = builtin_types[i];
      const char *name = ldk_component_name_get(&ecs->component, type);
      if (!ldk_component_is_registered(&ecs->component, type) || !name)
      {
        continue;
      }
      component_types[component_count] = type;
      component_labels[component_count] = name;
      component_count += 1u;
    }
  }

  for (u32 i = 0; i < metadata_count; ++i)
  {
    const LDKComponentMeta *meta = game->metadata_get(i);
    u32 type = ldk_scene_component_meta_runtime_type(meta);
    bool duplicate = false;

    if (!meta || !meta->name || type == 0)
    {
      continue;
    }

    for (u32 j = 1; j < component_count; ++j)
    {
      if (component_types[j] == type)
      {
        duplicate = true;
        break;
      }
    }

    if (!duplicate)
    {
      component_types[component_count] = type;
      component_labels[component_count] = meta->name;
      component_count += 1u;
    }
  }

  ldk_ui_begin_disabled(ui, !editable);
  ldk_ui_set_next_weight(ui, 0);
  if (ldk_ui_button(ui, "+ Add Grouping") && !s_grouping_add())
  {
    snprintf(s_grouping_catalog.error, sizeof(s_grouping_catalog.error),
        "Failed to add grouping.");
  }
  ldk_ui_end_disabled(ui);

  s_grouping_catalog.scroll = ldk_ui_begin_scrollview(
      ui, s_grouping_catalog.scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  for (u32 i = 0; i < s_grouping_catalog.count; ++i)
  {
    LDKEditorGroupingDraft *draft = &s_grouping_catalog.groups[i];
    bool delete_group = false;

    ldk_ui_push_id_u32(ui, i);
    ldk_ui_begin_disabled(ui, !editable);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_weight(ui, 1.0f);
    ldk_ui_input_box(ui, draft->name, (u32)sizeof(draft->name));
    ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
    if (ldk_ui_button(ui, "Delete"))
    {
      delete_group = true;
    }
    ldk_ui_end_horizontal(ui);

    char id_text[48];
    snprintf(id_text, sizeof(id_text), "id: 0x%016" PRIx64, draft->id);
    ldk_ui_label(ui, id_text);

    for (u32 component_index = 0;
         component_index < draft->component_count; ++component_index)
    {
      char fallback[32];
      const char *name = s_component_name(game,
          draft->component_types[component_index], fallback, sizeof(fallback));
      ldk_ui_push_id_u32(ui, component_index);
      ldk_ui_begin_horizontal(ui);
      ldk_ui_set_next_weight(ui, 1.0f);
      ldk_ui_label(ui, name);
      ldk_ui_set_next_width(ui, ldk_ui_px(72.0f));
      if (ldk_ui_button(ui, "Remove"))
      {
        memmove(&draft->component_types[component_index],
            &draft->component_types[component_index + 1u],
            sizeof(u32) *
                (size_t)(draft->component_count - component_index - 1u));
        draft->component_count -= 1u;
        ldk_ui_end_horizontal(ui);
        ldk_ui_pop_id(ui);
        break;
      }
      ldk_ui_end_horizontal(ui);
      ldk_ui_pop_id(ui);
    }

    if (component_labels &&
        draft->component_count < LDK_ENTITY_MAX_COMPONENTS)
    {
      u32 selected =
          ldk_ui_combo_box(ui, component_labels, component_count, 0);
      if (selected > 0 && selected < component_count &&
          component_types[selected] != 0 &&
          !s_grouping_has_component(draft, component_types[selected]))
      {
        draft->component_types[draft->component_count++] =
            component_types[selected];
      }
    }

    ldk_ui_end_disabled(ui);
    ldk_ui_horizontal_line(ui);
    ldk_ui_pop_id(ui);

    if (delete_group)
    {
      memmove(&s_grouping_catalog.groups[i], &s_grouping_catalog.groups[i + 1u],
          sizeof(*s_grouping_catalog.groups) *
              (size_t)(s_grouping_catalog.count - i - 1u));
      s_grouping_catalog.count -= 1u;
      --i;
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);

  if (s_grouping_catalog.error[0])
  {
    ldk_ui_label(ui, s_grouping_catalog.error);
  }

  ldk_ui_horizontal_line(ui);
  ldk_ui_begin_disabled(ui, !editable);
  ldk_ui_set_next_weight(ui, 0);
  if (ldk_ui_button(ui, "Save Groupings"))
  {
    s_grouping_catalog_apply(editor);
  }
  ldk_ui_end_disabled(ui);

  free(component_types);
  free(component_labels);
}
