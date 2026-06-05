#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_MATH
#define X_IMPL_LOG
#define X_IMPL_HPOOL
#endif

#include <ldk_common.h>
#include <ldk.h>

#include <stdx/stdx_log.h>
#include <stdx/stdx_math.h>
#include <component/ldk_transform.h>

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
    X_TEST(test_transform_parent_value_semantics),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
