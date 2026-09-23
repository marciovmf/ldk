#include <ldk_game.h>
#include <generated_component_metadata_includes.h>

#define game_component_metadata_count ldk_game_generated_component_metadata_count
#define game_component_metadata_get ldk_game_generated_component_metadata_get
#define game_system_metadata_count ldk_game_generated_system_metadata_count
#define game_system_metadata_get ldk_game_generated_system_metadata_get
#define game_system_descriptor_count ldk_game_generated_system_descriptor_count
#define game_system_descriptor_get ldk_game_generated_system_descriptor_get

#define LDK_COMPONENT_METADATA_IMPLEMENTATION
#include <generated_component_metadata.h>

#undef game_component_metadata_count
#undef game_component_metadata_get
#undef game_system_metadata_count
#undef game_system_metadata_get
#undef game_system_descriptor_count
#undef game_system_descriptor_get

typedef bool (*LDKSystemDescriptorGetFn)(u32 index, LDKSystemDesc *out);

static bool s_game_systems_register(u32 count, LDKSystemDescriptorGetFn get,
    u32 *out_registered)
{
  u32 registered = 0;

  if (!get || !out_registered)
  {
    return false;
  }

  for (u32 i = 0; i < count; ++i)
  {
    LDKSystemDesc desc = {0};
    if (!get(i, &desc))
    {
      ldk_log_error("Failed to get system descriptor %u.\n", i);
      *out_registered = registered;
      return false;
    }

    if (!ldk_ecs_system_register(&desc))
    {
      ldk_log_error("Failed to register system %s.\n",
          desc.name ? desc.name : "<unnamed system>");
      *out_registered = registered;
      return false;
    }

    ++registered;
  }

  *out_registered = registered;
  return true;
}

static void s_game_systems_unregister(
    u32 count, LDKSystemDescriptorGetFn get)
{
  while (count > 0u)
  {
    LDKSystemDesc desc = {0};
    --count;

    if (!get || !get(count, &desc))
    {
      ldk_log_error("Failed to get system descriptor %u during rollback.\n",
          count);
      continue;
    }

    if (!ldk_ecs_system_unregister(desc.id))
    {
      ldk_log_error("Failed to unregister system %s during rollback.\n",
          desc.name ? desc.name : "<unnamed system>");
    }
  }
}

LDK_GAME_API u32 game_component_metadata_count(void)
{
  return ldk_engine_component_metadata_count() +
         ldk_game_generated_component_metadata_count();
}

LDK_GAME_API const LDKComponentMeta *game_component_metadata_get(u32 index)
{
  u32 engine_count = ldk_engine_component_metadata_count();

  if (index < engine_count)
  {
    return ldk_engine_component_metadata_get(index);
  }

  return ldk_game_generated_component_metadata_get(index - engine_count);
}

LDK_GAME_API u32 game_system_metadata_count(void)
{
  return ldk_engine_system_metadata_count() +
         ldk_game_generated_system_metadata_count();
}

LDK_GAME_API const LDKSystemMeta *game_system_metadata_get(u32 index)
{
  u32 engine_count = ldk_engine_system_metadata_count();

  if (index < engine_count)
  {
    return ldk_engine_system_metadata_get(index);
  }

  return ldk_game_generated_system_metadata_get(index - engine_count);
}

LDK_GAME_API bool game_register_systems(void)
{
  u32 engine_registered = 0;
  u32 game_registered = 0;

  if (!s_game_systems_register(ldk_engine_system_descriptor_count(),
          ldk_engine_system_descriptor_get, &engine_registered))
  {
    s_game_systems_unregister(
        engine_registered, ldk_engine_system_descriptor_get);
    return false;
  }

  if (!s_game_systems_register(ldk_game_generated_system_descriptor_count(),
          ldk_game_generated_system_descriptor_get, &game_registered))
  {
    s_game_systems_unregister(
        game_registered, ldk_game_generated_system_descriptor_get);
    s_game_systems_unregister(
        engine_registered, ldk_engine_system_descriptor_get);
    return false;
  }

  return true;
}
