#include <stdx/stdx_common.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <ldk_system.h>
#include <stdlib.h>

#define DELTA_TIME 0.016f
enum
{
  TEST_SYSTEM_ID_A = 1001,
  TEST_SYSTEM_ID_B = 1002,
  TEST_SYSTEM_ID_C = 1003
};

typedef struct TestSystemState
{
  int marker;
} TestSystemState;

static int g_initialize_count = 0;
static int g_terminate_count = 0;
static int g_pre_update_count = 0;
static int g_update_count = 0;
static int g_post_update_count = 0;
static int g_render_count = 0;

static int g_callback_ok = 1;

static int g_order_log[32];
static int g_order_log_count = 0;

static void test_reset_counters(void)
{
  g_initialize_count = 0;
  g_terminate_count = 0;
  g_pre_update_count = 0;
  g_update_count = 0;
  g_post_update_count = 0;
  g_render_count = 0;
  g_callback_ok = 1;
  g_order_log_count = 0;
}

static int test_system_initialize(void** userdata)
{
  TestSystemState* state = NULL;

  state = (TestSystemState*)malloc(sizeof(TestSystemState));
  if (!state)
  {
    g_callback_ok = 0;
    return 1;
  }

  state->marker = 123;
  *userdata = state;
  g_initialize_count++;
  return 0;
}

static void test_system_terminate(void* userdata)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state)
  {
    g_callback_ok = 0;
    return;
  }

  if (state->marker != 123)
  {
    g_callback_ok = 0;
    return;
  }

  free(state);
  g_terminate_count++;
}

static void test_system_pre_update(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_pre_update_count++;
}

static void test_system_update_a(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_order_log[g_order_log_count++] = 1;
  g_update_count++;
}

static void test_system_update_b(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_order_log[g_order_log_count++] = 2;
  g_update_count++;
}

static void test_system_update_c(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_order_log[g_order_log_count++] = 3;
  g_update_count++;
}

static void test_system_post_update(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_post_update_count++;
}

static void test_system_render(void* userdata, float dt)
{
  TestSystemState* state = (TestSystemState*)userdata;

  if (!state || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  g_render_count++;
}

int test_system_registry_register_has_unregister(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc desc;
  LDKSystemDesc found;

  test_reset_counters();

  desc.id = TEST_SYSTEM_ID_A;
  desc.name = "system_a";
  desc.flags = LDK_SYSTEM_FLAG_ENABLED;
  desc.pre_update_order = 0;
  desc.update_order = 10;
  desc.post_update_order = 0;
  desc.render_order = 0;
  desc.callbacks.initialize = test_system_initialize;
  desc.callbacks.terminate = test_system_terminate;
  desc.callbacks.pre_update = NULL;
  desc.callbacks.update = test_system_update_a;
  desc.callbacks.post_update = NULL;
  desc.callbacks.render = NULL;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_find_by_id(&registry, TEST_SYSTEM_ID_A, &found));
  ASSERT_TRUE(found.id == TEST_SYSTEM_ID_A);

  ASSERT_TRUE(ldk_system_registry_unregister(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_find_by_id(&registry, TEST_SYSTEM_ID_A, &found));

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_duplicate_id_fails(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc a;
  LDKSystemDesc b;

  test_reset_counters();

  a.id = TEST_SYSTEM_ID_A;
  a.name = "system_a";
  a.flags = LDK_SYSTEM_FLAG_ENABLED;
  a.pre_update_order = 0;
  a.update_order = 0;
  a.post_update_order = 0;
  a.render_order = 0;
  a.callbacks.initialize = test_system_initialize;
  a.callbacks.terminate = test_system_terminate;
  a.callbacks.pre_update = NULL;
  a.callbacks.update = test_system_update_a;
  a.callbacks.post_update = NULL;
  a.callbacks.render = NULL;

  b = a;
  b.name = "system_b_same_id";

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b) == false);

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_start_run_stop(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc desc;

  test_reset_counters();

  desc.id = TEST_SYSTEM_ID_A;
  desc.name = "system_a";
  desc.flags = LDK_SYSTEM_FLAG_ENABLED;
  desc.pre_update_order = 1;
  desc.update_order = 2;
  desc.post_update_order = 3;
  desc.render_order = 4;
  desc.callbacks.initialize = test_system_initialize;
  desc.callbacks.terminate = test_system_terminate;
  desc.callbacks.pre_update = test_system_pre_update;
  desc.callbacks.update = test_system_update_a;
  desc.callbacks.post_update = test_system_post_update;
  desc.callbacks.render = test_system_render;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));

  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(g_initialize_count == 1);
  ASSERT_TRUE(g_callback_ok);

  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_PRE_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_POST_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_RENDER, DELTA_TIME));

  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(g_pre_update_count == 1);
  ASSERT_TRUE(g_update_count == 1);
  ASSERT_TRUE(g_post_update_count == 1);
  ASSERT_TRUE(g_render_count == 1);

  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(g_terminate_count == 1);

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_update_order(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc a;
  LDKSystemDesc b;
  LDKSystemDesc c;

  test_reset_counters();

  a.id = TEST_SYSTEM_ID_A;
  a.name = "system_a";
  a.flags = LDK_SYSTEM_FLAG_ENABLED;
  a.pre_update_order = 0;
  a.update_order = 30;
  a.post_update_order = 0;
  a.render_order = 0;
  a.callbacks.initialize = test_system_initialize;
  a.callbacks.terminate = test_system_terminate;
  a.callbacks.pre_update = NULL;
  a.callbacks.update = test_system_update_a;
  a.callbacks.post_update = NULL;
  a.callbacks.render = NULL;

  b.id = TEST_SYSTEM_ID_B;
  b.name = "system_b";
  b.flags = LDK_SYSTEM_FLAG_ENABLED;
  b.pre_update_order = 0;
  b.update_order = 10;
  b.post_update_order = 0;
  b.render_order = 0;
  b.callbacks.initialize = test_system_initialize;
  b.callbacks.terminate = test_system_terminate;
  b.callbacks.pre_update = NULL;
  b.callbacks.update = test_system_update_b;
  b.callbacks.post_update = NULL;
  b.callbacks.render = NULL;

  c.id = TEST_SYSTEM_ID_C;
  c.name = "system_c";
  c.flags = LDK_SYSTEM_FLAG_ENABLED;
  c.pre_update_order = 0;
  c.update_order = 20;
  c.post_update_order = 0;
  c.render_order = 0;
  c.callbacks.initialize = test_system_initialize;
  c.callbacks.terminate = test_system_terminate;
  c.callbacks.pre_update = NULL;
  c.callbacks.update = test_system_update_c;
  c.callbacks.post_update = NULL;
  c.callbacks.render = NULL;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &c));

  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));

  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(g_order_log_count == 3);
  ASSERT_TRUE(g_order_log[0] == 2);
  ASSERT_TRUE(g_order_log[1] == 3);
  ASSERT_TRUE(g_order_log[2] == 1);

  ASSERT_TRUE(ldk_system_registry_stop(&registry));

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_equal_order_uses_registration_order(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc a;
  LDKSystemDesc b;

  test_reset_counters();

  a.id = TEST_SYSTEM_ID_A;
  a.name = "system_a";
  a.flags = LDK_SYSTEM_FLAG_ENABLED;
  a.pre_update_order = 0;
  a.update_order = 10;
  a.post_update_order = 0;
  a.render_order = 0;
  a.callbacks.initialize = test_system_initialize;
  a.callbacks.terminate = test_system_terminate;
  a.callbacks.pre_update = NULL;
  a.callbacks.update = test_system_update_a;
  a.callbacks.post_update = NULL;
  a.callbacks.render = NULL;

  b.id = TEST_SYSTEM_ID_B;
  b.name = "system_b";
  b.flags = LDK_SYSTEM_FLAG_ENABLED;
  b.pre_update_order = 0;
  b.update_order = 10;
  b.post_update_order = 0;
  b.render_order = 0;
  b.callbacks.initialize = test_system_initialize;
  b.callbacks.terminate = test_system_terminate;
  b.callbacks.pre_update = NULL;
  b.callbacks.update = test_system_update_b;
  b.callbacks.post_update = NULL;
  b.callbacks.render = NULL;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));

  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_run_bucket(&registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));

  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(g_order_log_count == 2);
  ASSERT_TRUE(g_order_log[0] == 1);
  ASSERT_TRUE(g_order_log[1] == 2);

  ASSERT_TRUE(ldk_system_registry_stop(&registry));

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_mutation_rejected_while_started(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc desc;

  test_reset_counters();

  desc.id = TEST_SYSTEM_ID_A;
  desc.name = "system_a";
  desc.flags = LDK_SYSTEM_FLAG_ENABLED;
  desc.pre_update_order = 0;
  desc.update_order = 0;
  desc.post_update_order = 0;
  desc.render_order = 0;
  desc.callbacks.initialize = test_system_initialize;
  desc.callbacks.terminate = test_system_terminate;
  desc.callbacks.pre_update = NULL;
  desc.callbacks.update = test_system_update_a;
  desc.callbacks.post_update = NULL;
  desc.callbacks.render = NULL;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(ldk_system_registry_start(&registry));

  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc) == false);
  ASSERT_TRUE(ldk_system_registry_unregister(&registry, TEST_SYSTEM_ID_A) == false);
  ASSERT_TRUE(ldk_system_registry_clear(&registry) == false);

  ASSERT_TRUE(ldk_system_registry_stop(&registry));

  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_clear_while_stopped(void)
{
  LDKSystemRegistry registry;
  LDKSystemDesc desc_a;
  LDKSystemDesc desc_b;

  test_reset_counters();

  desc_a.id = TEST_SYSTEM_ID_A;
  desc_a.name = "system_a";
  desc_a.flags = LDK_SYSTEM_FLAG_ENABLED;
  desc_a.pre_update_order = 0;
  desc_a.update_order = 0;
  desc_a.post_update_order = 0;
  desc_a.render_order = 0;
  desc_a.callbacks.initialize = test_system_initialize;
  desc_a.callbacks.terminate = test_system_terminate;
  desc_a.callbacks.pre_update = NULL;
  desc_a.callbacks.update = test_system_update_a;
  desc_a.callbacks.post_update = NULL;
  desc_a.callbacks.render = NULL;

  desc_b = desc_a;
  desc_b.id = TEST_SYSTEM_ID_B;
  desc_b.name = "system_b";

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc_a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc_b));

  ASSERT_TRUE(ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_has(&registry, TEST_SYSTEM_ID_B));

  ASSERT_TRUE(ldk_system_registry_clear(&registry));

  ASSERT_TRUE(!ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_has(&registry, TEST_SYSTEM_ID_B));

  ldk_system_registry_terminate(&registry);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_system_registry_register_has_unregister),
    X_TEST(test_system_registry_duplicate_id_fails),
    X_TEST(test_system_registry_start_run_stop),
    X_TEST(test_system_registry_update_order),
    X_TEST(test_system_registry_equal_order_uses_registration_order),
    X_TEST(test_system_registry_mutation_rejected_while_started),
    X_TEST(test_system_registry_clear_while_stopped),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
