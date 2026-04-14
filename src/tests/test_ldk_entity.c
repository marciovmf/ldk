#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#include <stdx/stdx_common.h>
#include <stdx/stdx_array.h>
#include <ldk_entity.h>
#include <ldk_component.h>

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

  ASSERT_TRUE(ldk_entity_module_initialize(&registry, 16, 1));

  LDKEntity entity = ldk_entity_create(&registry);

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
  u32 out_slot = 0;
  u32 out_index = 0;

  ASSERT_TRUE(ldk_entity_module_initialize(&registry, 16, 1));

  LDKEntity entity = ldk_entity_create(&registry);
  LDKEntityInfo* info = ldk_entity_get_info(&registry, entity);

  ASSERT_TRUE(info != NULL);
  ASSERT_TRUE(info->components.version == 0);

  ASSERT_TRUE(ldk_entity_add_component_ref(&registry, entity, 10, 3));
  ASSERT_TRUE(ldk_entity_has_component(&registry, entity, 10));
  ASSERT_TRUE(ldk_entity_find_component(&registry, entity, 10, &out_slot, &out_index));
  ASSERT_TRUE(out_index == 3);
  ASSERT_TRUE(info->components.version == 1);

  ASSERT_TRUE(ldk_entity_update_component_ref(&registry, entity, 10, 8));
  ASSERT_TRUE(ldk_entity_find_component(&registry, entity, 10, &out_slot, &out_index));
  ASSERT_TRUE(out_index == 8);
  ASSERT_TRUE(info->components.version == 1);

  ASSERT_TRUE(ldk_entity_remove_component_ref(&registry, entity, 10));
  ASSERT_TRUE(!ldk_entity_has_component(&registry, entity, 10));
  ASSERT_TRUE(info->components.version == 2);

  ldk_entity_module_terminate(&registry);
  return 0;
}


int test_entity_component_ref_dereference_and_invalidation(void)
{
  LDKComponentRef ref;
  LDKComponentRegistry component_registry;
  LDKEntityRegistry entity_registry;
  TestComponentA component = (TestComponentA){ .value = 77 };

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  LDKEntity entity = ldk_entity_create(&entity_registry);
  XArray* store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
  XArray* owners = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);

  ASSERT_TRUE(store != NULL);
  ASSERT_TRUE(owners != NULL);

  x_array_push(store, &component);
  x_array_push(owners, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_A, 0));

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref));

  TestComponentA* stored_component = (TestComponentA*)ldk_component_ref_get(&entity_registry, &component_registry, ref);
  ASSERT_TRUE(stored_component != NULL);
  ASSERT_TRUE(stored_component->value == 77);

  ASSERT_TRUE(ldk_entity_remove_component_ref(&entity_registry, entity, TEST_COMPONENT_A));
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
  TestComponentA component = (TestComponentA){.value = 12};

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  LDKEntity entity = ldk_entity_create(&entity_registry);
  XArray* store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
  XArray* owners = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);

  ASSERT_TRUE(store != NULL);
  ASSERT_TRUE(owners != NULL);

  x_array_push(store, &component);
  x_array_push(owners, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_A, 0));
  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref));

  TestComponentA* stored_component = (TestComponentA*)ldk_component_ref_get(&entity_registry, &component_registry, ref);
  ASSERT_TRUE(stored_component != NULL);
  stored_component->value = 99;

  const TestComponentA* stored_component_const = (const TestComponentA*)ldk_component_ref_get_const(
      &entity_registry,
      &component_registry,
      ref);
  ASSERT_TRUE(stored_component_const != NULL);
  ASSERT_TRUE(stored_component_const->value == 99);
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
  TestComponentA component = (TestComponentA){.value = 33};

  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  LDKEntity entity = ldk_entity_create(&entity_registry);
  XArray* store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
  XArray* owners = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);

  ASSERT_TRUE(store != NULL);
  ASSERT_TRUE(owners != NULL);

  x_array_push(store, &component);
  x_array_push(owners, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_A, 0));
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
  u32 out_slot = 0;
  u32 out_index = 0;
  TestComponentA component_a_0 = (TestComponentA){.value = 10};
  TestComponentB component_b_0 = (TestComponentB){.value = 30};
  TestComponentB component_b_1 = (TestComponentB){.value = 40};


  ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
  ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
  ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));
  ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentB",
        TEST_COMPONENT_B,
        sizeof(TestComponentB),
        8));

  LDKEntity entity = ldk_entity_create(&entity_registry);
  LDKEntityInfo* info = ldk_entity_get_info(&entity_registry, entity);
  XArray* store_a = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
  XArray* owners_a = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);
  XArray* store_b = ldk_component_get_store(&component_registry, TEST_COMPONENT_B);
  XArray* owners_b = ldk_component_get_owners(&component_registry, TEST_COMPONENT_B);

  ASSERT_TRUE(info != NULL);
  ASSERT_TRUE(store_a != NULL);
  ASSERT_TRUE(owners_a != NULL);
  ASSERT_TRUE(store_b != NULL);
  ASSERT_TRUE(owners_b != NULL);

  x_array_push(store_a, &component_a_0);
  x_array_push(owners_a, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_A, 0));
  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref_a));
  ASSERT_TRUE(info->components.version == 1);

  x_array_push(store_b, &component_b_0);
  x_array_push(owners_b, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_B, 0));
  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_B, &ref_b));
  ASSERT_TRUE(info->components.version == 2);

  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref_a));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref_b));

  ASSERT_TRUE(ldk_entity_remove_component_ref(&entity_registry, entity, TEST_COMPONENT_B));
  ASSERT_TRUE(info->components.version == 3);
  ASSERT_TRUE(!ldk_component_ref_is_valid(&entity_registry, ref_b));

  x_array_push(store_b, &component_b_1);
  x_array_push(owners_b, &entity);
  ASSERT_TRUE(ldk_entity_add_component_ref(&entity_registry, entity, TEST_COMPONENT_B, 1));
  ASSERT_TRUE(ldk_entity_find_component(&entity_registry, entity, TEST_COMPONENT_B, &out_slot, &out_index));
  ASSERT_TRUE(out_index == 1);
  ASSERT_TRUE(info->components.version == 4);

  ASSERT_TRUE(ldk_entity_get_component_ref(&entity_registry, entity, TEST_COMPONENT_A, &ref_a_new));
  ASSERT_TRUE(ldk_component_ref_is_valid(&entity_registry, ref_a_new));
  ASSERT_TRUE(((TestComponentA*)ldk_component_ref_get(&entity_registry, &component_registry, ref_a_new))->value == 10);

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

