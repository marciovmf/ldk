#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_MATH
#define X_IMPL_HPOOL
#endif
#endif

#include <stdx/stdx_common.h>
#include <stdx/stdx_math.h>
#include <ldk_component_transform.h>
#include <ldk_system_scenegraph.h>

static int s_entity_eq(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

static int test_transform_make_default(void)
{
  LDKTransform transform = ldk_transform_make_default();

  ASSERT_TRUE(float_eq(transform.local_position.x, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_position.y, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_position.z, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_scale.x, 1.0f));
  ASSERT_TRUE(float_eq(transform.local_scale.y, 1.0f));
  ASSERT_TRUE(float_eq(transform.local_scale.z, 1.0f));
  ASSERT_TRUE(float_eq(transform.local_rotation.x, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_rotation.y, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_rotation.z, 0.0f));
  ASSERT_TRUE(float_eq(transform.local_rotation.w, 1.0f));
  ASSERT_TRUE(x_handle_is_null(transform.parent));
  ASSERT_TRUE(x_handle_is_null(transform.first_child));
  ASSERT_TRUE(x_handle_is_null(transform.next_sibling));
  ASSERT_TRUE(x_handle_is_null(transform.prev_sibling));
  ASSERT_TRUE((transform.flags & LDK_TRANSFORM_FLAG_WORLD_DIRTY) != 0);

  return 0;
}

static int test_scenegraph_system_desc_exists(void)
{
  const LDKSystemDesc* desc = ldk_scenegraph_system_desc();

  ASSERT_TRUE(desc != NULL);
  ASSERT_TRUE(desc->id == LDK_SYSTEM_ID_SCENEGRAPH);
  ASSERT_TRUE(desc->callbacks.update != NULL);
  return 0;
}

static int test_transform_parent_value_semantics(void)
{
  LDKTransform parent = ldk_transform_make_default();
  LDKTransform child = ldk_transform_make_default();

  parent.first_child.index = 7;
  parent.first_child.version = 2;
  child.parent = parent.first_child;

  ASSERT_TRUE(s_entity_eq(child.parent, parent.first_child));
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_transform_make_default),
    X_TEST(test_scenegraph_system_desc_exists),
    X_TEST(test_transform_parent_value_semantics),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
