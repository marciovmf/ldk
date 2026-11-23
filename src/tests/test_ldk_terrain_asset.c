#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_HPOOL
#define X_IMPL_STRING
#define X_IMPL_STRBUILDER
#define X_IMPL_TML
#endif

#include <module/ldk_asset_manager.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int test_terrain_defaults_and_roundtrip(void)
{
  LDKTerrain terrain, restored;
  char *text = NULL;
  u64 layer_id = 0;
  ldk_terrain_init(&terrain);
  ldk_terrain_init(&restored);
  ASSERT_TRUE(ldk_terrain_validate(&terrain));
  ASSERT_EQ(terrain.surface_count, 1u);
  ASSERT_EQ(terrain.heightmap_path.length, 0u);
  ASSERT_TRUE(ldk_terrain_vegetation_layer_add(&terrain,
      "Tall Grass", "terrain/grass.png", 0.7f, 123u, &layer_id));
  ASSERT_EQ(layer_id, 1u);
  terrain.surfaces[0].color = 0x885522ffu;
  terrain.surfaces[0].sprite_id = 23u;
  ASSERT_TRUE(ldk_asset_path_set(
      &terrain.surfaces[0].paint_mask_path, "terrain/mud-mask.png"));
  ASSERT_TRUE(ldk_terrain_to_tml(&terrain, &text));
  ASSERT_TRUE(text != NULL);
  ASSERT_TRUE(ldk_terrain_from_tml(&restored, text));
  ASSERT_EQ(restored.surface_count, 1u);
  ASSERT_EQ(restored.surfaces[0].sprite_id, 23u);
  ASSERT_EQ(restored.surfaces[0].color, 0x885522ffu);
  ASSERT_TRUE(strcmp(restored.surfaces[0].paint_mask_path.buf,
      "terrain/mud-mask.png") == 0);
  ASSERT_EQ(restored.vegetation_layer_count, 1u);
  ASSERT_EQ(restored.vegetation_layers[0].id, layer_id);
  ASSERT_EQ(restored.vegetation_layers[0].seed, 123u);
  ASSERT_TRUE(strcmp(restored.vegetation_layers[0].type_name,
      "Tall Grass") == 0);
  ASSERT_TRUE(strcmp(restored.vegetation_layers[0].density_map_path.buf,
      "terrain/grass.png") == 0);
  ASSERT_TRUE(restored.vegetation_layers[0].density_scale > 0.69f &&
      restored.vegetation_layers[0].density_scale < 0.71f);
  ASSERT_TRUE(ldk_terrain_vegetation_layer_remove(&restored, layer_id));
  ASSERT_TRUE(ldk_terrain_vegetation_layer_add(&restored,
      "Flowers", "", 1.0f, 11u, &layer_id));
  ASSERT_EQ(layer_id, 2u);
  free(text);
  return 0;
}

static int test_terrain_import_heightmap(void)
{
  u16 pixels[3u * 2u * 4u] = {0};
  LDKImage *image = ldk_image_create_format(3u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  LDKTerrain terrain;
  ASSERT_TRUE(image != NULL);
  ldk_terrain_init(&terrain);
  ASSERT_TRUE(ldk_terrain_import_heightmap(&terrain,
      "terrain/source.png", image, LDK_TERRAIN_CHANNEL_G));
  ASSERT_EQ(terrain.width, 3u);
  ASSERT_EQ(terrain.height, 2u);
  ASSERT_EQ(terrain.height_channel, LDK_TERRAIN_CHANNEL_G);
  ASSERT_TRUE(strcmp(terrain.heightmap_path.buf, "terrain/source.png") == 0);
  ASSERT_FALSE(ldk_terrain_import_heightmap(&terrain,
      "../outside.png", image, LDK_TERRAIN_CHANNEL_R));
  ASSERT_EQ(terrain.height_channel, LDK_TERRAIN_CHANNEL_G);
  ASSERT_FALSE(ldk_terrain_import_heightmap(&terrain,
      "terrain/source.png", image, (LDKTerrainChannel)4));
  ldk_image_destroy(image);
  return 0;
}

static int test_heightmap_resample_explicit_grid(void)
{
  /* Four samples: shrinking to 1x1 must average, not select a corner. */
  const u8 pixels[2u * 2u * 4u] = {
      0, 20, 0, 255,   255, 20, 0, 255,
      255, 20, 0, 255, 0, 20, 0, 255};
  LDKImage *source = ldk_image_create(2u, 2u, pixels);
  LDKImageInfo info = {0};
  ASSERT_TRUE(source != NULL);
  LDKImage *resampled = ldk_terrain_heightmap_resample(source, 1u, 1u);
  ASSERT_TRUE(resampled != NULL);
  ASSERT_TRUE(ldk_image_get_info(resampled, &info));
  ASSERT_EQ(info.width, 1u);
  ASSERT_EQ(info.height, 1u);
  ASSERT_EQ(info.format, LDK_IMAGE_FORMAT_RGBA16_UNORM);
  const u16 *values = (const u16 *)info.pixels;
  ASSERT_TRUE(values[0] >= 32767u && values[0] <= 32768u);
  ASSERT_EQ(values[1], 20u * 257u);
  ASSERT_EQ(values[3], 65535u);
  ASSERT_FALSE(ldk_terrain_heightmap_resample(source, 0u, 1u));
  ASSERT_FALSE(ldk_terrain_heightmap_resample(source, 1u, 0u));
  ldk_image_destroy(resampled);

  /* 16-bit source values must survive a same-resolution import exactly. */
  const u16 hires[8] = {
      12345u, 100u, 65535u, 17u, 54321u, 1000u, 32000u, 65535u};
  LDKImage *source16 = ldk_image_create_format(2u, 1u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, hires);
  ASSERT_TRUE(source16 != NULL);
  resampled = ldk_terrain_heightmap_resample(source16, 2u, 1u);
  ASSERT_TRUE(resampled != NULL);
  ASSERT_TRUE(ldk_image_get_info(resampled, &info));
  ASSERT_EQ(memcmp(info.pixels, hires, sizeof(hires)), 0);
  ldk_image_destroy(source16);
  ldk_image_destroy(resampled);
  ldk_image_destroy(source);
  return 0;
}

static float s_sample_world_height(const LDKTerrain *terrain, u16 sample)
{
  return terrain->height_offset +
      ((float)sample / 65535.0f) * terrain->height_scale;
}

static int test_terrain_heightmap_range_expansion(void)
{
  LDKTerrain terrain;
  ldk_terrain_init(&terrain);
  terrain.width = 3u;
  terrain.height = 1u;
  terrain.height_scale = 1.0f; /* Existing .terrain files can use this. */
  terrain.surface_count = 2u;
  terrain.surfaces[1].min_height = 0.6f;
  terrain.surfaces[1].blend_width = 0.2f;
  const u16 marker = 1234u;
  u16 pixels[3u * 4u] = {
      0u, marker, 0u, 0u, 32768u, marker, 0u, 0u,
      65535u, marker, 0u, 0u};
  u16 before[3u * 4u];
  memcpy(before, pixels, sizeof(before));
  float original[3];
  for (u32 i = 0u; i < 3u; ++i)
    original[i] = s_sample_world_height(&terrain, pixels[4u * i]);
  const float original_surface_height = terrain.height_offset +
      terrain.height_scale * terrain.surfaces[1].min_height;

  /* Painting above the legacy 1-unit ceiling must extend the range. */
  ASSERT_TRUE(ldk_terrain_heightmap_range_expand(
      &terrain, pixels, 0.0f, 1.2f));
  ASSERT_TRUE(terrain.height_scale >= 1.2f);
  /* A subsequent Raise can now encode a height greater than one unit. */
  const u16 raised = (u16)lroundf(
      ((1.1f - terrain.height_offset) / terrain.height_scale) * 65535.0f);
  ASSERT_TRUE(s_sample_world_height(&terrain, raised) > 1.0f);
  for (u32 i = 0u; i < 3u; ++i)
  {
    float height = s_sample_world_height(&terrain, pixels[4u * i]);
    ASSERT_TRUE(fabsf(height - original[i]) < 0.0001f);
    ASSERT_EQ(pixels[4u * i + 1u], marker);
  }
  ASSERT_TRUE(fabsf(original_surface_height - (terrain.height_offset +
      terrain.height_scale * terrain.surfaces[1].min_height)) < 0.0001f);

  /* Lower must also work below the original zero plane, without moving the
   * existing hills or introducing a new surface threshold. */
  ASSERT_TRUE(ldk_terrain_heightmap_range_expand(
      &terrain, pixels, -0.3f, 1.2f));
  ASSERT_TRUE(terrain.height_offset < 0.0f);
  ASSERT_TRUE(terrain.height_offset + terrain.height_scale >= 1.2f);
  for (u32 i = 0u; i < 3u; ++i)
  {
    float height = s_sample_world_height(&terrain, pixels[4u * i]);
    ASSERT_TRUE(fabsf(height - original[i]) < 0.0001f);
    ASSERT_EQ(pixels[4u * i + 1u], marker);
  }
  ASSERT_TRUE(fabsf(original_surface_height - (terrain.height_offset +
      terrain.height_scale * terrain.surfaces[1].min_height)) < 0.0001f);

  LDKTerrain snapshot = terrain;
  memcpy(before, pixels, sizeof(before));
  ASSERT_TRUE(ldk_terrain_heightmap_range_expand(&terrain, pixels,
      terrain.height_offset, terrain.height_offset + terrain.height_scale));
  ASSERT_EQ(memcmp(&terrain, &snapshot, sizeof(terrain)), 0);
  ASSERT_EQ(memcmp(pixels, before, sizeof(pixels)), 0);
  ASSERT_FALSE(ldk_terrain_heightmap_range_expand(
      &terrain, pixels, NAN, 3.0f));
  ASSERT_EQ(memcmp(&terrain, &snapshot, sizeof(terrain)), 0);
  ASSERT_EQ(memcmp(pixels, before, sizeof(pixels)), 0);
  return 0;
}

static int test_terrain_invalid_parse_preserves_original(void)
{
  LDKTerrain terrain;
  ldk_terrain_init(&terrain);
  terrain.height_scale = 4.0f;
  ASSERT_FALSE(ldk_terrain_from_tml(&terrain,
      "terrain:\n  version: 3\n"));
  ASSERT_EQ(terrain.height_scale, 4.0f);
  terrain.surface_count = LDK_TERRAIN_SURFACE_COUNT + 1u;
  ASSERT_FALSE(ldk_terrain_validate(&terrain));
  return 0;
}

static int test_terrain_manager_shared_reload(void)
{
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  LDKTerrain terrain;
  XFSPath cwd = {0};
  XFSPath path = {0};
  char name[96];
  FILE *file;
  ldk_terrain_init(&terrain);
  ASSERT_TRUE(x_fs_cwd_get(&cwd) != 0);
  snprintf(name, sizeof(name), "ldk-terrain-%llu-%lu.terrain",
      (unsigned long long)time(NULL), (unsigned long)clock());
  ASSERT_TRUE(x_fs_path(&path, cwd.buf, name));
  ASSERT_TRUE(ldk_terrain_save(&terrain, path.buf));
  ASSERT_TRUE(ldk_asset_source_initialize(&source, cwd.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16u, 1u));
  LDKAssetTerrain first = ldk_asset_manager_terrain_load_shared(&manager, name);
  LDKAssetTerrain second =
      ldk_asset_manager_terrain_load_shared(&manager, name);
  ASSERT_TRUE(ldk_asset_manager_terrain_is_alive(&manager, first));
  ASSERT_EQ(first.h.index, second.h.index);
  ASSERT_EQ(first.h.version, second.h.version);
  const LDKAssetTerrainData *data =
      ldk_asset_manager_terrain_get_const(&manager, first);
  ASSERT_TRUE(data != NULL);
  ASSERT_EQ(data->revision, 1u);
  ASSERT_EQ(data->terrain.height_scale, 1.0f);
  ASSERT_TRUE(x_handle_is_null(data->heightmap.h));
  terrain.height_scale = 7.5f;
  ASSERT_TRUE(ldk_terrain_save(&terrain, path.buf));
  ASSERT_TRUE(ldk_asset_manager_terrain_reload(&manager, name));
  data = ldk_asset_manager_terrain_get_const(&manager, first);
  ASSERT_EQ(data->revision, 2u);
  ASSERT_EQ(data->terrain.height_scale, 7.5f);
  terrain.height_offset = -3.25f;
  ASSERT_TRUE(ldk_asset_manager_terrain_update(&manager, first, &terrain));
  data = ldk_asset_manager_terrain_get_const(&manager, first);
  ASSERT_EQ(data->revision, 3u);
  ASSERT_EQ(data->terrain.height_offset, -3.25f);
  file = fopen(path.buf, "wb");
  ASSERT_TRUE(file != NULL);
  ASSERT_EQ(fwrite("invalid", 1, 7, file), 7u);
  ASSERT_EQ(fclose(file), 0);
  ASSERT_FALSE(ldk_asset_manager_terrain_reload(&manager, name));
  data = ldk_asset_manager_terrain_get_const(&manager, first);
  ASSERT_EQ(data->revision, 3u);
  ASSERT_EQ(data->terrain.height_scale, 7.5f);
  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  ASSERT_EQ(remove(path.buf), 0);
  return 0;
}

static int test_terrain_preview_lifecycle(void)
{
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  LDKTerrain terrain;
  XFSPath cwd = {0};
  u16 samples16[4u * 4u * 4u] = {0};
  u8 samples8[4u * 4u * 4u] = {0};
  LDKImageInfo info = {0};
  ASSERT_TRUE(x_fs_cwd_get(&cwd) != 0);
  ASSERT_TRUE(ldk_asset_source_initialize(&source, cwd.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16u, 1u));
  ldk_terrain_init(&terrain);
  terrain.width = 4u;
  terrain.height = 4u;
  LDKAssetTerrain asset = ldk_asset_manager_terrain_create(&manager, &terrain);
  ASSERT_TRUE(ldk_asset_manager_terrain_is_alive(&manager, asset));
  samples16[0] = 5678u;
  LDKImage *original = ldk_image_create_format(4u, 4u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, samples16);
  ASSERT_TRUE(original != NULL);
  LDKImageInfo image_info = {0};
  ASSERT_TRUE(ldk_image_get_info(original, &image_info));
  ASSERT_TRUE(ldk_asset_manager_terrain_preview_set(&manager, asset, &image_info));
  ldk_image_destroy(original);
  const LDKAssetTerrainData *data =
      ldk_asset_manager_terrain_get_const(&manager, asset);
  ASSERT_TRUE(data->heightmap_preview_enabled);
  ASSERT_TRUE(ldk_image_get_info(data->heightmap_preview, &info));
  ASSERT_EQ(info.format, LDK_IMAGE_FORMAT_RGBA16_UNORM);
  ASSERT_EQ(((const u16 *)info.pixels)[0], 5678u);
  const u8 *original_pixels = info.pixels;
  samples16[0] = 2314u;
  LDKImage *replacement = ldk_image_create_format(4u, 4u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, samples16);
  ASSERT_TRUE(replacement != NULL);
  ASSERT_TRUE(ldk_image_get_info(replacement, &image_info));
  ASSERT_TRUE(ldk_asset_manager_terrain_preview_set(&manager, asset, &image_info));
  ldk_image_destroy(replacement);
  ASSERT_TRUE(ldk_image_get_info(data->heightmap_preview, &info));
  ASSERT_TRUE(info.pixels == original_pixels); /* Stable storage per stroke. */
  ASSERT_EQ(((const u16 *)info.pixels)[0], 2314u);
  samples8[0] = 90u;
  replacement = ldk_image_create(4u, 4u, samples8);
  ASSERT_TRUE(replacement != NULL);
  ASSERT_TRUE(ldk_image_get_info(replacement, &image_info));
  ASSERT_TRUE(ldk_asset_manager_terrain_preview_set(&manager, asset, &image_info));
  ldk_image_destroy(replacement);
  ASSERT_EQ(data->heightmap_preview_retired_count, 1u);
  ASSERT_TRUE(ldk_image_get_info(data->heightmap_preview, &info));
  ASSERT_EQ(info.format, LDK_IMAGE_FORMAT_RGBA8_UNORM);
  ASSERT_EQ(info.pixels[0], 90u);
  ASSERT_EQ(((const u16 *)original_pixels)[0], 2314u);
  ASSERT_TRUE(ldk_asset_manager_terrain_preview_clear(&manager, asset));
  ASSERT_FALSE(data->heightmap_preview_enabled);
  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  return 0;
}

static int test_terrain_heightmap_embedded_roundtrip(void)
{
  const u32 widths[] = {1u, 2u, 3u, 64u};
  for (u32 test = 0u; test < 4u; ++test)
  {
    LDKTerrain terrain, read;
    ldk_terrain_init(&terrain);
    terrain.width = widths[test];
    terrain.height = test == 3u ? 64u : 1u;
    terrain.height_scale = 48.0f;
    const size_t count = (size_t)terrain.width * terrain.height;
    u16 *pixels = calloc(count * 4u, sizeof(u16));
    ASSERT_TRUE(pixels != NULL);
    for (size_t i = 0; i < count; ++i)
    {
      pixels[i * 4u] = (u16)((i * 27017u + 19u) & 65535u);
      pixels[i * 4u + 3u] = 65535u;
    }
    LDKImage *heightmap = ldk_image_create_format(terrain.width,
        terrain.height, LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
    ASSERT_TRUE(heightmap != NULL);
    LDKImageInfo source = {0}, decoded = {0};
    ASSERT_TRUE(ldk_image_get_info(heightmap, &source));
    char *text = NULL;
    ASSERT_TRUE(ldk_terrain_to_tml_with_heightmap(&terrain, &source, &text));
    LDKImage *read_image = NULL;
    ldk_terrain_init(&read);
    ASSERT_TRUE(ldk_terrain_from_tml_with_heightmap(
        &read, &read_image, text));
    ASSERT_TRUE(read_image != NULL);
    ASSERT_TRUE(ldk_image_get_info(read_image, &decoded));
    ASSERT_EQ(decoded.format, LDK_IMAGE_FORMAT_RGBA16_UNORM);
    ASSERT_EQ(read.height_scale, 48.0f);
    for (size_t i = 0u; i < count; ++i)
    {
      u16 value;
      memcpy(&value, decoded.pixels + i * 8u, sizeof(value));
      ASSERT_EQ(value, pixels[i * 4u]);
    }
    /* Invalid base64 must fail without destroying the prior image/terrain. */
    char *samples = strstr(text, "heightmap_samples: \"");
    ASSERT_TRUE(samples != NULL);
    samples = strchr(samples, '"') + 1;
    samples[0] = '!';
    LDKTerrain preserved = read;
    LDKImage *preserved_image = read_image;
    ASSERT_FALSE(ldk_terrain_from_tml_with_heightmap(&read,
        &read_image, text));
    ASSERT_EQ(memcmp(&read, &preserved, sizeof(read)), 0);
    ASSERT_TRUE(read_image == preserved_image);
    free(text);
    ldk_image_destroy(read_image);
    ldk_image_destroy(heightmap);
    free(pixels);
  }
  return 0;
}

static int test_terrain_manager_embedded_reload(void)
{
  LDKTerrain terrain;
  ldk_terrain_init(&terrain);
  terrain.width = 2u;
  terrain.height = 1u;
  u16 pixels[8] = { 4000u, 0u, 0u, 65535u,
                    64000u, 0u, 0u, 65535u };
  LDKImage *heightmap = ldk_image_create_format(terrain.width,
      terrain.height, LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  LDKImageInfo image = {0};
  ASSERT_TRUE(heightmap != NULL);
  ASSERT_TRUE(ldk_image_get_info(heightmap, &image));
  XFSPath cwd = {0}, path = {0};
  ASSERT_TRUE(x_fs_cwd_get(&cwd) != 0);
  char name[96];
  snprintf(name, sizeof(name), "ldk-embedded-%llu-%lu.terrain",
      (unsigned long long)time(NULL), (unsigned long)clock());
  ASSERT_TRUE(x_fs_path(&path, cwd.buf, name));
  ASSERT_TRUE(ldk_terrain_save_with_heightmap(&terrain, &image, path.buf));
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  ASSERT_TRUE(ldk_asset_source_initialize(&source, cwd.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16u, 1u));
  LDKAssetTerrain handle = ldk_asset_manager_terrain_load_shared(
      &manager, name);
  ASSERT_TRUE(ldk_asset_manager_terrain_is_alive(&manager, handle));
  const LDKAssetTerrainData *data =
      ldk_asset_manager_terrain_get_const(&manager, handle);
  ASSERT_TRUE(data != NULL);
  ASSERT_TRUE(data->heightmap_embedded != NULL);
  ASSERT_TRUE(x_handle_is_null(data->heightmap.h));
  LDKImageInfo restored = {0};
  ASSERT_TRUE(ldk_image_get_info(data->heightmap_embedded, &restored));
  const u8 *retained = restored.pixels;
  ASSERT_EQ(((const u16*)retained)[0], 4000u);
  pixels[0] = 6000u;
  ldk_image_destroy(heightmap);
  heightmap = ldk_image_create_format(terrain.width, terrain.height,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  ASSERT_TRUE(heightmap != NULL);
  ASSERT_TRUE(ldk_image_get_info(heightmap, &image));
  ASSERT_TRUE(ldk_terrain_save_with_heightmap(&terrain, &image, path.buf));
  ASSERT_TRUE(ldk_asset_manager_terrain_reload(&manager, name));
  data = ldk_asset_manager_terrain_get_const(&manager, handle);
  ASSERT_TRUE(ldk_image_get_info(data->heightmap_embedded, &restored));
  ASSERT_EQ(((const u16*)restored.pixels)[0], 6000u);
  ASSERT_EQ(((const u16*)retained)[0], 4000u);
  ASSERT_EQ(data->revision, 2u);
  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  ldk_image_destroy(heightmap);
  ASSERT_EQ(remove(path.buf), 0);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_terrain_defaults_and_roundtrip),
      X_TEST(test_terrain_import_heightmap),
      X_TEST(test_heightmap_resample_explicit_grid),
      X_TEST(test_terrain_heightmap_range_expansion),
      X_TEST(test_terrain_invalid_parse_preserves_original),
      X_TEST(test_terrain_manager_shared_reload),
      X_TEST(test_terrain_preview_lifecycle),
      X_TEST(test_terrain_heightmap_embedded_roundtrip),
      X_TEST(test_terrain_manager_embedded_reload),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
