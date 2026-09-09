#if defined(LDK_SHAREDLIB)
#define X_IMPL_LOG
#endif

#include <ldk_common.h>
#include <module/ldk_system.h>
#include <stdx/stdx_log.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>
#include <stdlib.h>
#include <string.h>

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
  int generation;
} TestSystemState;

static LDKSystemRegistry *g_registry;
static int g_initialize_count;
static int g_terminate_count;
static int g_update_count;
static int g_pre_update_count;
static int g_post_update_count;
static int g_render_count;
static int g_callback_ok;
static int g_fail_initialize;
static int g_check_mutation;
static int g_order_log[64];
static int g_order_log_count;

static void test_reset_counters(void)
{
  g_registry = NULL;
  g_initialize_count = 0;
  g_terminate_count = 0;
  g_update_count = 0;
  g_pre_update_count = 0;
  g_post_update_count = 0;
  g_render_count = 0;
  g_callback_ok = 1;
  g_fail_initialize = 0;
  g_check_mutation = 0;
  g_order_log_count = 0;
  memset(g_order_log, 0, sizeof(g_order_log));
}

static int test_system_initialize(void **userdata)
{
  TestSystemState *state = (TestSystemState *)malloc(sizeof(*state));
  if (!state)
  {
    g_callback_ok = 0;
    return 1;
  }

  state->marker = 123;
  state->generation = ++g_initialize_count;
  *userdata = state;

  if (g_check_mutation)
  {
    if (ldk_system_registry_system_start(g_registry, TEST_SYSTEM_ID_A) ||
        ldk_system_registry_stop(g_registry) ||
        ldk_system_registry_pause(g_registry) ||
        ldk_system_registry_clear(g_registry))
    {
      g_callback_ok = 0;
    }
  }

  return g_fail_initialize ? 1 : 0;
}

static void test_system_terminate(void *userdata)
{
  TestSystemState *state = (TestSystemState *)userdata;
  if (!state || state->marker != 123)
  {
    g_callback_ok = 0;
    return;
  }

  if (g_check_mutation &&
      (ldk_system_registry_system_start(g_registry, TEST_SYSTEM_ID_A) ||
          ldk_system_registry_stop(g_registry)))
  {
    g_callback_ok = 0;
  }

  free(state);
  g_terminate_count++;
}

static void test_system_update(void *userdata, float dt)
{
  TestSystemState *state = (TestSystemState *)userdata;
  if (!state || state->marker != 123 || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }

  if (g_check_mutation &&
      (ldk_system_registry_system_stop(g_registry, TEST_SYSTEM_ID_A) ||
          ldk_system_registry_run_bucket(
              g_registry, LDK_SYSTEM_BUCKET_UPDATE, dt) ||
          ldk_system_registry_resume(g_registry)))
  {
    g_callback_ok = 0;
  }

  g_update_count++;
}

static void test_system_update_a(void *userdata, float dt)
{
  test_system_update(userdata, dt);
  g_order_log[g_order_log_count++] = 1;
}

static void test_system_update_b(void *userdata, float dt)
{
  test_system_update(userdata, dt);
  g_order_log[g_order_log_count++] = 2;
}

static void test_system_update_c(void *userdata, float dt)
{
  test_system_update(userdata, dt);
  g_order_log[g_order_log_count++] = 3;
}

static void test_system_pre_update(void *userdata, float dt)
{
  if (!userdata || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }
  g_pre_update_count++;
}

static void test_system_post_update(void *userdata, float dt)
{
  if (!userdata || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }
  g_post_update_count++;
}

static void test_system_render(void *userdata, float dt)
{
  if (!userdata || dt <= 0.0f)
  {
    g_callback_ok = 0;
    return;
  }
  g_render_count++;
}

static LDKSystemDesc test_system_desc(
    u64 id, i32 order, LDKSystemUpdateFn update)
{
  LDKSystemDesc desc = {0};
  desc.id = id;
  desc.name = "test_system";
  desc.flags = LDK_SYSTEM_FLAG_ENABLED;
  desc.update_order = order;
  desc.callbacks.initialize = test_system_initialize;
  desc.callbacks.terminate = test_system_terminate;
  desc.callbacks.update = update;
  return desc;
}

int test_system_registry_register_has_unregister(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc desc = test_system_desc(
      TEST_SYSTEM_ID_A, 10, test_system_update_a);
  LDKSystemDesc found;

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_find_by_id(
      &registry, TEST_SYSTEM_ID_A, &found));
  ASSERT_TRUE(found.id == TEST_SYSTEM_ID_A);
  ASSERT_TRUE(ldk_system_registry_count(&registry) == 1);
  ASSERT_TRUE(ldk_system_registry_at(&registry, 0, &found));
  ASSERT_TRUE(found.id == TEST_SYSTEM_ID_A);
  ASSERT_TRUE(!ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(ldk_system_registry_unregister(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_has(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_find_by_id(
      &registry, TEST_SYSTEM_ID_A, &found));
  ASSERT_TRUE(ldk_system_registry_clear(&registry));
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_start_run_stop(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc desc = test_system_desc(
      TEST_SYSTEM_ID_A, 2, test_system_update_a);

  test_reset_counters();
  desc.pre_update_order = 1;
  desc.post_update_order = 3;
  desc.render_order = 4;
  desc.callbacks.pre_update = test_system_pre_update;
  desc.callbacks.post_update = test_system_post_update;
  desc.callbacks.render = test_system_render;

  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(!ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(g_initialize_count == 0);
  ASSERT_TRUE(!ldk_system_registry_system_is_started(
      &registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_update_count == 0);

  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(g_initialize_count == 1);
  ASSERT_TRUE(ldk_system_registry_system_is_started(
      &registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_system_start(&registry, 9999));
  ASSERT_TRUE(!ldk_system_registry_system_stop(&registry, 9999));

  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_PRE_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_POST_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_RENDER, DELTA_TIME));
  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(g_pre_update_count == 1);
  ASSERT_TRUE(g_update_count == 1);
  ASSERT_TRUE(g_post_update_count == 1);
  ASSERT_TRUE(g_render_count == 1);

  ASSERT_TRUE(ldk_system_registry_system_stop(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_stop(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(g_terminate_count == 1);
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_update_count == 1);
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(g_initialize_count == 2);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_terminate_count == 2);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(ldk_system_registry_count(&registry) == 1);
  ASSERT_TRUE(!ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_update_order(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 30, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 10, test_system_update_b);
  LDKSystemDesc c = test_system_desc(
      TEST_SYSTEM_ID_C, 20, test_system_update_c);

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &c));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_C));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log_count == 3);
  ASSERT_TRUE(g_order_log[0] == 2);
  ASSERT_TRUE(g_order_log[1] == 3);
  ASSERT_TRUE(g_order_log[2] == 1);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_terminate_count == 3);
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_equal_order_uses_registration_order(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 10, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 10, test_system_update_b);

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log_count == 2);
  ASSERT_TRUE(g_order_log[0] == 1);
  ASSERT_TRUE(g_order_log[1] == 2);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_individual_activation(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 30, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 10, test_system_update_b);
  LDKSystemDesc c = test_system_desc(
      TEST_SYSTEM_ID_C, 20, test_system_update_c);

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &c));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(!ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(!ldk_system_registry_unregister(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(!ldk_system_registry_clear(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_C));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log_count == 2);
  ASSERT_TRUE(g_order_log[0] == 3);
  ASSERT_TRUE(g_order_log[1] == 1);
  ASSERT_TRUE(ldk_system_registry_system_stop(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log[2] == 2);
  ASSERT_TRUE(g_order_log[3] == 3);
  ASSERT_TRUE(g_initialize_count == 3);
  ASSERT_TRUE(g_terminate_count == 1);
  ASSERT_TRUE(ldk_system_registry_system_is_started(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_system_is_started(&registry, TEST_SYSTEM_ID_C));
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_terminate_count == 3);
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_pause_resume(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 10, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 20, test_system_update_b);

  test_reset_counters();
  b.flags |= LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED;
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_pause(&registry));
  ASSERT_TRUE(ldk_system_registry_pause(&registry));
  ASSERT_TRUE(ldk_system_registry_is_paused(&registry));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log_count == 1 && g_order_log[0] == 2);
  ASSERT_TRUE(g_terminate_count == 0);
  ASSERT_TRUE(ldk_system_registry_resume(&registry));
  ASSERT_TRUE(ldk_system_registry_resume(&registry));
  ASSERT_TRUE(!ldk_system_registry_is_paused(&registry));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(g_order_log_count == 3);
  ASSERT_TRUE(g_order_log[1] == 1 && g_order_log[2] == 2);
  ASSERT_TRUE(g_initialize_count == 2 && g_terminate_count == 0);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_terminate_count == 2);
  ASSERT_TRUE(!ldk_system_registry_is_paused(&registry));
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_mutation_rejected_in_callbacks(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc desc = test_system_desc(
      TEST_SYSTEM_ID_A, 0, test_system_update_a);

  test_reset_counters();
  g_registry = &registry;
  g_check_mutation = 1;
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &desc));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(ldk_system_registry_run_bucket(
      &registry, LDK_SYSTEM_BUCKET_UPDATE, DELTA_TIME));
  ASSERT_TRUE(ldk_system_registry_system_stop(&registry, TEST_SYSTEM_ID_A));
  ASSERT_TRUE(g_callback_ok);
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_failed_initialize(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 0, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 0, test_system_update_b);

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_start(&registry));
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_A));
  g_fail_initialize = 1;
  ASSERT_TRUE(!ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(g_initialize_count == 2 && g_terminate_count == 1);
  ASSERT_TRUE(!ldk_system_registry_system_is_started(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_system_is_started(&registry, TEST_SYSTEM_ID_A));
  g_fail_initialize = 0;
  ASSERT_TRUE(ldk_system_registry_system_start(&registry, TEST_SYSTEM_ID_B));
  ASSERT_TRUE(ldk_system_registry_stop(&registry));
  ASSERT_TRUE(g_initialize_count == 3 && g_terminate_count == 3);
  ASSERT_TRUE(g_callback_ok);
  ldk_system_registry_terminate(&registry);
  return 0;
}

int test_system_registry_clear_while_stopped(void)
{
  LDKSystemRegistry registry = {0};
  LDKSystemDesc a = test_system_desc(
      TEST_SYSTEM_ID_A, 0, test_system_update_a);
  LDKSystemDesc b = test_system_desc(
      TEST_SYSTEM_ID_B, 0, test_system_update_b);

  test_reset_counters();
  ASSERT_TRUE(ldk_system_registry_initialize(&registry));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &a));
  ASSERT_TRUE(ldk_system_registry_register(&registry, &b));
  ASSERT_TRUE(ldk_system_registry_clear(&registry));
  ASSERT_TRUE(ldk_system_registry_count(&registry) == 0);
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
    X_TEST(test_system_registry_start_run_stop),
    X_TEST(test_system_registry_update_order),
    X_TEST(test_system_registry_equal_order_uses_registration_order),
    X_TEST(test_system_registry_individual_activation),
    X_TEST(test_system_registry_pause_resume),
    X_TEST(test_system_registry_mutation_rejected_in_callbacks),
    X_TEST(test_system_registry_failed_initialize),
    X_TEST(test_system_registry_clear_while_stopped),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
