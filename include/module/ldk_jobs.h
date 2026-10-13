/**
 * # Job execution
 *
 * Shared worker job module
 */

#ifndef LDK_JOBS_H
#define LDK_JOBS_H

#include <ldk_common.h>
#include <stdx/stdx_thread.h>

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

  /**
   * @brief Callback executed as a job by the work scheduler.
   * @param user_data Opaque data forwarded to the callback or job.
   * @return True if the operation succeeds; false otherwise.
   */
  typedef bool (*LDKJobFunc)(void *user_data);

  typedef enum LDKAsyncStatus
  {
    LDK_ASYNC_STATUS_INVALID = 0,
    LDK_ASYNC_STATUS_PENDING,
    LDK_ASYNC_STATUS_RUNNING,
    LDK_ASYNC_STATUS_SUCCEEDED,
    LDK_ASYNC_STATUS_FAILED,
  } LDKAsyncStatus;

  /**
   * @brief Initialize the jobs.
   * @param jobs Jobs used by this operation.
   * @param worker_count Number of worker threads configured for the job pool.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_jobs_initialize(LDKJobs *jobs, u32 worker_count);
  /**
   * @brief Shut down the jobs.
   * @param jobs Jobs used by this operation.
   */
  LDK_API void ldk_jobs_terminate(LDKJobs *jobs);
  /**
   * @brief Shut down the job system and collect termination diagnostics.
   * @param jobs Jobs used by this operation.
   * @param callback Callback invoked by this operation.
   * @param user_data Opaque data forwarded to the callback or job.
   */
  LDK_API void ldk_jobs_terminate_with_diagnostics(LDKJobs *jobs,
      XThreadPoolShutdownCallback callback, void *user_data);

  /**
   * @brief Submit work to the job pool and obtain an asynchronous result.
   * @param jobs Jobs used by this operation.
   * @param function Function to run as a scheduled job.
   * @param user_data Opaque data forwarded to the callback or job.
   * @return Asynchronous result for tracking job completion.
   */
  LDK_API LDKAsyncResult ldk_jobs_submit(
      LDKJobs *jobs, LDKJobFunc function, void *user_data);

  /**
   * @brief Check whether the async result is valid.
   * @param result Optional result structure populated with error details.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_async_result_is_valid(LDKAsyncResult result);
  /**
   * @brief Check whether an asynchronous job has completed.
   * @param result Optional result structure populated with error details.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_async_result_is_done(LDKAsyncResult result);
  /**
   * @brief Read the completion status of an asynchronous job.
   * @param result Optional result structure populated with error details.
   * @return The requested value.
   */
  LDK_API LDKAsyncStatus ldk_async_result_status(LDKAsyncResult result);
  /**
   * @brief Wait for an asynchronous job to complete.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_async_result_wait(LDKAsyncResult result);
  /**
   * @brief Release resources associated with an asynchronous result.
   * @param result Optional result structure populated with error details.
   */
  LDK_API void ldk_async_result_release(LDKAsyncResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_JOBS_H */
