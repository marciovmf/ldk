#include <stdx/stdx_common.h>
#include <stdx/stdx_array.h>
#include <ldk_entity.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>


int test_entity_create_destroy(void)
{
  LDKEntityRegistry registry;
  LDKEntity entity;

  ASSERT_TRUE(ldk_entity_module_initialize(&registry, 16, 1));

  entity = ldk_entity_create(&registry);

  ASSERT_TRUE(ldk_entity_is_alive(&registry, entity));
  ASSERT_TRUE(ldk_entity_alive_count(&registry) == 1);

  ldk_entity_destroy(&registry, entity);

  ASSERT_TRUE(!ldk_entity_is_alive(&registry, entity));
  ASSERT_TRUE(ldk_entity_alive_count(&registry) == 0);

  ldk_entity_module_terminate(&registry);
  return 0;
}

int test_entity_flags(void)
{
  LDKEntityRegistry registry;
  LDKEntity entity;

  ASSERT_TRUE(ldk_entity_module_initialize(&registry, 16, 1));

  entity = ldk_entity_create(&registry);

  ldk_entity_set_flags(&registry, entity, 0x01);
  ASSERT_TRUE(ldk_entity_get_flags(&registry, entity) == 0x01);

  ldk_entity_add_flags(&registry, entity, 0x04);
  ASSERT_TRUE(ldk_entity_has_flags(&registry, entity, 0x01));
  ASSERT_TRUE(ldk_entity_has_flags(&registry, entity, 0x04));

  ldk_entity_remove_flags(&registry, entity, 0x01);
  ASSERT_TRUE(!ldk_entity_has_flags(&registry, entity, 0x01));
  ASSERT_TRUE(ldk_entity_has_flags(&registry, entity, 0x04));

  ldk_entity_module_terminate(&registry);
  return 0;
}

int test_entity_component_refs(void)
{
  LDKEntityRegistry registry;
  LDKEntity entity;
  u32 out_slot = 0;
  u32 out_index = 0;

  ASSERT_TRUE(ldk_entity_module_initialize(&registry, 16, 1));

  entity = ldk_entity_create(&registry);

  ASSERT_TRUE(ldk_entity_add_component_ref(&registry, entity, 10, 3));
  ASSERT_TRUE(ldk_entity_has_component(&registry, entity, 10));
  ASSERT_TRUE(ldk_entity_find_component(&registry, entity, 10, &out_slot, &out_index));
  ASSERT_TRUE(out_index == 3);

  ASSERT_TRUE(ldk_entity_update_component_ref(&registry, entity, 10, 8));
  ASSERT_TRUE(ldk_entity_find_component(&registry, entity, 10, &out_slot, &out_index));
  ASSERT_TRUE(out_index == 8);

  ASSERT_TRUE(ldk_entity_remove_component_ref(&registry, entity, 10));
  ASSERT_TRUE(!ldk_entity_has_component(&registry, entity, 10));

  ldk_entity_module_terminate(&registry);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_entity_create_destroy),
    X_TEST(test_entity_flags),
    X_TEST(test_entity_component_refs),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
