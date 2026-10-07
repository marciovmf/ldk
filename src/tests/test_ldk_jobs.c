#include <module/ldk_jobs.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

static bool s_set_value(void *user_data)
{
  i32 *value = (i32 *)user_data;
  *value = 42;
  return true;
}

static bool s_fail(void *user_data)
{
  (void)user_data;
  return false;
}

static int test_jobs_result_success(void)
{
  LDKJobs jobs;
  i32 value = 0;

  ASSERT_TRUE(ldk_jobs_initialize(&jobs, 2));

  LDKAsyncResult result = ldk_jobs_submit(&jobs, s_set_value, &value);
  ASSERT_TRUE(ldk_async_result_is_valid(result));
  ASSERT_TRUE(ldk_async_result_wait(result));
  ASSERT_EQ(value, 42);
  ASSERT_TRUE(ldk_async_result_is_done(result));
  ASSERT_EQ(ldk_async_result_status(result), LDK_ASYNC_STATUS_SUCCEEDED);

  ldk_async_result_release(&result);
  ASSERT_FALSE(ldk_async_result_is_valid(result));
  ldk_jobs_terminate(&jobs);
  return 0;
}

static int test_jobs_result_failure(void)
{
  LDKJobs jobs;

  ASSERT_TRUE(ldk_jobs_initialize(&jobs, 1));

  LDKAsyncResult result = ldk_jobs_submit(&jobs, s_fail, NULL);
  ASSERT_TRUE(ldk_async_result_is_valid(result));
  ASSERT_FALSE(ldk_async_result_wait(result));
  ASSERT_TRUE(ldk_async_result_is_done(result));
  ASSERT_EQ(ldk_async_result_status(result), LDK_ASYNC_STATUS_FAILED);

  ldk_async_result_release(&result);
  ldk_jobs_terminate(&jobs);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_jobs_result_success),
      X_TEST(test_jobs_result_failure),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
