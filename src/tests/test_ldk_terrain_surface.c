#include <ldk_terrain.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>

static bool s_close(float a, float b)
{
  return fabsf(a - b) < 0.001f;
}

static int test_terrain_surface_flat(void)
{
  LDKTerrain terrain;
  float height = 0.0f;
  Vec3 normal;
  ldk_terrain_init(&terrain);
  terrain.height_offset = 3.0f;
  ASSERT_TRUE(ldk_terrain_surface_sample(
      &terrain, NULL, 0.0f, 0.0f, &height, &normal));
  ASSERT_TRUE(s_close(height, 3.0f));
  ASSERT_TRUE(s_close(normal.x, 0.0f));
  ASSERT_TRUE(s_close(normal.y, 1.0f));
  ASSERT_TRUE(s_close(normal.z, 0.0f));
  ASSERT_FALSE(ldk_terrain_surface_sample(
      &terrain, NULL, 40.0f, 0.0f, &height, NULL));
  return 0;
}

static int test_terrain_surface_rgba8_rgba16(void)
{
  LDKTerrain terrain;
  float height;
  Vec3 normal;
  u8 pixels[] = {
      0, 0, 0, 255, 255, 0, 0, 255,
      255, 0, 0, 255, 0, 0, 0, 255};
  u16 pixels16[] = {
      0, 0, 0, 65535, 65535, 0, 0, 65535,
      65535, 0, 0, 65535, 0, 0, 0, 65535};
  LDKImageInfo info = {0};
  ldk_terrain_init(&terrain);
  terrain.width = terrain.height = 2u;
  terrain.cell_size = 2.0f;
  terrain.height_scale = 10.0f;
  terrain.height_offset = -3.0f;
  terrain.center_on_origin = false;
  info.width = info.height = 2u;
  info.channel_count = 4u;
  info.bytes_per_channel = 1u;
  info.format = LDK_IMAGE_FORMAT_RGBA8_UNORM;
  info.pixels = pixels;
  ASSERT_TRUE(ldk_terrain_surface_sample(
      &terrain, &info, 0.0f, 0.0f, &height, NULL));
  ASSERT_TRUE(s_close(height, -3.0f));
  ASSERT_TRUE(ldk_terrain_surface_sample(
      &terrain, &info, 2.0f, 0.0f, &height, &normal));
  ASSERT_TRUE(s_close(height, 2.0f));
  ASSERT_TRUE(normal.y > 0.0f && normal.y <= 1.0f);
  ASSERT_TRUE(ldk_terrain_surface_sample(
      &terrain, &info, 2.0f, 2.0f, &height, &normal));
  ASSERT_TRUE(s_close(height, 2.0f));
  ASSERT_FALSE(ldk_terrain_surface_sample(
      &terrain, &info, 4.001f, 2.0f, &height, NULL));

  info.pixels = (const u8 *)pixels16;
  info.format = LDK_IMAGE_FORMAT_RGBA16_UNORM;
  info.bytes_per_channel = 2u;
  ASSERT_TRUE(ldk_terrain_surface_sample(
      &terrain, &info, 2.0f, 0.0f, &height, NULL));
  ASSERT_TRUE(s_close(height, 2.0f));

  /* An unrelated image size must not silently produce incorrect placement. */
  info.width = 3u;
  ASSERT_FALSE(ldk_terrain_surface_sample(
      &terrain, &info, 2.0f, 0.0f, &height, NULL));
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_terrain_surface_flat),
      X_TEST(test_terrain_surface_rgba8_rgba16),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
