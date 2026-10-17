#include "ldk_editor_callbacks_internal.h"

#include <editor/ldk_editor_callback.h>
#include <module/ldk_ecs.h>

/* Never cache pointers returned by a game DLL: hot reload replaces it. */
static bool s_editor_callbacks_api(LDKGame *game,
    LDKGameEditorCallbackCountFn *count, LDKGameEditorCallbackGetFn *get)
{
  if (!game || !game->lib)
  {
    return false;
  }
  *count = (LDKGameEditorCallbackCountFn)ldk_os_library_fuction_ptr_get(
      game->lib, LDK_GAME_EDITOR_CALLBACK_COUNT_NAME);
  *get = (LDKGameEditorCallbackGetFn)ldk_os_library_fuction_ptr_get(
      game->lib, LDK_GAME_EDITOR_CALLBACK_GET_NAME);
  return *count != NULL && *get != NULL;
}

static LDKEditorComponentCallbackFn s_editor_component_callback_resolve(
    LDKGame *game, const LDKEditorCallbackMeta *meta)
{
  return (LDKEditorComponentCallbackFn)ldk_os_library_fuction_ptr_get(
      game->lib, meta->symbol);
}

static LDKEditorSystemCallbackFn s_editor_system_callback_resolve(
    LDKGame *game, const LDKEditorCallbackMeta *meta)
{
  return (LDKEditorSystemCallbackFn)ldk_os_library_fuction_ptr_get(
      game->lib, meta->symbol);
}

/* Scene-owned context is prepared on scene load, even without Play.
 * The ECS registry's data binding belongs to Play and can be unbound in Edit.
 */
static bool s_editor_system_callback_available(
    const LDKSceneSystems *systems, u64 system_id)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  LDKSystemDesc desc = {0};
  return systems && ldk_scene_systems_contains(systems, system_id) &&
      registry && ldk_system_registry_find_by_id(registry, system_id, &desc);
}

typedef struct LDKEditorCallbackIteration
{
  u32 component_type;
  float dt;
  LDKEditorComponentCallbackFn callback;
} LDKEditorCallbackIteration;

static bool s_editor_callback_entity_update(LDKEntity entity, void *user)
{
  const LDKEditorCallbackIteration *iteration = user;
  if (ldk_ecs_component_get_const(entity, iteration->component_type))
  {
    iteration->callback(entity, iteration->dt);
  }
  return true;
}

void ldki_editor_callbacks_update(LDKGame *game,
    const LDKSceneSystems *systems, float dt)
{
  LDKGameEditorCallbackCountFn count = NULL;
  LDKGameEditorCallbackGetFn get = NULL;
  if (!s_editor_callbacks_api(game, &count, &get))
  {
    return;
  }
  const u32 total = count();
  for (u32 i = 0u; i < total; ++i)
  {
    const LDKEditorCallbackMeta *meta = get(i);
    if (!meta || !(meta->flags & LDK_EDITOR_CALLBACK_ON_UPDATE))
    {
      continue;
    }
    if (meta->owner == LDK_EDITOR_CALLBACK_SYSTEM)
    {
      if (!s_editor_system_callback_available(systems, meta->system_id))
      {
        continue;
      }
      LDKEditorSystemCallbackFn callback =
          s_editor_system_callback_resolve(game, meta);
      if (callback)
      {
        callback(meta->system_id,
            ldk_scene_systems_data_get(systems, meta->system_id), dt);
      }
    }
    else if (meta->owner == LDK_EDITOR_CALLBACK_COMPONENT)
    {
      LDKEditorComponentCallbackFn callback =
          s_editor_component_callback_resolve(game, meta);
      if (callback)
      {
        LDKEditorCallbackIteration iteration = {
            meta->component_type, dt, callback};
        (void)ldk_ecs_entity_foreach(s_editor_callback_entity_update, &iteration);
      }
    }
  }
}

void ldki_editor_callbacks_draw(LDKGame *game, LDKUIContext *ui,
    const LDKSceneSystems *systems, LDKEntity entity, u64 system_id)
{
  LDKGameEditorCallbackCountFn count = NULL;
  LDKGameEditorCallbackGetFn get = NULL;
  if (!ui || !s_editor_callbacks_api(game, &count, &get))
  {
    return;
  }
  const u32 total = count();
  bool has_section = false;
  for (u32 i = 0u; i < total; ++i)
  {
    const LDKEditorCallbackMeta *meta = get(i);
    if (!meta || !(meta->flags & LDK_EDITOR_CALLBACK_MANUAL))
    {
      continue;
    }
    if (meta->owner == LDK_EDITOR_CALLBACK_COMPONENT)
    {
      if (system_id || x_handle_is_null(entity) ||
          !ldk_ecs_component_get_const(entity, meta->component_type))
      {
        continue;
      }
    }
    else if (meta->owner == LDK_EDITOR_CALLBACK_SYSTEM)
    {
      if (!system_id || system_id != meta->system_id ||
          !s_editor_system_callback_available(systems, system_id))
      {
        continue;
      }
    }
    else
    {
      continue;
    }
    LDKEditorComponentCallbackFn component_callback = NULL;
    LDKEditorSystemCallbackFn system_callback = NULL;
    if (meta->owner == LDK_EDITOR_CALLBACK_COMPONENT)
    {
      component_callback = s_editor_component_callback_resolve(game, meta);
    }
    else
    {
      system_callback = s_editor_system_callback_resolve(game, meta);
    }
    if (!component_callback && !system_callback)
    {
      continue;
    }
    if (!has_section)
    {
      ldk_ui_horizontal_line(ui);
      ldk_ui_label(ui, "Editor Actions");
      has_section = true;
    }
    ldk_ui_push_id_u32(ui, i);
    bool clicked = ldk_ui_button(ui, meta->label);
    ldk_ui_pop_id(ui);
    if (clicked)
    {
      if (component_callback)
      {
        component_callback(entity, 0.0f);
      }
      else
      {
        system_callback(meta->system_id,
            ldk_scene_systems_data_get(systems, meta->system_id), 0.0f);
      }
      /* Callback may mutate components/hierarchy. Rebuild the Inspector on
       * the next frame rather than retaining potentially invalid pointers. */
      break;
    }
  }
}
