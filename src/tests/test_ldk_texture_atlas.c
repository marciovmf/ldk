#if defined(LDK_SHAREDLIB)
#define X_IMPL_STRBUILDER
#define X_IMPL_TML
#define X_IMPL_HPOOL
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#endif

#include <ldk_texture_atlas.h>
#include <module/ldk_asset_manager.h>
#define X_IMPL_TEST
#include <stdx/stdx_test.h>
#include <stdlib.h>
#include <string.h>

static int test_atlas_empty(void)
{
  LDKTextureAtlas atlas, loaded;
  char *text = NULL;
  ldk_texture_atlas_init(&atlas);
  ldk_texture_atlas_init(&loaded);
  ASSERT_TRUE(ldk_texture_atlas_validate(&atlas));
  ASSERT_TRUE(ldk_texture_atlas_to_tml(&atlas, &text));
  ASSERT_TRUE(ldk_texture_atlas_from_tml(&loaded, text));
  ASSERT_EQ(loaded.region_count, 0u);
  ASSERT_EQ(loaded.next_region_id, 1u);
  free(text);
  ldk_texture_atlas_clear(&atlas);
  ldk_texture_atlas_clear(&loaded);
  return 0;
}

static int test_atlas_grid_and_uv(void)
{
  LDKTextureAtlas atlas;
  ldk_texture_atlas_init(&atlas);
  ASSERT_TRUE(ldk_texture_atlas_texture_set(&atlas, "sprites/walk.png"));
  ASSERT_TRUE(ldk_texture_atlas_grid_append(&atlas, 110, 70,
      32, 32, 2, 2, 4, 2));
  ASSERT_EQ(atlas.region_count, 6u);
  ASSERT_EQ(atlas.regions[0].x, 4u);
  ASSERT_EQ(atlas.regions[0].y, 2u);
  ASSERT_EQ(atlas.regions[1].x, 38u);
  ASSERT_EQ(atlas.regions[3].y, 36u);
  float uv[4];
  ASSERT_TRUE(ldk_texture_atlas_region_uv(&atlas.regions[0], 110, 70, uv));
  ASSERT_TRUE(uv[0] > 0.0f && uv[0] < uv[2]);
  ASSERT_FALSE(ldk_texture_atlas_region_uv(&atlas.regions[0], 1, 1, uv));
  ASSERT_FALSE(ldk_texture_atlas_grid_append(&atlas, 10, 10, 0, 1, 0, 0, 0, 0));
  ldk_texture_atlas_clear(&atlas);
  return 0;
}

static int test_atlas_roundtrip_identity(void)
{
  LDKTextureAtlas atlas, parsed;
  char *text = NULL;
  u64 id = 0;
  ldk_texture_atlas_init(&atlas);
  ldk_texture_atlas_init(&parsed);
  ASSERT_TRUE(ldk_texture_atlas_texture_set(&atlas, "sprites/a.png"));
  ASSERT_TRUE(ldk_texture_atlas_region_add(&atlas, "one", 0, 0, 16, 16, &id));
  ASSERT_TRUE(ldk_texture_atlas_region_rename(&atlas, id, "idle"));
  ASSERT_FALSE(ldk_texture_atlas_region_add(&atlas, "idle", 16, 0, 16, 16, NULL));
  atlas.regions[0].pivot_x = 0.25f;
  ASSERT_TRUE(ldk_texture_atlas_to_tml(&atlas, &text));
  ASSERT_TRUE(ldk_texture_atlas_from_tml(&parsed, text));
  ASSERT_EQ(parsed.region_count, 1u);
  ASSERT_EQ(parsed.regions[0].id, id);
  ASSERT_EQ(parsed.regions[0].pivot_x, 0.25f);
  ASSERT_TRUE(strcmp(parsed.regions[0].name, "idle") == 0);
  ASSERT_TRUE(ldk_texture_atlas_region_remove(&parsed, id));
  ASSERT_TRUE(ldk_texture_atlas_region_add(&parsed, "second", 0, 0, 1, 1, NULL));
  ASSERT_TRUE(parsed.regions[0].id != id);
  free(text);
  ldk_texture_atlas_clear(&atlas);
  ldk_texture_atlas_clear(&parsed);
  return 0;
}

static int test_atlas_parse_failure_preserves_data(void)
{
  LDKTextureAtlas atlas;
  ldk_texture_atlas_init(&atlas);
  ASSERT_TRUE(ldk_texture_atlas_region_add(&atlas, "keep", 0, 0, 8, 8, NULL));
  ASSERT_FALSE(ldk_texture_atlas_from_tml(&atlas,
      "texture_atlas:\n  version: 3\n  texture: \"x.png\"\n  regions:\n"));
  ASSERT_EQ(atlas.region_count, 1u);
  ASSERT_TRUE(strcmp(atlas.regions[0].name, "keep") == 0);
  ldk_texture_atlas_clear(&atlas);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_atlas_empty),
      X_TEST(test_atlas_grid_and_uv),
      X_TEST(test_atlas_roundtrip_identity),
      X_TEST(test_atlas_parse_failure_preserves_data),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
