#include "ldk_editor_internal.h"
#include <inttypes.h> // for PRIu64

static u64 s_editor_entity_id(LDKEntity entity)
{
  u64 id = 0;
  size_t copy_size = sizeof(entity) < sizeof(id) ? sizeof(entity) : sizeof(id);
  memcpy(&id, &entity, copy_size);
  return id;
}

bool ldki_editor_entity_equal(LDKEntity a, LDKEntity b)
{
  return memcmp(&a, &b, sizeof(a)) == 0;
}

bool ldki_editor_selected_entity_get(
    LDKEditorContext *editor, LDKECS *ecs, LDKEntity *out_entity)
{
  if (!editor || !ecs ||
      !ldk_entity_is_alive(&ecs->entity, editor->selected_entity))
  {
    if (editor)
    {
      editor->selected_entity = x_handle_null();
    }
    return false;
  }

  if (out_entity)
  {
    *out_entity = editor->selected_entity;
  }
  return true;
}

void ldki_editor_entity_display_name(
    const LDKEntityInfo *info, LDKEntity entity, char *out, size_t out_size)
{
  const char *name = NULL;

#if defined(_DEBUG) || defined(LDK_EDITOR)
  if (info && info->name[0] != 0)
  {
    name = (const char *)info->name;
  }
#endif

  if (name)
  {
    snprintf(out, out_size, "%s", name);
  }
  else
  {
    snprintf(out, out_size, "Entity 0x%016" PRIx64, s_editor_entity_id(entity));
  }
}

static bool s_editor_hierarchy_initialize(LDKEditorContext *editor)
{
  if (editor->hierarchy_expanded_entities == NULL)
  {
    editor->hierarchy_expanded_entities = x_array_create(sizeof(LDKEntity), 32);
  }

  return editor->hierarchy_expanded_entities != NULL;
}

static i32 s_editor_hierarchy_expanded_index(
    LDKEditorContext *editor, LDKEntity entity)
{
  if (editor->hierarchy_expanded_entities == NULL)
  {
    return -1;
  }

  for (u32 i = 0; i < x_array_count(editor->hierarchy_expanded_entities); ++i)
  {
    LDKEntity *expanded = x_array_get(editor->hierarchy_expanded_entities, i);
    if (expanded != NULL && ldki_editor_entity_equal(*expanded, entity))
    {
      return (i32)i;
    }
  }

  return -1;
}

static bool s_editor_hierarchy_expanded_get(
    LDKEditorContext *editor, LDKEntity entity)
{
  return s_editor_hierarchy_expanded_index(editor, entity) >= 0;
}

static void s_editor_hierarchy_expanded_set(
    LDKEditorContext *editor, LDKEntity entity, bool expanded)
{
  i32 index = s_editor_hierarchy_expanded_index(editor, entity);

  if (expanded)
  {
    if (index < 0)
    {
      x_array_add(editor->hierarchy_expanded_entities, &entity);
    }
    return;
  }

  if (index >= 0)
  {
    x_array_delete_at(editor->hierarchy_expanded_entities, (u32)index);
  }
}

static void s_editor_hierarchy_entity_draw(LDKEditorContext *editor,
    LDKECS *ecs, LDKEntity entity, u32 depth, LDKEntity *selected_entity,
    bool *has_selection)
{
  if (ldk_entity_internal_flags_has(
          &ecs->entity, entity, LDK_ENTITY_INTERNAL_EDITOR))
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  const LDKEntityInfo *info = ldk_entity_info_get(&ecs->entity, entity);
  const LDKTransform *transform =
      ldk_entity_transform_get_const(&ecs->entity, &ecs->component, entity);
  LDKEntity first_child =
      transform != NULL ? transform->first_child : x_handle_null();
  bool has_children = !x_handle_is_null(first_child) &&
                      ldk_entity_is_alive(&ecs->entity, first_child);
  bool expanded =
      has_children && s_editor_hierarchy_expanded_get(editor, entity);
  u32 flags = has_children ? LDK_UI_TREE_NODE_NONE : LDK_UI_TREE_NODE_LEAF;
  char label[LDK_ENTITY_NAME_MAX_LEN + 32];
  u64 id = s_editor_entity_id(entity);

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color =
      editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT]; // same color as text
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];

  ldki_editor_entity_display_name(info, entity, label, sizeof(label));

  if (*has_selection && ldki_editor_entity_equal(*selected_entity, entity))
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  ldk_ui_push_id_u32(ui, (u32)id);
  ldk_ui_push_id_u32(ui, (u32)(id >> 32));

  u32 result = ldk_ui_tree_node_ex(ui, label, icon, expanded, depth, flags);

  if ((result & LDK_UI_TREE_NODE_RESULT_CLICKED) != 0)
  {
    editor->selected_entity = entity;
    *selected_entity = entity;
    *has_selection = true;
  }

  if ((result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    expanded = !expanded;
    s_editor_hierarchy_expanded_set(editor, entity, expanded);
  }

  ldk_ui_pop_id(ui);
  ldk_ui_pop_id(ui);

  if (!has_children || !expanded)
  {
    return;
  }

  LDKEntity child = first_child;
  while (!x_handle_is_null(child))
  {
    if (!ldk_entity_is_alive(&ecs->entity, child))
    {
      break;
    }

    const LDKTransform *child_transform =
        ldk_entity_transform_get_const(&ecs->entity, &ecs->component, child);
    LDKEntity next_sibling = child_transform != NULL
                                 ? child_transform->next_sibling
                                 : x_handle_null();

    s_editor_hierarchy_entity_draw(
        editor, ecs, child, depth + 1, selected_entity, has_selection);

    child = next_sibling;
  }
}

void s_editor_entity_list_window(LDKEditorContext *editor, LDKECS *ecs)
{
  LDKUIContext *ui = &editor->ui;
  static LDKUIRect window_rect = {10, 60, 100, 100};
  static LDKUIPoint scroll = {0};
  bool owns_window = ui->current_window == NULL;
  LDKEntity selected_entity = x_handle_null();
  bool has_selection = false;

  LDKUIIcon icon = {0};
  icon.size =
      ldk_sizef(LDK_UI_DEFAULT_CONTROL_HEIGHT, LDK_UI_DEFAULT_CONTROL_HEIGHT);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  icon.color =
      editor->ui.theme.colors[LDK_UI_COLOR_CONTROL_TEXT]; // same color as text
  icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_OBJECT];

  if (editor == NULL || ecs == NULL)
  {
    return;
  }

  has_selection =
      ldki_editor_selected_entity_get(editor, ecs, &selected_entity);

  if (owns_window)
  {
    window_rect =
        ldk_ui_begin_window(ui, "HIERARCHY", window_rect, LDK_UI_WINDOW_TOOL);
  }

  if (!s_editor_hierarchy_initialize(editor))
  {
    ldk_ui_label(ui, "Hierarchy allocation failed.");
    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  scroll = ldk_ui_begin_scrollview(
      ui, scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  static bool systems_expanded = true;
  static bool entities_expanded = true;

  ldk_ui_push_id_cstr(ui, "systems");
  u32 systems_result = ldk_ui_tree_node_ex(
      ui, "Systems", icon, systems_expanded, 0, LDK_UI_TREE_NODE_NONE);
  if ((systems_result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    systems_expanded = !systems_expanded;
  }
  ldk_ui_pop_id(ui);

  if (systems_expanded)
  {
    u32 system_count = ldk_system_registry_count(&ecs->system);
    for (u32 i = 0; i < system_count; ++i)
    {
      LDKSystemDesc desc = {0};
      if (!ldk_system_registry_at(&ecs->system, i, &desc))
      {
        continue;
      }

      ldk_ui_push_id_u32(ui, i);
      ldk_ui_tree_node_ex(ui,
          desc.name != NULL ? desc.name : "<unnamed system>", icon, false, 1,
          LDK_UI_TREE_NODE_LEAF);
      ldk_ui_pop_id(ui);
    }
  }

  ldk_ui_push_id_cstr(ui, "entity");
  u32 entities_result = ldk_ui_tree_node_ex(
      ui, "Entity", icon, entities_expanded, 0, LDK_UI_TREE_NODE_NONE);
  if ((entities_result & LDK_UI_TREE_NODE_RESULT_TOGGLED) != 0)
  {
    entities_expanded = !entities_expanded;
  }
  ldk_ui_pop_id(ui);

  if (entities_expanded)
  {
    LDKEntityIterator it = ldk_entity_iterator_begin(&ecs->entity);
    LDKEntity entity;

    while (ldk_entity_iterator_next(&it, &entity))
    {
      if (ldk_entity_internal_flags_has(
              &ecs->entity, entity, LDK_ENTITY_INTERNAL_EDITOR))
      {
        continue;
      }

      const LDKTransform *transform =
          ldk_entity_transform_get_const(&ecs->entity, &ecs->component, entity);

      if (transform != NULL && !x_handle_is_null(transform->parent) &&
          ldk_entity_is_alive(&ecs->entity, transform->parent))
      {
        continue;
      }

      s_editor_hierarchy_entity_draw(
          editor, ecs, entity, 1, &selected_entity, &has_selection);
    }

    ldk_entity_iterator_end(&it);
  }
  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

void ldk_editor_hierarchy_show(LDKEditor *editor, LDKECS *ecs)
{
  s_editor_entity_list_window((LDKEditorContext *)editor, ecs);
}
