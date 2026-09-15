#if defined(LDK_SHAREDLIB)
#define X_IMPL_MATH
#endif

#include <ldk_raycast.h>
#include <stdx/stdx_math.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

static int test_triangle_hit(void)
{
  LDKRay ray;
  LDKRaycastHit hit;

  ASSERT_TRUE(ldk_ray_make(
      vec3_make(0.0f, 0.0f, 1.0f),
      vec3_make(0.0f, 0.0f, -1.0f), &ray));

  ASSERT_TRUE(ldk_raycast_triangle(ray,
      vec3_make(-1.0f, -1.0f, 0.0f),
      vec3_make(1.0f, -1.0f, 0.0f),
      vec3_make(0.0f, 1.0f, 0.0f), &hit));

  ASSERT_TRUE(float_eq(hit.distance, 1.0f));
  return 0;
}

static int test_transformed_triangle_hit(void)
{
  LDKMeshVertex vertices[3] = {0};
  u32 indices[3] = {0, 1, 2};
  LDKMeshData mesh = {0};
  LDKRay ray;
  LDKRaycastHit hit;

  vertices[0].position = vec3_make(-1.0f, -1.0f, 0.0f);
  vertices[1].position = vec3_make(1.0f, -1.0f, 0.0f);
  vertices[2].position = vec3_make(0.0f, 1.0f, 0.0f);

  mesh.vertices = vertices;
  mesh.vertex_count = 3;
  mesh.indices = indices;
  mesh.index_count = 3;

  ASSERT_TRUE(ldk_ray_make(
      vec3_make(0.0f, 0.0f, 10.0f),
      vec3_make(0.0f, 0.0f, -1.0f), &ray));

  ASSERT_TRUE(ldk_raycast_mesh_transformed(ray, &mesh,
      mat4_translate(vec3_make(0.0f, 0.0f, 4.0f)), &hit));

  ASSERT_TRUE(float_eq(hit.distance, 6.0f));
  ASSERT_TRUE(hit.triangle_index == 0u);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_triangle_hit),
    X_TEST(test_transformed_triangle_hit),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
