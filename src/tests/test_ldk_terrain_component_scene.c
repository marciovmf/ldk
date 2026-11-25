#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_HPOOL
#define X_IMPL_STRING
#define X_IMPL_STRBUILDER
#define X_IMPL_TML
#endif

#include <component/ldk_terrain_component.h>
#include <stdx/stdx_tml.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exercise the complete scene/entity/component/fields hierarchy, not a
 * standalone fields node. A previous scene writer gave the embedded terrain
 * only six spaces instead of twelve, making it a sibling of components and
 * silently causing flat terrain on reload. */
static int test_component_render_settings(void)
{
  LDKTerrainComponent a = ldk_terrain_component_make_default();
  LDKTerrainComponent b = ldk_terrain_component_make_default();
  ASSERT_EQ(a.chunk_size, 16u);
  ASSERT_EQ(a.mesh_sample_step, 1u);
  ASSERT_TRUE(a.cull);
  ASSERT_TRUE(a.casts_shadows);
  b.chunk_size = 32u;
  b.mesh_sample_step = 4u;
  b.cull = false;
  b.casts_shadows = false;
  ASSERT_EQ(a.chunk_size, 16u);
  ASSERT_EQ(b.chunk_size, 32u);
  ASSERT_EQ(a.mesh_sample_step, 1u);
  ASSERT_EQ(b.mesh_sample_step, 4u);
  ASSERT_TRUE(!b.cull && !b.casts_shadows);
  ASSERT_TRUE(ldk_terrain_component_set(&b, &b.terrain, NULL));
  ASSERT_EQ(b.chunk_size, 32u);
  ASSERT_EQ(b.mesh_sample_step, 4u);
  ASSERT_TRUE(!b.cull && !b.casts_shadows);
  return 0;
}

static LDKGrassTypeDesc s_vegetation_type(const char *name);

static int test_component_metadata_does_not_replace_heightmap(void)
{
  LDKTerrainComponent component = ldk_terrain_component_make_default();
  component.terrain.width = 2u;
  component.terrain.height = 2u;
  const u16 pixels[2u * 2u * 4u] = {0};
  LDKImageInfo image = {2u, 2u, 4u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, sizeof(pixels), (u8 *)pixels};
  ASSERT_TRUE(ldk_terrain_component_set(
      &component, &component.terrain, &image));
  const u64 source_revision = component.heightmap_revision;
  const LDKImage *source_image = component.heightmap;
  ASSERT_TRUE(source_image != NULL);

  LDKGrassTypeDesc grass = s_vegetation_type("A");
  u64 id_a = 0u, id_b = 0u;
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_add(
      &component, &grass, 1.0f, 0u, &id_a));
  grass = s_vegetation_type("B");
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_add(
      &component, &grass, 1.0f, 0u, &id_b));
  const u8 mask_a[] = {255u, 0u, 0u, 255u};
  const u8 mask_b[] = {0u, 255u, 255u, 0u};
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &component, id_a, mask_a));
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &component, id_b, mask_b));

  LDKTerrain updated = component.terrain;
  updated.vegetation_layers[0] = component.terrain.vegetation_layers[1];
  updated.vegetation_layers[1] = component.terrain.vegetation_layers[0];
  updated.vegetation_layers[0].density_scale = 0.33f;
  ASSERT_TRUE(ldk_terrain_component_metadata_set(&component, &updated));
  ASSERT_TRUE(component.heightmap == source_image);
  ASSERT_EQ(component.heightmap_revision, source_revision);
  ASSERT_EQ(component.terrain.vegetation_layers[0].id, id_b);
  ASSERT_EQ(component.terrain.vegetation_layers[1].id, id_a);
  ASSERT_TRUE(memcmp(ldk_terrain_component_vegetation_density_get(
      &component, id_a), mask_a, sizeof(mask_a)) == 0);
  ASSERT_TRUE(memcmp(ldk_terrain_component_vegetation_density_get(
      &component, id_b), mask_b, sizeof(mask_b)) == 0);

  updated.width = 4u;
  ASSERT_FALSE(ldk_terrain_component_metadata_set(&component, &updated));
  ASSERT_TRUE(component.heightmap == source_image);
  ASSERT_EQ(component.heightmap_revision, source_revision);
  ASSERT_TRUE(ldk_terrain_component_set(
      &component, &component.terrain, &image));
  ASSERT_TRUE(component.heightmap_revision != source_revision);
  ldk_image_destroy(component.heightmap);
  if (component.vegetation_density)
  {
    for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
      free(component.vegetation_density->samples[i]);
    free(component.vegetation_density);
  }
  return 0;
}

static int test_component_local_heightmap_edits(void)
{
  LDKTerrainComponent component = ldk_terrain_component_make_default();
  component.terrain.width = 8u;
  component.terrain.height = 8u;
  u16 pixels[8u * 8u * 4u] = {0};
  LDKImageInfo image = {8u, 8u, 4u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, sizeof(pixels), (const u8 *)pixels};
  ASSERT_TRUE(ldk_terrain_component_set(&component, &component.terrain, &image));
  const LDKImage *original_image = component.heightmap;
  LDKImageInfo snapshot = {0};
  ASSERT_TRUE(ldk_image_get_info(component.heightmap, &snapshot));
  const u8 *original_pixels = snapshot.pixels;
  const u64 original_revision = component.heightmap_revision;

  /* Initial region and a second overlapping region accumulate without
   * copying the entire owned image or losing the first dirty rectangle. */
  pixels[(2u * 8u + 2u) * 4u] = 100u;
  ASSERT_TRUE(ldk_terrain_component_heightmap_region_set(
      &component, &image, 2u, 2u, 1u, 1u));
  ASSERT_TRUE(component.heightmap == original_image);
  ASSERT_EQ(component.heightmap_dirty_base_revision, original_revision);
  ASSERT_EQ(component.heightmap_grass_dirty_base_revision, original_revision);
  ASSERT_EQ(component.heightmap_dirty_region.x, 2u);
  ASSERT_EQ(component.heightmap_dirty_region.y, 2u);
  ASSERT_EQ(component.heightmap_dirty_region.width, 1u);
  ASSERT_EQ(component.heightmap_dirty_region.height, 1u);

  pixels[(3u * 8u + 5u) * 4u] = 200u;
  ASSERT_TRUE(ldk_terrain_component_heightmap_region_set(
      &component, &image, 5u, 3u, 1u, 1u));
  ASSERT_TRUE(component.heightmap_dirty_valid);
  ASSERT_TRUE(component.heightmap_grass_dirty_valid);
  ASSERT_EQ(component.heightmap_dirty_base_revision, original_revision);
  ASSERT_EQ(component.heightmap_grass_dirty_base_revision, original_revision);
  ASSERT_EQ(component.heightmap_dirty_region.x, 2u);
  ASSERT_EQ(component.heightmap_dirty_region.y, 2u);
  ASSERT_EQ(component.heightmap_dirty_region.width, 4u);
  ASSERT_EQ(component.heightmap_dirty_region.height, 2u);
  ASSERT_EQ(component.heightmap_grass_dirty_region.x, 2u);
  ASSERT_EQ(component.heightmap_grass_dirty_region.width, 4u);
  ASSERT_TRUE(ldk_image_get_info(component.heightmap, &snapshot));
  ASSERT_TRUE(snapshot.pixels == original_pixels);
  const u16 *stored = (const u16 *)snapshot.pixels;
  ASSERT_EQ(stored[(2u * 8u + 2u) * 4u], 100u);
  ASSERT_EQ(stored[(3u * 8u + 5u) * 4u], 200u);
  ASSERT_EQ(stored[(3u * 8u + 4u) * 4u], 0u);
  ASSERT_FALSE(ldk_terrain_component_heightmap_region_set(
      &component, &image, 8u, 0u, 1u, 1u));
  ASSERT_FALSE(ldk_terrain_component_heightmap_region_set(
      &component, &image, 7u, 0u, 2u, 1u));

  /* Independently acknowledged consumers start from their own revisions. */
  component.heightmap_dirty_valid = false;
  pixels[(1u * 8u + 1u) * 4u] = 300u;
  const u64 new_revision = component.heightmap_revision;
  ASSERT_TRUE(ldk_terrain_component_heightmap_region_set(
      &component, &image, 1u, 1u, 1u, 1u));
  ASSERT_EQ(component.heightmap_dirty_base_revision, new_revision);
  ASSERT_EQ(component.heightmap_dirty_region.x, 1u);
  ASSERT_EQ(component.heightmap_dirty_region.width, 1u);
  ASSERT_EQ(component.heightmap_grass_dirty_base_revision, original_revision);
  ASSERT_EQ(component.heightmap_grass_dirty_region.x, 1u);
  ASSERT_EQ(component.heightmap_grass_dirty_region.y, 1u);
  ASSERT_EQ(component.heightmap_grass_dirty_region.width, 5u);
  ASSERT_EQ(component.heightmap_grass_dirty_region.height, 3u);

  /* A full replacement invalidates both incremental tracking cursors. */
  ASSERT_TRUE(ldk_terrain_component_set(&component, &component.terrain, &image));
  ASSERT_FALSE(component.heightmap_dirty_valid);
  ASSERT_FALSE(component.heightmap_grass_dirty_valid);
  ldk_image_destroy(component.heightmap);
  return 0;
}

static int test_component_embedded_scene_roundtrip(void)
{
  LDKTerrainComponent source = ldk_terrain_component_make_default();
  source.terrain.width = 256u;
  source.terrain.height = 256u;
  source.terrain.height_scale = 81.0f;
  source.terrain.height_offset = -7.0f;
  const size_t sample_count = (size_t)256u * 256u;
  u16 *pixels = calloc(sample_count * 4u, sizeof(u16));
  ASSERT_TRUE(pixels != NULL);
  for (size_t i = 0; i < sample_count; ++i)
    pixels[i * 4u] = (u16)((i * 3079u) & 0xffffu);
  LDKImageInfo info = {256u, 256u, 4u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, sample_count * 8u, (u8 *)pixels};
  ASSERT_TRUE(ldk_terrain_component_set(
      &source, &source.terrain, &info));
  free(pixels);
  pixels = NULL;

  char *terrain_text = NULL;
  ASSERT_TRUE(ldk_terrain_component_heightmap_info_get(&source, &info));
  ASSERT_TRUE(ldk_terrain_to_tml_with_heightmap(
      &source.terrain, &info, &terrain_text));
  ASSERT_TRUE(strlen(terrain_text) > 256u);

  const char *header = "terrain:\n";
  ASSERT_TRUE(strncmp(terrain_text, header, strlen(header)) == 0);
  const char *payload = terrain_text + strlen(header);
  const size_t capacity = strlen(payload) + 8192u;
  char *scene_text = malloc(capacity);
  ASSERT_TRUE(scene_text != NULL);
  size_t used = (size_t)snprintf(scene_text, capacity,
      "scene:\n"
      "  version: 1\n"
      "  entities:\n"
      "    - entity: 0\n"
      "      components:\n"
      "        - type: 0\n"
      "          fields:\n"
      "            terrain:\n");
  const char *cursor = payload;
  while (*cursor)
  {
    const char *newline = strchr(cursor, '\n');
    const size_t count = newline ? (size_t)(newline - cursor) + 1u
                                  : strlen(cursor);
    ASSERT_TRUE(used + count + 12u < capacity);
    memset(scene_text + used, ' ', 12u);
    used += 12u;
    memcpy(scene_text + used, cursor, count);
    used += count;
    cursor += count;
  }
  scene_text[used] = '\0';

  TMLParseResult parsed = tml_parse(scene_text);
  ASSERT_TRUE(parsed.ok);
  const TMLNode *scene = tml_path_find_node(parsed.document, "scene");
  ASSERT_TRUE(scene != NULL);
  const TMLNode *entities = tml_node_find_child(
      parsed.document, scene, "entities");
  ASSERT_TRUE(entities != NULL);
  const TMLNode *entity = tml_node_child_at(parsed.document, entities, 0u);
  ASSERT_TRUE(entity != NULL);
  const TMLNode *components = tml_node_find_child(
      parsed.document, entity, "components");
  ASSERT_TRUE(components != NULL);
  const TMLNode *component = tml_node_child_at(parsed.document,
      components, 0u);
  ASSERT_TRUE(component != NULL);
  const TMLNode *fields = tml_node_find_child(parsed.document,
      component, "fields");
  ASSERT_TRUE(fields != NULL);
  const TMLNode *nested = tml_node_find_child(
      parsed.document, fields, "terrain");
  ASSERT_TRUE(tml_node_find_child(parsed.document, entity, "terrain") == NULL);
  ASSERT_TRUE(nested != NULL);
  LDKTerrain restored;
  LDKImage *decoded = NULL;
  ASSERT_TRUE(ldk_terrain_from_tml_node_with_heightmap(
      &restored, &decoded, parsed.document, nested));
  ASSERT_TRUE(decoded != NULL);
  ASSERT_EQ(restored.width, 256u);
  ASSERT_EQ(restored.height, 256u);
  ASSERT_EQ(restored.height_scale, 81.0f);
  ASSERT_EQ(restored.height_offset, -7.0f);
  LDKImageInfo check = {0};
  ASSERT_TRUE(ldk_image_get_info(decoded, &check));
  for (size_t i = 0; i < sample_count; ++i)
  {
    u16 sample;
    memcpy(&sample, check.pixels + i * 8u, sizeof(sample));
    ASSERT_EQ(sample, (u16)((i * 3079u) & 0xffffu));
  }

  LDKTerrainComponent copy = ldk_terrain_component_make_default();
  ASSERT_TRUE(ldk_terrain_component_set(&copy, &restored, &check));
  ASSERT_TRUE(copy.heightmap != decoded);
  ldk_image_destroy(decoded);
  decoded = NULL;
  ASSERT_TRUE(ldk_terrain_component_heightmap_info_get(&copy, &check));
  ASSERT_EQ(check.width, 256u);
  ASSERT_EQ(copy.terrain.height_scale, 81.0f);

  /* Invalid metadata must preserve the owned image and component data. */
  LDKTerrain invalid = copy.terrain;
  invalid.cell_size = 0.0f;
  LDKImage *owned = copy.heightmap;
  u64 revision = copy.revision;
  ASSERT_FALSE(ldk_terrain_component_set(&copy, &invalid, &check));
  ASSERT_TRUE(copy.heightmap == owned);
  ASSERT_EQ(copy.revision, revision);

  ldk_image_destroy(copy.heightmap);
  ldk_image_destroy(source.heightmap);
  tml_document_free(parsed.document);
  free(scene_text);
  free(terrain_text);
  return 0;
}

/* Scenes written by the faulty implementation put the terrain alongside
 * components, with an otherwise intact base64 heightmap. Verify that this
 * legacy location remains accessible for the loader's recovery path. */
static int test_component_legacy_scene_nesting(void)
{
  LDKTerrain terrain;
  ldk_terrain_init(&terrain);
  terrain.width = 4u;
  terrain.height = 4u;
  terrain.height_scale = 80.0f;
  u16 pixels[4u * 4u * 4u] = {0};
  for (u32 i = 0u; i < 16u; ++i)
    pixels[i * 4u] = (u16)(i * 4096u);
  LDKImageInfo info = {4u, 4u, 4u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, sizeof(pixels), (u8 *)pixels};
  char *terrain_text = NULL;
  ASSERT_TRUE(ldk_terrain_to_tml_with_heightmap(
      &terrain, &info, &terrain_text));
  const char *header = "terrain:\n";
  const char *payload = terrain_text + strlen(header);
  size_t capacity = strlen(payload) + 1024u;
  char *scene_text = malloc(capacity);
  ASSERT_TRUE(scene_text != NULL);
  size_t used = (size_t)snprintf(scene_text, capacity,
      "scene:\n"
      "  entities:\n"
      "    - entity: 0\n"
      "      components:\n"
      "        - type: 0\n"
      "          fields:\n"
      "      terrain:\n");
  const char *cursor = payload;
  while (*cursor)
  {
    const char *newline = strchr(cursor, '\n');
    size_t count = newline ? (size_t)(newline - cursor) + 1u
                           : strlen(cursor);
    ASSERT_TRUE(used + count + 7u < capacity);
    memset(scene_text + used, ' ', 6u);
    used += 6u;
    memcpy(scene_text + used, cursor, count);
    used += count;
    cursor += count;
  }
  scene_text[used] = '\0';
  TMLParseResult parsed = tml_parse(scene_text);
  ASSERT_TRUE(parsed.ok);
  const TMLNode *scene = tml_path_find_node(parsed.document, "scene");
  ASSERT_TRUE(scene != NULL);
  const TMLNode *entities = tml_node_find_child(
      parsed.document, scene, "entities");
  ASSERT_TRUE(entities != NULL);
  const TMLNode *entity = tml_node_child_at(parsed.document, entities, 0u);
  ASSERT_TRUE(entity != NULL);
  const TMLNode *components = tml_node_find_child(
      parsed.document, entity, "components");
  ASSERT_TRUE(components != NULL);
  const TMLNode *component = tml_node_child_at(parsed.document,
      components, 0u);
  ASSERT_TRUE(component != NULL);
  const TMLNode *fields = tml_node_find_child(parsed.document,
      component, "fields");
  ASSERT_TRUE(fields != NULL);
  ASSERT_TRUE(tml_node_find_child(parsed.document, fields, "terrain") == NULL);
  const TMLNode *misplaced = tml_node_find_child(
      parsed.document, entity, "terrain");
  ASSERT_TRUE(misplaced != NULL);
  LDKTerrain restored;
  LDKImage *image = NULL;
  ASSERT_TRUE(ldk_terrain_from_tml_node_with_heightmap(
      &restored, &image, parsed.document, misplaced));
  ASSERT_TRUE(image != NULL);
  LDKImageInfo restored_info = {0};
  ASSERT_TRUE(ldk_image_get_info(image, &restored_info));
  for (u32 i = 0u; i < 16u; ++i)
  {
    u16 value = 0u;
    memcpy(&value, restored_info.pixels + i * 8u, sizeof(value));
    ASSERT_EQ(value, (u16)(i * 4096u));
  }
  ldk_image_destroy(image);
  tml_document_free(parsed.document);
  free(scene_text);
  free(terrain_text);
  return 0;
}

static int test_component_flat_terrain(void)
{
  LDKTerrainComponent component = ldk_terrain_component_make_default();
  ASSERT_TRUE(ldk_terrain_validate(&component.terrain));
  ASSERT_TRUE(component.heightmap == NULL);
  char *text = NULL;
  ASSERT_TRUE(ldk_terrain_to_tml_with_heightmap(
      &component.terrain, NULL, &text));
  ASSERT_TRUE(strstr(text, "heightmap_samples") == NULL);
  LDKTerrain restored;
  LDKImage *image = NULL;
  ASSERT_TRUE(ldk_terrain_from_tml_with_heightmap(
      &restored, &image, text));
  ASSERT_TRUE(image == NULL);
  ASSERT_EQ(restored.width, component.terrain.width);
  ASSERT_EQ(restored.height, component.terrain.height);
  free(text);
  return 0;
}

/* R8 density is embedded per vegetation layer, independently of the
 * heightmap. Exercise ownership, stable layer IDs and actual TML parsing. */
static LDKGrassTypeDesc s_vegetation_type(const char *name)
{
  LDKGrassTypeDesc type;
  ldk_grass_type_desc_defaults(&type);
  (void)x_smallstr_init(&type.name, name);
  return type;
}

static int test_component_vegetation_density_scene_roundtrip(void)
{
  LDKTerrainComponent source = ldk_terrain_component_make_default();
  source.terrain.width = 8u;
  source.terrain.height = 8u;
  u64 first_id = 0u, second_id = 0u;
  LDKGrassTypeDesc grass_type = s_vegetation_type("Grass");
  LDKGrassTypeDesc thorn_type = s_vegetation_type("Thorns");
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_add(
      &source, &grass_type, 0.8f, 11u, &first_id));
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_add(
      &source, &thorn_type, 1.5f, 22u, &second_id));
  ASSERT_TRUE(first_id != second_id);
  /* A terrain owns both the type configuration and its corresponding mask. */
  for (u32 i = 0u; i < 2u; ++i)
  {
    LDKTerrainVegetationLayer *layer = &source.terrain.vegetation_layers[i];
    LDKGrassTypeDesc *type = &layer->type;
    type->blade_preset = i == 0u ? LDK_GRASS_BLADE_PRESET_THIN :
        LDK_GRASS_BLADE_PRESET_THORNY;
    type->density = i == 0u ? 0.6f : 2.8f;
    type->min_height = 0.25f;
    type->max_height = i == 0u ? 0.7f : 1.6f;
    type->blade_width = 0.1f;
    type->bottom_color = 0x557755ffu;
    type->top_color = i == 0u ? 0x99aa99ffu : 0xdd4477ffu;
  }
  ASSERT_TRUE(ldk_terrain_validate(&source.terrain));
  u8 first[64], second[64];
  for (u32 i = 0u; i < 64u; ++i)
  {
    first[i] = (u8)(i * 3u);
    second[i] = (u8)(255u - i * 4u);
  }
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &source, first_id, first));
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &source, second_id, second));
  first[0] = second[0] = 0u; /* Confirm the component copied the input. */
  ASSERT_EQ(source.vegetation_density->samples[0][0], 0u);
  ASSERT_EQ(source.vegetation_density->samples[1][0], 255u);
  ASSERT_EQ(source.vegetation_density->samples[0][1], 3u);

  char *text = NULL;
  ASSERT_TRUE(ldk_terrain_to_tml_with_component_data(
      &source.terrain, NULL,
      (const u8 *const *)source.vegetation_density->samples, &text));
  ASSERT_TRUE(strstr(text, "density_encoding: \"u8-base64\"") != NULL);
  ASSERT_TRUE(strstr(text, "heightmap_samples") == NULL);
  TMLParseResult parsed = tml_parse(text);
  ASSERT_TRUE(parsed.ok);
  const TMLNode *node = tml_root_node_at(parsed.document, 0u);
  LDKTerrain terrain;
  LDKImage *image = NULL;
  u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT] = {0};
  ASSERT_TRUE(ldk_terrain_from_tml_node_with_component_data(
      &terrain, &image, masks, parsed.document, node));
  ASSERT_TRUE(image == NULL);
  ASSERT_EQ(terrain.vegetation_layer_count, 2u);
  ASSERT_EQ(terrain.vegetation_layers[0].id, first_id);
  ASSERT_EQ(terrain.vegetation_layers[1].id, second_id);
  ASSERT_EQ(terrain.vegetation_layers[0].type.blade_preset,
      LDK_GRASS_BLADE_PRESET_THIN);
  ASSERT_EQ(terrain.vegetation_layers[1].type.blade_preset,
      LDK_GRASS_BLADE_PRESET_THORNY);
  ASSERT_EQ(terrain.vegetation_layers[1].type.top_color,
      0xdd4477ffu);
  ASSERT_EQ(memcmp(masks[0], source.vegetation_density->samples[0], 64u), 0);
  ASSERT_EQ(memcmp(masks[1], source.vegetation_density->samples[1], 64u), 0);

  LDKTerrainComponent copy = ldk_terrain_component_make_default();
  ASSERT_TRUE(ldk_terrain_component_set(&copy, &terrain, NULL));
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &copy, first_id, masks[0]));
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &copy, second_id, masks[1]));
  ASSERT_TRUE(copy.vegetation_density->samples[0] != masks[0]);
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_remove(
      &copy, first_id));
  ASSERT_EQ(copy.terrain.vegetation_layer_count, 1u);
  ASSERT_EQ(copy.terrain.vegetation_layers[0].id, second_id);
  ASSERT_EQ(memcmp(copy.vegetation_density->samples[0], masks[1], 64u), 0);
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_get(
      &copy, first_id) == NULL);
  LDKTerrain resized = copy.terrain;
  resized.width = 4u;
  resized.height = 4u;
  ASSERT_TRUE(ldk_terrain_component_set(&copy, &resized, NULL));
  ASSERT_TRUE(copy.vegetation_density->samples[0] == NULL);
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_get(
      &source, first_id) != NULL);
  for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
  {
    free(masks[i]);
    free(source.vegetation_density->samples[i]);
    free(copy.vegetation_density->samples[i]);
  }
  free(source.vegetation_density);
  free(copy.vegetation_density);
  tml_document_free(parsed.document);
  free(text);
  return 0;
}

static int test_component_vegetation_invalid_base64(void)
{
  LDKTerrainComponent component = ldk_terrain_component_make_default();
  component.terrain.width = component.terrain.height = 2u;
  u64 layer_id = 0u;
  const u8 painted[] = {0u, 255u, 128u, 32u};
  LDKGrassTypeDesc type = s_vegetation_type("Grass");
  ASSERT_TRUE(ldk_terrain_component_vegetation_layer_add(
      &component, &type, 1.0f, 1u, &layer_id));
  ASSERT_TRUE(ldk_terrain_component_vegetation_density_set(
      &component, layer_id, painted));
  char *text = NULL;
  ASSERT_TRUE(ldk_terrain_to_tml_with_component_data(
      &component.terrain, NULL,
      (const u8 *const *)component.vegetation_density->samples, &text));
  char *sample = strstr(text, "density_samples: \"");
  ASSERT_TRUE(sample != NULL);
  sample += strlen("density_samples: \"");
  *sample = '!';
  TMLParseResult parsed = tml_parse(text);
  ASSERT_TRUE(parsed.ok);
  LDKTerrain output;
  LDKImage *heightmap = NULL;
  u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT] = {0};
  ASSERT_FALSE(ldk_terrain_from_tml_node_with_component_data(
      &output, &heightmap, masks, parsed.document,
      tml_root_node_at(parsed.document, 0u)));
  ASSERT_TRUE(heightmap == NULL);
  for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
  {
    ASSERT_TRUE(masks[i] == NULL);
    free(component.vegetation_density->samples[i]);
  }
  free(component.vegetation_density);
  tml_document_free(parsed.document);
  free(text);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_component_render_settings),
      X_TEST(test_component_metadata_does_not_replace_heightmap),
      X_TEST(test_component_local_heightmap_edits),
      X_TEST(test_component_embedded_scene_roundtrip),
      X_TEST(test_component_legacy_scene_nesting),
      X_TEST(test_component_flat_terrain),
      X_TEST(test_component_vegetation_density_scene_roundtrip),
      X_TEST(test_component_vegetation_invalid_base64),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
