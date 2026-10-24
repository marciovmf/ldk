#include <ldk_game.h>
#include <generated_engine_component_metadata_includes.h>

#define game_component_metadata_count ldk_engine_generated_component_metadata_count
#define game_component_metadata_get ldk_engine_generated_component_metadata_get
#define game_system_metadata_count ldk_engine_generated_system_metadata_count
#define game_system_metadata_get ldk_engine_generated_system_metadata_get
#define game_system_descriptor_count ldk_engine_generated_system_descriptor_count
#define game_system_descriptor_get ldk_engine_generated_system_descriptor_get

#define game_editor_callback_metadata_count ldk_engine_generated_editor_callback_metadata_count
#define game_editor_callback_metadata_get ldk_engine_generated_editor_callback_metadata_get
#define game_editor_component_callback_function_get ldk_engine_generated_editor_component_callback_function_get
#define game_editor_system_callback_function_get ldk_engine_generated_editor_system_callback_function_get

#define LDK_COMPONENT_METADATA_IMPLEMENTATION
#include <generated_engine_component_metadata.h>

#undef game_component_metadata_count
#undef game_component_metadata_get
#undef game_system_metadata_count
#undef game_system_metadata_get
#undef game_system_descriptor_count
#undef game_system_descriptor_get
#undef game_editor_callback_metadata_count
#undef game_editor_callback_metadata_get
#undef game_editor_component_callback_function_get
#undef game_editor_system_callback_function_get

LDK_API u32 ldk_engine_component_metadata_count(void)
{
  return ldk_engine_generated_component_metadata_count();
}

LDK_API const LDKComponentMeta *ldk_engine_component_metadata_get(u32 index)
{
  return ldk_engine_generated_component_metadata_get(index);
}

LDK_API u32 ldk_engine_system_metadata_count(void)
{
  return ldk_engine_generated_system_metadata_count();
}

LDK_API const LDKSystemMeta *ldk_engine_system_metadata_get(u32 index)
{
  return ldk_engine_generated_system_metadata_get(index);
}

LDK_API u32 ldk_engine_system_descriptor_count(void)
{
  return ldk_engine_generated_system_descriptor_count();
}

LDK_API bool ldk_engine_system_descriptor_get(
    u32 index, LDKSystemDesc *out)
{
  return ldk_engine_generated_system_descriptor_get(index, out);
}

#ifdef LDK_EDITOR
LDK_API u32 ldk_engine_editor_callback_metadata_count(void)
{
  return ldk_engine_generated_editor_callback_metadata_count();
}

LDK_API const LDKEditorCallbackMeta *ldk_engine_editor_callback_metadata_get(u32 index)
{
  return ldk_engine_generated_editor_callback_metadata_get(index);
}

LDK_API LDKEditorComponentCallbackFn ldk_engine_editor_component_callback_get(u32 index)
{
  return ldk_engine_generated_editor_component_callback_function_get(index);
}

LDK_API LDKEditorSystemCallbackFn ldk_engine_editor_system_callback_get(u32 index)
{
  return ldk_engine_generated_editor_system_callback_function_get(index);
}
#endif
