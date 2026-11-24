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

  /* Preserve masks for unchanged layer IDs, but never retain a mask whose
   * dimensions no longer match the replaced terrain grid. */
  u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT] = {0};
  const bool same_grid = component->terrain.width == terrain->width &&
      component->terrain.height == terrain->height;
  LDKTerrainVegetationDensity *density = component->vegetation_density;
  if (density)
  {
    if (same_grid)
      for (u32 i = 0u; i < terrain->vegetation_layer_count; ++i)
        for (u32 j = 0u; j < component->terrain.vegetation_layer_count; ++j)
          if (terrain->vegetation_layers[i].id ==
              component->terrain.vegetation_layers[j].id)
          {
            masks[i] = density->samples[j];
            density->samples[j] = NULL;
            break;
          }
    for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
      free(density->samples[i]);
    memcpy(density->samples, masks, sizeof(masks));
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


static i32 s_terrain_vegetation_index(const LDKTerrainComponent *component,
    u64 layer_id)
{
  if (!component || !layer_id) return -1;
  for (u32 i = 0u; i < component->terrain.vegetation_layer_count; ++i)
    if (component->terrain.vegetation_layers[i].id == layer_id)
      return (i32)i;
  return -1;
}

const u8 *ldk_terrain_component_vegetation_density_get(
    const LDKTerrainComponent *component, u64 layer_id)
{
  i32 i = s_terrain_vegetation_index(component, layer_id);
  return i < 0 || !component->vegetation_density ? NULL :
      component->vegetation_density->samples[i];
}

bool ldk_terrain_component_vegetation_density_set(
    LDKTerrainComponent *component, u64 layer_id, const u8 *samples)
{
  i32 i = s_terrain_vegetation_index(component, layer_id);
  if (i < 0) return false;
  u64 count = (u64)component->terrain.width * component->terrain.height;
  if (!count || count > SIZE_MAX) return false;
  u8 *owned = NULL;
  if (samples)
  {
    owned = malloc((size_t)count);
    if (!owned) return false;
    memcpy(owned, samples, (size_t)count);
  }
  if (!component->vegetation_density)
  {
    component->vegetation_density = calloc(1u,
        sizeof(*component->vegetation_density));
    if (!component->vegetation_density)
    {
      free(owned);
      return false;
    }
  }
  free(component->vegetation_density->samples[i]);
  component->vegetation_density->samples[i] = owned;
  component->revision = component->revision == UINT64_MAX
      ? 1u : component->revision + 1u;
  return true;
}

bool ldk_terrain_component_vegetation_layer_add(
    LDKTerrainComponent *component, const LDKGrassTypeDesc *type,
    float density_scale, u32 seed, u64 *out_id)
{
  if (!component) return false;
  LDKTerrain updated = component->terrain;
  if (!ldk_terrain_vegetation_layer_add(&updated, type,
          density_scale, seed, out_id)) return false;
  component->terrain = updated;
  component->revision = component->revision == UINT64_MAX
      ? 1u : component->revision + 1u;
  return true;
}

bool ldk_terrain_component_vegetation_layer_remove(
    LDKTerrainComponent *component, u64 layer_id)
{
  i32 i = s_terrain_vegetation_index(component, layer_id);
  if (i < 0) return false;
  LDKTerrain updated = component->terrain;
  if (!ldk_terrain_vegetation_layer_remove(&updated, layer_id)) return false;
  if (component->vegetation_density)
  {
    u8 **masks = component->vegetation_density->samples;
    free(masks[i]);
    memmove(masks + i, masks + i + 1,
        (size_t)(component->terrain.vegetation_layer_count - (u32)i - 1u) *
        sizeof(masks[0]));
    masks[updated.vegetation_layer_count] = NULL;
  }
  component->terrain = updated;
  component->revision = component->revision == UINT64_MAX
      ? 1u : component->revision + 1u;
  return true;
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
  target->vegetation_density = NULL;
  LDKImageInfo image = {0};
  if (source->heightmap &&
      !ldk_image_get_info(source->heightmap, &image))
  {
    return false;
  }
  target->revision = source->revision;
  if (!ldk_terrain_component_set(target, &source->terrain,
          source->heightmap ? &image : NULL)) return false;
  for (u32 i = 0u; i < source->terrain.vegetation_layer_count; ++i)
    if (source->vegetation_density &&
        source->vegetation_density->samples[i] &&
        !ldk_terrain_component_vegetation_density_set(target,
            source->terrain.vegetation_layers[i].id,
            source->vegetation_density->samples[i]))
    {
      if (target->vegetation_density)
      {
        for (u32 j = 0u; j < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++j)
          free(target->vegetation_density->samples[j]);
        free(target->vegetation_density);
        target->vegetation_density = NULL;
      }
      ldk_image_destroy(target->heightmap);
      target->heightmap = NULL;
      return false;
    }
  target->revision = source->revision;
  return true;
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
  if (terrain->vegetation_density)
  {
    for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
      free(terrain->vegetation_density->samples[i]);
    free(terrain->vegetation_density);
    terrain->vegetation_density = NULL;
  }
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
