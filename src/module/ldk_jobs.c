#include <module/ldk_jobs.h>


#include <stdx/stdx_thread.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKJobsInternal
{
  XThreadPool *pool;
} LDKJobsInternal;

struct LDKAsyncResultState
{
  XMutex *mutex;
  XCondVar *condition;
  LDKAsyncStatus status;
};

typedef struct LDKJobExecution
{
  LDKAsyncResultState *result;
  LDKJobFunc function;
  void *user_data;
} LDKJobExecution;

static bool s_async_status_is_done(LDKAsyncStatus status)
{
  return status == LDK_ASYNC_STATUS_SUCCEEDED ||
         status == LDK_ASYNC_STATUS_FAILED;
}

static LDKAsyncResultState *s_async_result_state_create(void)
{
  LDKAsyncResultState *state =
      (LDKAsyncResultState *)calloc(1, sizeof(*state));
  if (!state)
  {
    return NULL;
  }

  if (x_thread_mutex_init(&state->mutex) != 0 ||
      x_thread_condvar_init(&state->condition) != 0)
  {
    if (state->condition)
    {
      x_thread_condvar_destroy(state->condition);
    }
    if (state->mutex)
    {
      x_thread_mutex_destroy(state->mutex);
    }
    free(state);
    return NULL;
  }

  state->status = LDK_ASYNC_STATUS_PENDING;
  return state;
}

static void s_async_result_state_destroy(LDKAsyncResultState *state)
{
  if (!state)
  {
    return;
  }

  x_thread_condvar_destroy(state->condition);
  x_thread_mutex_destroy(state->mutex);
  free(state);
}

static void s_job_execute(void *user_data)
{
  LDKJobExecution *execution = (LDKJobExecution *)user_data;
  bool succeeded;

  x_thread_mutex_lock(execution->result->mutex);
  execution->result->status = LDK_ASYNC_STATUS_RUNNING;
  x_thread_mutex_unlock(execution->result->mutex);

  succeeded = execution->function(execution->user_data);

  x_thread_mutex_lock(execution->result->mutex);
  execution->result->status = succeeded ? LDK_ASYNC_STATUS_SUCCEEDED
                                        : LDK_ASYNC_STATUS_FAILED;
  x_thread_condvar_broadcast(execution->result->condition);
  x_thread_mutex_unlock(execution->result->mutex);

  free(execution);
}

bool ldk_jobs_initialize(LDKJobs *jobs, u32 worker_count)
{
  if (!jobs || worker_count == 0 || worker_count > (u32)INT32_MAX)
  {
    return false;
  }

  memset(jobs, 0, sizeof(*jobs));

  LDKJobsInternal *internal =
      (LDKJobsInternal *)calloc(1, sizeof(*internal));
  if (!internal)
  {
    return false;
  }

  internal->pool = x_threadpool_create((int)worker_count);
  if (!internal->pool)
  {
    free(internal);
    return false;
  }

  jobs->internal = internal;
  jobs->is_initialized = true;
  return true;
}

void ldk_jobs_terminate_with_diagnostics(LDKJobs *jobs,
    XThreadPoolShutdownCallback callback, void *user_data)
{
  if (!jobs)
  {
    return;
  }

  LDKJobsInternal *internal = (LDKJobsInternal *)jobs->internal;
  if (internal)
  {
    if (internal->pool)
    {
      x_threadpool_destroy_with_diagnostics(internal->pool, callback, user_data);
    }
    free(internal);
  }

  memset(jobs, 0, sizeof(*jobs));
}

void ldk_jobs_terminate(LDKJobs *jobs)
{
  ldk_jobs_terminate_with_diagnostics(jobs, NULL, NULL);
}

LDKAsyncResult ldk_jobs_submit(
    LDKJobs *jobs, LDKJobFunc function, void *user_data)
{
  LDKAsyncResult result = {0};
  LDKJobExecution *execution;

  if (!jobs || !jobs->is_initialized || !jobs->internal || !function)
  {
    return result;
  }

  LDKJobsInternal *internal = (LDKJobsInternal *)jobs->internal;

  result.state = s_async_result_state_create();
  if (!result.state)
  {
    return result;
  }

  execution = (LDKJobExecution *)malloc(sizeof(*execution));
  if (!execution)
  {
    s_async_result_state_destroy(result.state);
    result.state = NULL;
    return result;
  }

  execution->result = result.state;
  execution->function = function;
  execution->user_data = user_data;

  if (x_threadpool_enqueue(internal->pool, s_job_execute, execution) != 0)
  {
    free(execution);
    s_async_result_state_destroy(result.state);
    result.state = NULL;
  }

  return result;
}

bool ldk_async_result_is_valid(LDKAsyncResult result)
{
  return result.state != NULL;
}

LDKAsyncStatus ldk_async_result_status(LDKAsyncResult result)
{
  LDKAsyncStatus status;

  if (!result.state)
  {
    return LDK_ASYNC_STATUS_INVALID;
  }

  x_thread_mutex_lock(result.state->mutex);
  status = result.state->status;
  x_thread_mutex_unlock(result.state->mutex);
  return status;
}

bool ldk_async_result_is_done(LDKAsyncResult result)
{
  return s_async_status_is_done(ldk_async_result_status(result));
}

bool ldk_async_result_wait(LDKAsyncResult result)
{
  LDKAsyncStatus status;

  if (!result.state)
  {
    return false;
  }

  x_thread_mutex_lock(result.state->mutex);
  while (!s_async_status_is_done(result.state->status))
  {
    x_thread_condvar_wait(result.state->condition, result.state->mutex);
  }
  status = result.state->status;
  x_thread_mutex_unlock(result.state->mutex);

  return status == LDK_ASYNC_STATUS_SUCCEEDED;
}

void ldk_async_result_release(LDKAsyncResult *result)
{
  if (!result || !result->state)
  {
    return;
  }

  (void)ldk_async_result_wait(*result);
  s_async_result_state_destroy(result->state);
  result->state = NULL;
}
