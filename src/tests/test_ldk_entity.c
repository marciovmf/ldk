#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <stdx/stdx_array.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

typedef struct TestComponentA
{
  int value;
} TestComponentA;

typedef struct TestComponentB
{
  int value;
} TestComponentB;

enum
{
  TEST_COMPONENT_A = 1,
  TEST_COMPONENT_B = 2
};

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
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  LDKEntity entity;
  LDKComponentRef ref;
  TestComponentA* component = NULL;

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentA",
      TEST_COMPONENT_A,
      sizeof(TestComponentA),
      8,
      NULL,
      NULL,
      NULL));

  entity = ldk_entity_create(&entity_registry);

  component = (TestComponentA*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A,
      NULL);
  ASSERT_TRUE(component != NULL);

  component->value = 123;

  ASSERT_TRUE(ldk_entity_has_component(
      &entity_registry,
      entity,
      TEST_COMPONENT_A));

  ASSERT_TRUE(ldk_entity_get_component_ref(
      &entity_registry,
      entity,
      TEST_COMPONENT_A,
      &ref));

  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref));
  ASSERT_TRUE(((TestComponentA*)ldk_component_ref_get(
      &entity_registry,
      &component_registry,
      ref))->value == 123);

  ASSERT_TRUE(ldk_entity_remove_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A));

  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref));
  ASSERT_TRUE(!ldk_entity_has_component(
      &entity_registry,
      entity,
      TEST_COMPONENT_A));

  ldk_component_registry_terminate(&component_registry);
  ldk_entity_module_terminate(&entity_registry);
  return 0;
}

int test_entity_component_ref_dereference_and_invalidation(void)
{
  LDKComponentRef ref;
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  LDKEntity entity;
  TestComponentA* stored_component = NULL;

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentA",
      TEST_COMPONENT_A,
      sizeof(TestComponentA),
      8,
      NULL,
      NULL,
      NULL));

  entity = ldk_entity_create(&entity_registry);

  stored_component = (TestComponentA*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A,
      NULL);
  ASSERT_TRUE(stored_component != NULL);
  stored_component->value = 77;

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref));

  stored_component = (TestComponentA*)ldk_component_ref_get(
      &entity_registry,
      &component_registry,
      ref);
  ASSERT_TRUE(stored_component != NULL);
  ASSERT_TRUE(stored_component->value == 77);

  ASSERT_TRUE(ldk_entity_remove_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A));

  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref));
  ASSERT_TRUE(ldk_component_ref_get(&entity_registry, &component_registry, ref) == NULL);

  ldk_component_registry_terminate(&component_registry);
  ldk_entity_module_terminate(&entity_registry);
  return 0;
}

int test_entity_component_ref_mutation(void)
{
  LDKComponentRef ref;
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  LDKEntity entity;
  TestComponentA* stored_component = NULL;
  const TestComponentA* stored_component_const = NULL;
  XArray* store = NULL;

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentA",
      TEST_COMPONENT_A,
      sizeof(TestComponentA),
      8,
      NULL,
      NULL,
      NULL));

  entity = ldk_entity_create(&entity_registry);

  stored_component = (TestComponentA*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A,
      NULL);
  ASSERT_TRUE(stored_component != NULL);
  stored_component->value = 12;

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref));

  stored_component = (TestComponentA*)ldk_component_ref_get(
      &entity_registry,
      &component_registry,
      ref);
  ASSERT_TRUE(stored_component != NULL);

  stored_component->value = 99;

  stored_component_const = (const TestComponentA*)ldk_component_ref_get_const(
      &entity_registry,
      &component_registry,
      ref);
  ASSERT_TRUE(stored_component_const != NULL);
  ASSERT_TRUE(stored_component_const->value == 99);

  store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
  ASSERT_TRUE(store != NULL);
  ASSERT_TRUE(((TestComponentA*)x_array_get(store, 0))->value == 99);

  ldk_component_registry_terminate(&component_registry);
  ldk_entity_module_terminate(&entity_registry);
  return 0;
}

int test_entity_component_ref_invalid_after_entity_destroy(void)
{
  LDKComponentRef ref;
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  LDKEntity entity;
  TestComponentA* stored_component = NULL;

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentA",
      TEST_COMPONENT_A,
      sizeof(TestComponentA),
      8,
      NULL,
      NULL,
      NULL));

  entity = ldk_entity_create(&entity_registry);

  stored_component = (TestComponentA*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A,
      NULL);
  ASSERT_TRUE(stored_component != NULL);
  stored_component->value = 33;

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref));

  ldk_entity_destroy(&entity_registry, entity);

  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref));
  ASSERT_TRUE(ldk_component_ref_get(&entity_registry, &component_registry, ref) == NULL);

  ldk_component_registry_terminate(&component_registry);
  ldk_entity_module_terminate(&entity_registry);
  return 0;
}

int test_entity_component_ref_invalidation_on_remove_add(void)
{
  LDKComponentRef ref_a;
  LDKComponentRef ref_a_new;
  LDKComponentRef ref_b;
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  LDKEntity entity;
  LDKEntityInfo* info = NULL;
  u32 out_slot = 0;
  u32 out_index = 0;
  TestComponentA* component_a_0 = NULL;
  TestComponentB* component_b_0 = NULL;
  TestComponentB* component_b_1 = NULL;

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentA",
      TEST_COMPONENT_A,
      sizeof(TestComponentA),
      8,
      NULL,
      NULL,
      NULL));
  ASSERT_TRUE(ldk_component_register(
      &component_registry,
      "TestComponentB",
      TEST_COMPONENT_B,
      sizeof(TestComponentB),
      8,
      NULL,
      NULL,
      NULL));

  entity = ldk_entity_create(&entity_registry);
  info = ldk_entity_get_info(&entity_registry, entity);

  ASSERT_TRUE(info != NULL);

  component_a_0 = (TestComponentA*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_A,
      NULL);
  ASSERT_TRUE(component_a_0 != NULL);
  component_a_0->value = 10;

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref_a));
  ASSERT_TRUE(info->components.version == 1);

  component_b_0 = (TestComponentB*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_B,
      NULL);
  ASSERT_TRUE(component_b_0 != NULL);
  component_b_0->value = 30;

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_B, &ref_b));
  ASSERT_TRUE(info->components.version == 2);
  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref_a));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref_b));

  ASSERT_TRUE(ldk_entity_remove_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_B));
  ASSERT_TRUE(info->components.version == 3);
  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref_b));

  component_b_1 = (TestComponentB*)ldk_entity_add_component(
      &entity_registry,
      &component_registry,
      entity,
      TEST_COMPONENT_B,
      NULL);
  ASSERT_TRUE(component_b_1 != NULL);
  component_b_1->value = 40;

  ASSERT_TRUE(ldk_entity_find_component(
      &entity_registry,
      entity,
      TEST_COMPONENT_B,
      &out_slot,
      &out_index));
  ASSERT_TRUE(out_index == 0);
  ASSERT_TRUE(info->components.version == 4);

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref_a_new));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref_a_new));
  ASSERT_TRUE(((TestComponentA*)ldk_component_ref_get(
      &entity_registry,
      &component_registry,
      ref_a_new))->value == 10);

  ldk_component_registry_terminate(&component_registry);
  ldk_entity_module_terminate(&entity_registry);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_entity_create_destroy),
    X_TEST(test_entity_flags),
    X_TEST(test_entity_component_refs),
    X_TEST(test_entity_component_ref_dereference_and_invalidation),
    X_TEST(test_entity_component_ref_mutation),
    X_TEST(test_entity_component_ref_invalid_after_entity_destroy),
    X_TEST(test_entity_component_ref_invalidation_on_remove_add),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
