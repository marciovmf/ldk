/**
 * @file   ldk_jobs.h
 * @brief  Shared worker job module
 */

#ifndef LDK_JOBS_H
#define LDK_JOBS_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LDK_JOBS_DEFAULT_WORKER_COUNT 4u

  typedef struct LDKJobs
  {
    void *internal;
    bool is_initialized;
  } LDKJobs;

  typedef struct LDKAsyncResultState LDKAsyncResultState;

  typedef struct LDKAsyncResult
  {
    LDKAsyncResultState *state;
  } LDKAsyncResult;

  typedef bool (*LDKJobFunc)(void *user_data);

  typedef enum LDKAsyncStatus
  {
    LDK_ASYNC_STATUS_INVALID = 0,
    LDK_ASYNC_STATUS_PENDING,
    LDK_ASYNC_STATUS_RUNNING,
    LDK_ASYNC_STATUS_SUCCEEDED,
    LDK_ASYNC_STATUS_FAILED,
  } LDKAsyncStatus;

  LDK_API bool ldk_jobs_initialize(LDKJobs *jobs, u32 worker_count);
  LDK_API void ldk_jobs_terminate(LDKJobs *jobs);

  LDK_API LDKAsyncResult ldk_jobs_submit(
      LDKJobs *jobs, LDKJobFunc function, void *user_data);

  LDK_API bool ldk_async_result_is_valid(LDKAsyncResult result);
  LDK_API bool ldk_async_result_is_done(LDKAsyncResult result);
  LDK_API LDKAsyncStatus ldk_async_result_status(LDKAsyncResult result);
  LDK_API bool ldk_async_result_wait(LDKAsyncResult result);
  LDK_API void ldk_async_result_release(LDKAsyncResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_JOBS_H */
