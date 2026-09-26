/**
 * @file ldk_post_processing.c
 * @brief Camera post-processing component data.
 */

#include <component/ldk_post_processing.h>

#ifdef LDK_ENGINE

#include <module/ldk_component.h>
#include <module/ldk_entity.h>

#include <string.h>

static LDKPostProcessing s_post_processing_make_default(void)
{
  LDKPostProcessing post_processing = {0};
  post_processing.enabled = true;
  post_processing.tonemapping_enabled = false;
  post_processing.exposure = 0.0f;
  post_processing.blur_enabled = false;
  post_processing.blur_strength = 0.5f;
  post_processing.blur_focus_size = 0.25f;
  post_processing.blur_focus_feather = 0.15f;
  post_processing.blur_inverted = false;
  post_processing.blur_focus_offset_x = 0.0f;
  post_processing.blur_focus_offset_y = 0.0f;
  post_processing.color_enabled = false;
  post_processing.brightness = 0.0f;
  post_processing.contrast = 1.0f;
  post_processing.saturation = 1.0f;
  return post_processing;
}

static bool s_post_processing_attach(LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value,
    void *user)
{
  (void)entity_registry;
  (void)component_registry;
  (void)entity;
  (void)component_index;
  (void)user;

  if (component == NULL)
  {
    return false;
  }

  if (initial_value != NULL)
  {
    memcpy(component, initial_value, sizeof(LDKPostProcessing));
  }
  else
  {
    *(LDKPostProcessing *)component = s_post_processing_make_default();
  }

  return true;
}

LDKComponentDesc ldk_post_processing_component_desc(u32 initial_capacity)
{
  static const u32 required_components[] = {
      LDK_COMPONENT_TYPE_CAMERA};
  LDKComponentDesc desc = {0};

  desc.name = "PostProcessing";
  desc.type = LDK_COMPONENT_TYPE_POST_PROCESSING;
  desc.entry_size = sizeof(LDKPostProcessing);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required_components;
  desc.required_component_count =
      (u32)(sizeof(required_components) / sizeof(required_components[0]));
  desc.attach = s_post_processing_attach;
  return desc;
}

#endif // LDK_ENGINE
