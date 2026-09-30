/**
 * @file ldk_text3d.c
 * @brief World-space text source component data.
 */

#include <component/ldk_text3d.h>
#include <module/ldk_entity.h>
#include <module/ldk_asset_manager.h>

#include <math.h>
#include <string.h>

static LDKText3DComponent s_text3d_make_default(void)
{
  LDKText3DComponent text = {0};
  static const char default_text[] = "Text";

  memcpy(text.text.buf, default_text, sizeof(default_text));
  text.text.length = sizeof(default_text) - 1u;
  text.font = ldk_asset_font_null();
  text.pixel_height = 64.0f;
  text.color = 0xffffffffu;
  text.billboard = false;
  text.depth_test = true;
  return text;
}

static bool s_text3d_attach(LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, const void *initial_value, void *user)
{
  LDKText3DComponent *text = component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !text ||
      !ldk_entity_internal_flags_has(
          entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM))
  {
    return false;
  }

  if (!initial_value)
  {
    *text = s_text3d_make_default();
  }

  if (text->text.length > X_SMALLSTR_MAX_LENGTH ||
      text->text.buf[text->text.length] != 0 ||
      strlen(text->text.buf) != text->text.length)
  {
    return false;
  }

  if (!isfinite(text->pixel_height) || text->pixel_height <= 0.0f)
  {
    text->pixel_height = 64.0f;
  }

  ldk_entity_internal_flags_add(
      entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
  return true;
}

static void s_text3d_destroy(LDKEntityRegistry *entity_registry,
    LDKComponentRegistry *component_registry, LDKEntity entity,
    void *component, u32 component_index, void *user)
{
  (void)component_registry;
  (void)component;
  (void)component_index;
  (void)user;

  if (!entity_registry)
  {
    return;
  }

  if (!ldk_entity_component_has(
          entity_registry, entity, LDK_COMPONENT_TYPE_MESH_SOURCE) &&
      !ldk_entity_component_has(entity_registry, entity,
          LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE))
  {
    ldk_entity_internal_flags_remove(
        entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
  }
}

#ifdef LDK_ENGINE
LDKComponentDesc ldk_text3d_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};

  desc.name = "Text3D";
  desc.type = LDK_COMPONENT_TYPE_TEXT3D;
  desc.entry_size = sizeof(LDKText3DComponent);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_text3d_attach;
  desc.destroy = s_text3d_destroy;
  return desc;
}
#endif // LDK_ENGINE
