#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#include <module/ldk_entity.h>
#include <module/ldk_component.h>

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

int test_component_register_and_get_store(void)
{
  LDKComponentRegistry registry;
  XArray* store;

  ASSERT_TRUE(ldk_component_registry_initialize(&registry));

  ASSERT_TRUE(ldk_component_register(
        &registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  ASSERT_TRUE(ldk_component_is_registered(&registry, TEST_COMPONENT_A));

  store = ldk_component_get_store(&registry, TEST_COMPONENT_A);
  ASSERT_TRUE(store != NULL);

  ASSERT_TRUE(!ldk_component_is_registered(&registry, 999));
  ASSERT_TRUE(ldk_component_get_store(&registry, 999) == NULL);

  ldk_component_registry_terminate(&registry);
  return 0;
}

int test_component_duplicate_registration_fails(void)
{
  LDKComponentRegistry registry;

  ASSERT_TRUE(ldk_component_registry_initialize(&registry));

  ASSERT_TRUE(ldk_component_register(
        &registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  ASSERT_TRUE(!ldk_component_register(
        &registry,
        "TestComponentA_Duplicate",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

  ldk_component_registry_terminate(&registry);
  return 0;
}

int test_component_remove_entity_single_component(void)
{
    LDKComponentRegistry component_registry;
    LDKEntityRegistry entity_registry;
    LDKEntity entity;
    XArray* owners;
    XArray* store;
    TestComponentA* component_a = NULL;
    u32 out_slot = 0;
    u32 out_index = 0;

    ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
    ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
    ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

    entity = ldk_entity_create(&entity_registry);

    component_a = (TestComponentA*)ldk_entity_add_component(
        &entity_registry,
        &component_registry,
        entity,
        TEST_COMPONENT_A);
    ASSERT_TRUE(component_a != NULL);
    component_a->value = 42;

    store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
    owners = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);

    ASSERT_TRUE(store != NULL);
    ASSERT_TRUE(owners != NULL);

    ASSERT_TRUE(ldk_entity_find_component(
        &entity_registry,
        entity,
        TEST_COMPONENT_A,
        &out_slot,
        &out_index));
    ASSERT_TRUE(out_index == 0);

    ASSERT_TRUE(ldk_component_remove_entity(
        &component_registry,
        &entity_registry,
        entity,
        TEST_COMPONENT_A));

    ASSERT_TRUE(!ldk_entity_has_component(&entity_registry, entity, TEST_COMPONENT_A));
    ASSERT_TRUE(x_array_count(store) == 0);
    ASSERT_TRUE(x_array_count(owners) == 0);

    ldk_component_registry_terminate(&component_registry);
    ldk_entity_module_terminate(&entity_registry);
    return 0;
}

int test_component_remove_entity_updates_moved_owner_ref(void)
{
    LDKComponentRegistry component_registry;
    LDKEntityRegistry entity_registry;
    LDKEntity entity_a;
    LDKEntity entity_b;
    XArray* owners;
    XArray* store;
    TestComponentA* component_a = NULL;
    TestComponentA* component_b = NULL;
    TestComponentA* stored_component = NULL;
    LDKEntity* owner = NULL;
    u32 out_index = 0;

    ASSERT_TRUE(ldk_entity_module_initialize(&entity_registry, 16, 1));
    ASSERT_TRUE(ldk_component_registry_initialize(&component_registry));
    ASSERT_TRUE(ldk_component_register(
        &component_registry,
        "TestComponentA",
        TEST_COMPONENT_A,
        sizeof(TestComponentA),
        8));

    entity_a = ldk_entity_create(&entity_registry);
    entity_b = ldk_entity_create(&entity_registry);

    component_a = (TestComponentA*)ldk_entity_add_component(
        &entity_registry,
        &component_registry,
        entity_a,
        TEST_COMPONENT_A);
    ASSERT_TRUE(component_a != NULL);
    component_a->value = 11;

    component_b = (TestComponentA*)ldk_entity_add_component(
        &entity_registry,
        &component_registry,
        entity_b,
        TEST_COMPONENT_A);
    ASSERT_TRUE(component_b != NULL);
    component_b->value = 22;

    store = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
    owners = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);

    ASSERT_TRUE(store != NULL);
    ASSERT_TRUE(owners != NULL);

    ASSERT_TRUE(ldk_component_remove_entity(
        &component_registry,
        &entity_registry,
        entity_a,
        TEST_COMPONENT_A));

    ASSERT_TRUE(x_array_count(store) == 1);
    ASSERT_TRUE(x_array_count(owners) == 1);

    owner = (LDKEntity*)x_array_get(owners, 0);
    stored_component = (TestComponentA*)x_array_get(store, 0);

    ASSERT_TRUE(owner != NULL);
    ASSERT_TRUE(stored_component != NULL);
    ASSERT_TRUE(owner->index == entity_b.index);
    ASSERT_TRUE(owner->version == entity_b.version);
    ASSERT_TRUE(stored_component->value == 22);

    ASSERT_TRUE(ldk_entity_find_component(
        &entity_registry,
        entity_b,
        TEST_COMPONENT_A,
        NULL,
        &out_index));
    ASSERT_TRUE(out_index == 0);

    ldk_component_registry_terminate(&component_registry);
    ldk_entity_module_terminate(&entity_registry);
    return 0;
}

int test_component_registry_remove_all(void)
{
    LDKComponentRegistry component_registry;
    LDKEntityRegistry entity_registry;
    LDKEntity entity;
    XArray* owners_a;
    XArray* owners_b;
    XArray* store_a;
    XArray* store_b;
    TestComponentA* component_a = NULL;
    TestComponentB* component_b = NULL;

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

    entity = ldk_entity_create(&entity_registry);

    component_a = (TestComponentA*)ldk_entity_add_component(
        &entity_registry,
        &component_registry,
        entity,
        TEST_COMPONENT_A);
    ASSERT_TRUE(component_a != NULL);
    component_a->value = 11;

    component_b = (TestComponentB*)ldk_entity_add_component(
        &entity_registry,
        &component_registry,
        entity,
        TEST_COMPONENT_B);
    ASSERT_TRUE(component_b != NULL);
    component_b->value = 22;

    store_a = ldk_component_get_store(&component_registry, TEST_COMPONENT_A);
    store_b = ldk_component_get_store(&component_registry, TEST_COMPONENT_B);
    owners_a = ldk_component_get_owners(&component_registry, TEST_COMPONENT_A);
    owners_b = ldk_component_get_owners(&component_registry, TEST_COMPONENT_B);

    ASSERT_TRUE(store_a != NULL);
    ASSERT_TRUE(store_b != NULL);
    ASSERT_TRUE(owners_a != NULL);
    ASSERT_TRUE(owners_b != NULL);

    ASSERT_TRUE(ldk_entity_has_component(&entity_registry, entity, TEST_COMPONENT_A));
    ASSERT_TRUE(ldk_entity_has_component(&entity_registry, entity, TEST_COMPONENT_B));

    ldk_component_registry_remove_all(&component_registry, &entity_registry, entity);

    ASSERT_TRUE(!ldk_entity_has_component(&entity_registry, entity, TEST_COMPONENT_A));
    ASSERT_TRUE(!ldk_entity_has_component(&entity_registry, entity, TEST_COMPONENT_B));
    ASSERT_TRUE(x_array_count(store_a) == 0);
    ASSERT_TRUE(x_array_count(store_b) == 0);
    ASSERT_TRUE(x_array_count(owners_a) == 0);
    ASSERT_TRUE(x_array_count(owners_b) == 0);

    ldk_component_registry_terminate(&component_registry);
    ldk_entity_module_terminate(&entity_registry);
    return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_component_register_and_get_store),
    X_TEST(test_component_duplicate_registration_fails),
    X_TEST(test_component_remove_entity_single_component),
    X_TEST(test_component_remove_entity_updates_moved_owner_ref),
    X_TEST(test_component_registry_remove_all),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
