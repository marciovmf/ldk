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

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_component_embedded_scene_roundtrip),
      X_TEST(test_component_legacy_scene_nesting),
      X_TEST(test_component_flat_terrain),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
