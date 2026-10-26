#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#endif

#include <component/ldk_trail.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>
#include <stdlib.h>

extern bool ldk_trail_update(
    LDKTrailComponent *trail, LDKLineComponent *line, Mat4 world, float dt);

static Mat4 s_translation(float x, float y, float z)
{
  Mat4 result = {{
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      x, y, z, 1.0f,
  }};
  return result;
}

static int test_trail_record_expire(void)
{
  LDKLineComponent line = {0};
  LDKTrailComponent trail = ldk_trail_component_make_default();
  ASSERT_TRUE(ldk_line_max_segments_set(&line, 3u));
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(0, 0, 0), 0.0f));
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  ASSERT_EQ(trail.point_count, 1u);

  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(1, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 1u);
  const LDKLineSegment *segment = ldk_line_segment_get(&line, 0u);
  ASSERT_TRUE(segment != NULL);
  ASSERT_TRUE(fabsf(segment->pos_start.x + 1.0f) < 0.001f);
  ASSERT_TRUE(fabsf(segment->pos_end.x) < 0.001f);

  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(2, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 2u);
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(3, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 3u);
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(4, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 3u);
  ASSERT_EQ(trail.point_count, 4u);

  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(4, 0, 0), 1.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  ASSERT_EQ(trail.point_count, 1u);
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(5, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 1u);
  segment = ldk_line_segment_get(&line, 0u);
  ASSERT_TRUE(fabsf(segment->pos_start.x + 1.0f) < 0.001f);

  ldk_trail_clear(&trail, &line);
  ASSERT_EQ(trail.point_count, 0u);
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  free(trail.points);
  free(line.segments);
  return 0;
}

static int test_trail_distance_disable_and_parameters(void)
{
  LDKTrailComponent trail = ldk_trail_component_make_default();
  LDKLineComponent line = {0};
  ASSERT_TRUE(ldk_line_max_segments_set(&line, 2u));
  trail.min_distance = 2.0f;
  trail.local_offset = (Vec3){0.0f, 1.0f, 0.0f};
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(0, 0, 0), 0.0f));
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(1, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(2, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 1u);
  const LDKLineSegment *seg = ldk_line_segment_get(&line, 0u);
  ASSERT_TRUE(fabsf(seg->pos_start.y - 1.0f) < 0.001f);
  ASSERT_TRUE(fabsf(seg->final_width - 0.1f) < 0.001f);
  ASSERT_TRUE(seg->initial_alpha < seg->final_alpha);
  trail.enabled = false;
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(3, 0, 0), 0.1f));
  ASSERT_EQ(trail.point_count, 0u);
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  trail.enabled = true;
  ASSERT_TRUE(ldk_trail_update(&trail, &line, s_translation(20, 0, 0), 0.1f));
  ASSERT_EQ(ldk_line_segment_count(&line), 0u);
  free(trail.points);
  free(line.segments);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_trail_record_expire),
      X_TEST(test_trail_distance_disable_and_parameters),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
