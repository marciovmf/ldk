#include <ldk_game.h>
#include <generated_component_metadata_includes.h>

#define game_component_metadata_count ldk_game_generated_component_metadata_count
#define game_component_metadata_get ldk_game_generated_component_metadata_get

#define LDK_COMPONENT_METADATA_IMPLEMENTATION
#include <generated_component_metadata.h>

#undef game_component_metadata_count
#undef game_component_metadata_get

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
