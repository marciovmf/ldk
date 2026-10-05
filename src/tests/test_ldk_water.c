#include <module/ldk_renderer.h>
#include <system/ldk_water_system.h>
#include <editor/ldk_system_metadata.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>
#include <string.h>

static int test_water_defaults_and_zero_effects(void)
{
  LDKRendererWaterDesc desc;
  ldk_renderer_water_desc_defaults(&desc);
  ASSERT_TRUE(ldk_renderer_water_desc_is_valid(&desc));
  for (u32 i = 0u; i < LDK_RENDERER_WATER_WAVE_COUNT; ++i)
  {
    desc.waves[i].height = 0.0f;
  }
  desc.detail_strength = 0.0f;
  desc.foam_strength = 0.0f;
  desc.shore_range = 0.0f;
  desc.edge_fade_distance = 0.0f;
  ASSERT_TRUE(ldk_renderer_water_desc_is_valid(&desc));
  return 0;
}

static int test_water_rejects_invalid_input(void)
{
  LDKRendererWaterDesc desc;
  ldk_renderer_water_desc_defaults(&desc);
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(NULL));
  desc.waves[0].length = 0.0f;
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(&desc));
  ldk_renderer_water_desc_defaults(&desc);
  desc.water_level = NAN;
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(&desc));
  ldk_renderer_water_desc_defaults(&desc);
  desc.foam_strength = 1.01f;
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(&desc));
  ldk_renderer_water_desc_defaults(&desc);
  desc.waves[1].height = -0.1f;
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(&desc));
  ldk_renderer_water_desc_defaults(&desc);
  desc.detail_scale = INFINITY;
  ASSERT_FALSE(ldk_renderer_water_desc_is_valid(&desc));
  return 0;
}

static int test_water_surface_height_and_rectangle(void)
{
  LDKWaterSystem system;
  ldk_water_system_defaults(&system);
  system.water_level = 2.0f;
  system.width = 8.0f;
  system.depth = 8.0f;
  system.wave_0_height = 0.5f;
  system.wave_0_length = 8.0f;
  system.wave_0_direction_degrees = 0.0f;
  system.wave_1_height = 0.0f;
  system.wave_2_height = 0.0f;
  ASSERT_EQ(ldk_water_system_initialize(&system), 0);
  float height = 0.0f;
  ASSERT_TRUE(ldk_water_height_at_world(0.0f, 0.0f, &height));
  ASSERT_TRUE(fabsf(height - 2.0f) < 1e-5f);
  ASSERT_TRUE(ldk_water_height_at_world(2.0f, 0.0f, &height));
  ASSERT_TRUE(fabsf(height - 2.5f) < 1e-5f);
  ASSERT_TRUE(ldk_water_height_at_world(-2.0f, 0.0f, &height));
  ASSERT_TRUE(fabsf(height - 1.5f) < 1e-5f);
  ASSERT_TRUE(ldk_water_height_at_world(4.0f, 0.0f, &height));
  ASSERT_FALSE(ldk_water_height_at_world(4.01f, 0.0f, &height));
  ASSERT_FALSE(ldk_water_height_at_world(0.0f, -4.01f, &height));
  ASSERT_FALSE(ldk_water_height_at_world(NAN, 0.0f, &height));
  ASSERT_FALSE(ldk_water_height_at_world(0.0f, 0.0f, NULL));
  ldk_water_system_terminate(&system);
  ASSERT_FALSE(ldk_water_system_is_active());
  ASSERT_FALSE(ldk_water_height_at_world(0.0f, 0.0f, &height));
  return 0;
}

static int test_water_direction_and_center(void)
{
  LDKWaterSystem system;
  ldk_water_system_defaults(&system);
  system.center_x = 10.0f;
  system.center_z = -10.0f;
  system.width = 4.0f;
  system.depth = 4.0f;
  system.wave_0_height = 0.5f;
  system.wave_0_length = 8.0f;
  system.wave_0_direction_degrees = 90.0f;
  system.wave_1_height = 0.0f;
  system.wave_2_height = 0.0f;
  ASSERT_EQ(ldk_water_system_initialize(&system), 0);
  float height;
  ASSERT_TRUE(ldk_water_height_at_world(10.0f, -10.0f, &height));
  ASSERT_TRUE(fabsf(height + 0.5f) < 1e-5f);
  ASSERT_FALSE(ldk_water_height_at_world(0.0f, 0.0f, &height));
  ldk_water_system_terminate(&system);
  return 0;
}

static int test_water_ownership_and_quad_size(void)
{
  LDKWaterSystem first;
  LDKWaterSystem second;
  ldk_water_system_defaults(&first);
  ldk_water_system_defaults(&second);
  first.cell_size = 2.0f;
  ASSERT_EQ(ldk_water_system_initialize(&first), 0);
  ASSERT_TRUE(first.cell_size == 2.0f);
  ASSERT_TRUE(ldk_water_system_initialize(&second) != 0);
  ldk_water_system_terminate(&second);
  ASSERT_TRUE(ldk_water_system_is_active());
  ldk_water_system_terminate(&first);
  second.cell_size = -1.0f;
  ASSERT_TRUE(ldk_water_system_initialize(&second) != 0);
  ASSERT_FALSE(ldk_water_system_is_active());
  return 0;
}

static int test_water_metadata_registration(void)
{
  bool found = false;
  for (u32 i = 0u; i < ldk_engine_system_descriptor_count(); ++i)
  {
    LDKSystemDesc desc;
    ASSERT_TRUE(ldk_engine_system_descriptor_get(i, &desc));
    if (strcmp(desc.name, "WaterSystem") != 0)
    {
      continue;
    }
    found = true;
    ASSERT_TRUE(desc.id == UINT64_C(0x00e9968219b6aae4));
    ASSERT_EQ(desc.data_size, sizeof(LDKWaterSystem));
    ASSERT_EQ(desc.bucket, LDK_SYSTEM_BUCKET_RENDER);
    ASSERT_TRUE(desc.flags & LDK_SYSTEM_FLAG_ENGINE_NATIVE);
    ASSERT_TRUE(desc.initialize == ldk_water_system_initialize);
    ASSERT_TRUE(desc.update == ldk_water_system_update);
    ASSERT_TRUE(desc.terminate == ldk_water_system_terminate);
  }
  ASSERT_TRUE(found);

  u32 colors = 0u;
  bool cell_size = false;
  for (u32 i = 0u; i < ldk_engine_system_metadata_count(); ++i)
  {
    const LDKSystemMeta *meta = ldk_engine_system_metadata_get(i);
    if (!meta || strcmp(meta->name, "WaterSystem") != 0)
    {
      continue;
    }
    for (u32 f = 0u; f < meta->field_count; ++f)
    {
      const LDKComponentFieldMeta *field = &meta->fields[f];
      if (strcmp(field->name, "cell_size") == 0)
      {
        cell_size = field->type == LDK_FIELD_FLOAT &&
                    field->offset == offsetof(LDKWaterSystem, cell_size);
      }
      if (field->widget == LDK_FIELD_WIDGET_COLOR)
      {
        ++colors;
      }
    }
  }
  ASSERT_TRUE(cell_size);
  ASSERT_EQ(colors, 3u);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {X_TEST(test_water_defaults_and_zero_effects),
      X_TEST(test_water_rejects_invalid_input),
      X_TEST(test_water_surface_height_and_rectangle),
      X_TEST(test_water_direction_and_center),
      X_TEST(test_water_ownership_and_quad_size),
      X_TEST(test_water_metadata_registration)};
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
