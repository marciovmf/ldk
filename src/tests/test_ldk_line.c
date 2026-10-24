#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#endif

#include <component/ldk_line.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_asset_source.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>
#include <stdlib.h>

/* Exercise the engine-only pre-render synchronization in this engine test. */
extern bool ldk_line_mesh_sync(
    LDKLineComponent *line, LDKAssetManager *assets);

static int test_line_segment_api(void)
{
  LDKLineComponent line = {0};
  LDKLineSegment first = ldk_line_segment_default();
  LDKLineSegment second = ldk_line_segment_default();
  u32 index = UINT32_MAX;

  ASSERT_TRUE(ldk_line_max_segments_set(&line, 2));
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  first.pos_end = ((Vec3){1.0f, 0.0f, 0.0f});
  second.pos_end = ((Vec3){2.0f, 0.0f, 0.0f});
  ASSERT_TRUE(ldk_line_segment_add(&line, &first, &index));
  ASSERT_EQ(index, 0u);
  ASSERT_TRUE(ldk_line_segment_add(&line, &second, &index));
  ASSERT_EQ(index, 1u);
  ASSERT_FALSE(ldk_line_segment_add(&line, &first, NULL));
  ASSERT_FALSE(ldk_line_max_segments_set(&line, 1));

  ASSERT_TRUE(ldk_line_segment_remove(&line, 0));
  ASSERT_EQ(ldk_line_segment_count(&line), 1u);
  ASSERT_EQ(ldk_line_segment_get(&line, 0)->pos_end.x, 2.0f);
  second.final_alpha = 0.5f;
  ASSERT_TRUE(ldk_line_segment_set(&line, 0, &second));
  ASSERT_EQ(ldk_line_segment_get(&line, 0)->final_alpha, 0.5f);
  ASSERT_FALSE(ldk_line_segment_remove(&line, 1));
  ASSERT_FALSE(ldk_line_segment_set(&line, 1, &first));
  second.initial_width = -1.0f;
  ASSERT_FALSE(ldk_line_segment_set(&line, 0, &second));
  second.initial_width = 0.1f;
  second.final_alpha = NAN;
  ASSERT_FALSE(ldk_line_segment_set(&line, 0, &second));

  ldk_line_clear(&line);
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  ASSERT_TRUE(ldk_line_segment_get(&line, 0) == NULL);
  ASSERT_TRUE(ldk_line_max_segments_set(&line, 0));
  ASSERT_FALSE(ldk_line_segment_add(&line, &first, NULL));
  free(line.segments);
  return 0;
}

static int test_line_mesh_lifecycle(void)
{
  LDKAssetSource source = {0};
  LDKAssetManager assets = {0};
  XFSPath cwd = {0};
  LDKLineComponent line = {0};
  LDKLineSegment segment = ldk_line_segment_default();

  ASSERT_TRUE(x_fs_cwd_get(&cwd) != 0);
  ASSERT_TRUE(ldk_asset_source_initialize(&source, cwd.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&assets, &source, 16, 1));
  line.source.source_asset = ldk_asset_mesh_null();
  ASSERT_TRUE(ldk_line_max_segments_set(&line, 2));
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  ASSERT_TRUE(x_handle_is_null(line.source.source_asset.h));

  segment.pos_start = ((Vec3){0.0f, 0.0f, 0.0f});
  segment.pos_end = ((Vec3){2.0f, 0.0f, 0.0f});
  segment.initial_color = 0xff0000ffu;
  segment.final_color = 0x00ff00ffu;
  segment.initial_alpha = 0.5f;
  segment.final_alpha = 0.25f;
  ASSERT_TRUE(ldk_line_segment_add(&line, &segment, NULL));
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  LDKAssetMesh mesh = line.source.source_asset;
  const LDKAssetMeshData *data = ldk_asset_manager_mesh_get_const(&assets, mesh);
  ASSERT_TRUE(data != NULL);
  ASSERT_EQ(data->mesh.vertex_count, 4u);
  ASSERT_EQ(data->mesh.index_count, 12u);
  ASSERT_EQ(data->mesh.vertices[0].color, LDK_RGBA32(0xff000080u));
  ASSERT_EQ(data->mesh.vertices[2].color, LDK_RGBA32(0x00ff0040u));

  ASSERT_TRUE(ldk_line_segment_add(&line, &segment, NULL));
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  ASSERT_EQ(data->mesh.vertex_count, 8u);
  ASSERT_EQ(data->mesh.index_count, 24u);
  ldk_line_clear(&line);
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  ASSERT_TRUE(ldk_line_segment_add(&line, &segment, NULL));
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  ASSERT_EQ(line.source.source_asset.h.index, mesh.h.index);
  ASSERT_EQ(data->mesh.index_count, 12u);

  ASSERT_TRUE(ldk_line_max_segments_set(&line, 4));
  for (u32 i = 0; i < 3; ++i)
  {
    ASSERT_TRUE(ldk_line_segment_add(&line, &segment, NULL));
  }
  ASSERT_TRUE(ldk_line_mesh_sync(&line, &assets));
  ASSERT_EQ(data->mesh.vertex_count, 16u);
  ASSERT_EQ(data->mesh.index_count, 48u);

  free(line.segments);
  ldk_asset_manager_mesh_unload(&assets, line.source.source_asset);
  ldk_asset_manager_terminate(&assets);
  ldk_asset_source_terminate(&source);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_line_segment_api),
      X_TEST(test_line_mesh_lifecycle),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
