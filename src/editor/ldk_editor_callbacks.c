#include "ldk_editor_callbacks_internal.h"

#include <editor/ldk_editor_callback.h>
#include <ldk_scene.h>
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

/* Engine annotations and game annotations share the same metadata format.
 * Game symbols must still be resolved after every DLL reload. */
static const LDKEditorCallbackMeta *s_editor_callback_at(
    LDKGame *game, u32 index, u32 *out_local_index, bool *out_engine)
{
  u32 engine_count = ldk_engine_editor_callback_metadata_count();
  if (index < engine_count)
  {
    *out_local_index = index;
    *out_engine = true;
    return ldk_engine_editor_callback_metadata_get(index);
  }
  LDKGameEditorCallbackCountFn count = NULL;
  LDKGameEditorCallbackGetFn get = NULL;
  if (!s_editor_callbacks_api(game, &count, &get))
  {
    return NULL;
  }
  index -= engine_count;
  *out_local_index = index;
  *out_engine = false;
  return index < count() ? get(index) : NULL;
}

static u32 s_editor_callback_count(LDKGame *game)
{
  LDKGameEditorCallbackCountFn count = NULL;
  LDKGameEditorCallbackGetFn get = NULL;
  return ldk_engine_editor_callback_metadata_count() +
      (s_editor_callbacks_api(game, &count, &get) ? count() : 0u);
}

/* Comet hashes component IDs, while built-in ECS components have reserved
 * IDs. Translate through the existing scene metadata mapping. Game-defined
 * components already use their generated IDs. */
static u32 s_editor_callback_component_runtime_type(LDKGame *game,
    const LDKEditorCallbackMeta *callback, bool engine)
{
  u32 count = engine ? ldk_engine_component_metadata_count() :
      (game && game->metadata_count ? game->metadata_count() : 0u);

  for (u32 i = 0; i < count; ++i)
  {
    const LDKComponentMeta *meta = engine
        ? ldk_engine_component_metadata_get(i)
        : game->metadata_get ? game->metadata_get(i) : NULL;
    if (meta && meta->type == callback->component_type)
    {
      return ldk_scene_component_meta_runtime_type(meta);
    }
  }
  return callback->component_type;
}

static LDKEditorComponentCallbackFn s_editor_component_callback_resolve(
    LDKGame *game, const LDKEditorCallbackMeta *meta,
    bool engine, u32 index)
{
  if (engine)
  {
    return ldk_engine_editor_component_callback_get(index);
  }
  return (LDKEditorComponentCallbackFn)ldk_os_library_fuction_ptr_get(
      game->lib, meta->symbol);
}

static LDKEditorSystemCallbackFn s_editor_system_callback_resolve(
    LDKGame *game, const LDKEditorCallbackMeta *meta,
    bool engine, u32 index)
{
  if (engine)
  {
    return ldk_engine_editor_system_callback_get(index);
  }
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
  const u32 total = s_editor_callback_count(game);
  for (u32 i = 0u; i < total; ++i)
  {
    u32 local_index = 0u;
    bool engine = false;
    const LDKEditorCallbackMeta *meta =
        s_editor_callback_at(game, i, &local_index, &engine);
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
          s_editor_system_callback_resolve(game, meta, engine, local_index);
      if (callback)
      {
        callback(meta->system_id,
            ldk_scene_systems_data_get(systems, meta->system_id), dt);
      }
    }
    else if (meta->owner == LDK_EDITOR_CALLBACK_COMPONENT)
    {
      LDKEditorComponentCallbackFn callback =
          s_editor_component_callback_resolve(game, meta, engine, local_index);
      if (callback)
      {
        LDKEditorCallbackIteration iteration = {
            s_editor_callback_component_runtime_type(game, meta, engine),
            dt, callback};
        (void)ldk_ecs_entity_foreach(s_editor_callback_entity_update, &iteration);
      }
    }
  }
}

bool ldki_editor_callbacks_draw(LDKGame *game, LDKUIContext *ui,
    const LDKSceneSystems *systems, LDKEntity entity, u64 system_id,
    u32 component_type)
{
  if (!ui)
  {
    return false;
  }
  const u32 total = s_editor_callback_count(game);
  bool has_section = false;
  for (u32 i = 0u; i < total; ++i)
  {
    u32 local_index = 0u;
    bool engine = false;
    const LDKEditorCallbackMeta *meta =
        s_editor_callback_at(game, i, &local_index, &engine);
    if (!meta || !(meta->flags & LDK_EDITOR_CALLBACK_MANUAL))
    {
      continue;
    }
    if (meta->owner == LDK_EDITOR_CALLBACK_COMPONENT)
    {
      const u32 runtime_type =
          s_editor_callback_component_runtime_type(game, meta, engine);
      if (system_id || x_handle_is_null(entity) ||
          runtime_type != component_type ||
          !ldk_ecs_component_get_const(entity, runtime_type))
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
      component_callback = s_editor_component_callback_resolve(
          game, meta, engine, local_index);
    }
    else
    {
      system_callback = s_editor_system_callback_resolve(
          game, meta, engine, local_index);
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
      /* The caller must finish the UI scope and stop drawing: callbacks may
       * change or remove components, invalidating its iteration pointers. */
      return true;
    }
  }
  return false;
}
