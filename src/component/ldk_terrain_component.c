#include <component/ldk_terrain_component.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LDKTerrainComponent ldk_terrain_component_make_default(void)
{
  LDKTerrainComponent component = {0};
  ldk_terrain_init(&component.terrain);
  /* Match the previous New Terrain authoring default. */
  component.terrain.height_scale = 64.0f;
  component.revision = 1u;
  return component;
}

static bool s_terrain_component_image_valid(const LDKTerrain *terrain,
    const LDKImageInfo *info)
{
  return info && info->pixels &&
      info->width == terrain->width && info->height == terrain->height &&
      info->channel_count == 4u &&
      (info->format == LDK_IMAGE_FORMAT_RGBA8_UNORM ||
       info->format == LDK_IMAGE_FORMAT_RGBA16_UNORM) &&
      info->bytes_per_channel ==
          (info->format == LDK_IMAGE_FORMAT_RGBA16_UNORM ? 2u : 1u) &&
      info->byte_count == (u64)info->width * info->height * 4u *
          info->bytes_per_channel;
}

bool ldk_terrain_component_set(LDKTerrainComponent *component,
    const LDKTerrain *terrain, const LDKImageInfo *heightmap)
{
  if (!component || !ldk_terrain_validate(terrain) ||
      terrain->heightmap_path.length ||
      (heightmap && !s_terrain_component_image_valid(terrain, heightmap)))
  {
    return false;
  }

  LDKImage *owned = NULL;
  if (heightmap)
  {
    owned = ldk_image_create_format(heightmap->width, heightmap->height,
        heightmap->format, heightmap->pixels);
    if (!owned)
    {
      return false;
    }
  }

  LDKImage *old = component->heightmap;
  component->terrain = *terrain;
  component->heightmap = owned;
  component->revision = component->revision == UINT64_MAX
      ? 1u : component->revision + 1u;
  ldk_image_destroy(old);
  return true;
}

bool ldk_terrain_component_heightmap_set(
    LDKTerrainComponent *component, const LDKImageInfo *heightmap)
{
  if (!component)
  {
    return false;
  }
  return ldk_terrain_component_set(component, &component->terrain, heightmap);
}

bool ldk_terrain_component_heightmap_info_get(
    const LDKTerrainComponent *component, LDKImageInfo *out_info)
{
  if (!component || !component->heightmap || !out_info)
  {
    return false;
  }
  return ldk_image_get_info(component->heightmap, out_info);
}

#ifdef LDK_ENGINE
static bool s_terrain_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  if (!component)
  {
    return false;
  }

  /* The ECS has already made a raw copy of initial_value. Never let the new
   * component retain that borrowed image pointer: deep-copy it here. */
  LDKTerrainComponent *target = component;
  if (!initial_value)
  {
    *target = ldk_terrain_component_make_default();
    return true;
  }

  const LDKTerrainComponent *source = initial_value;
  target->heightmap = NULL;
  LDKImageInfo image = {0};
  if (source->heightmap &&
      !ldk_image_get_info(source->heightmap, &image))
  {
    return false;
  }
  target->revision = source->revision;
  return ldk_terrain_component_set(target, &source->terrain,
      source->heightmap ? &image : NULL);
}

static void s_terrain_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  LDKTerrainComponent *terrain = component;
  if (!terrain)
  {
    return;
  }
  ldk_image_destroy(terrain->heightmap);
  terrain->heightmap = NULL;
}

LDKComponentDesc ldk_terrain_component_desc(u32 initial_capacity)
{
  static const u32 required[] = {LDK_COMPONENT_TYPE_TRANSFORM};
  LDKComponentDesc desc = {0};
  desc.name = "TerrainComponent";
  desc.type = LDK_COMPONENT_TYPE_TERRAIN;
  desc.entry_size = sizeof(LDKTerrainComponent);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required;
  desc.required_component_count = 1u;
  desc.attach = s_terrain_attach;
  desc.destroy = s_terrain_destroy;
  return desc;
}
#endif
