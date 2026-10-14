/**
 * # Runtime profiling
 *
 * Lightweight engine profiling instrumentation.
 *
 * The engine owns the collector in shared and monolithic builds.
 */

#ifndef LDK_PROFILER_H
#define LDK_PROFILER_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.profiler */
typedef enum LDKProfilerZoneKind
{
  LDK_PROFILER_ZONE_USER = 0,
  LDK_PROFILER_ZONE_FRAME,
  LDK_PROFILER_ZONE_PHASE,
  LDK_PROFILER_ZONE_BUCKET,
  LDK_PROFILER_ZONE_SYSTEM
} LDKProfilerZoneKind;

/** @group ldk.profiler */
typedef struct LDKProfilerSource
{
  volatile i32 id;
} LDKProfilerSource;

/** @group ldk.profiler */
typedef struct LDKProfilerCounter
{
  volatile i32 id;
} LDKProfilerCounter;

/**
 * @brief Initialize the profiler.
 * @param runtree_path Path of the runtree.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.profiler
 */
LDK_API bool ldk_profiler_initialize(const char *runtree_path);
/**
 * @brief Shut down the profiler.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_terminate(void);
/* Control capture only at a frame boundary, with producers quiescent. */
/**
 * @brief Start the profiler capture.
 * @param path Path identifying the file or resource.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.profiler
 */
LDK_API bool ldk_profiler_capture_start(const char *path);
/**
 * @brief Stop the profiler capture.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_capture_stop(void);
/* Suspend between editor frames/while paused without closing the file. */
/**
 * @brief Collect the current profiler sample data.
 * @param enabled Enabled used by this operation.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_capture_collect(bool enabled);
/**
 * @brief Toggle profiler capture state.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_capture_toggle(void);
/**
 * @brief Check whether the profiler capture is active.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.profiler
 */
LDK_API bool ldk_profiler_capture_is_active(void);
/**
 * @brief Get the profiler capture path.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group ldk.profiler
 */
LDK_API const char *ldk_profiler_capture_path_get(void);

/**
 * @brief Begin the profiler frame.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_frame_begin(void);
/**
 * @brief Finish the profiler frame.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_frame_end(void);

/**
 * @brief Begin a profiling zone tagged with a source location.
 * @param source Source object or source data for this operation.
 * @param kind Kind used by this operation.
 * @param name Name identifying the object or setting.
 * @param file File used by this operation.
 * @param function Function to run as a scheduled job.
 * @param line Line used by this operation.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_zone_begin_source(LDKProfilerSource *source,
    LDKProfilerZoneKind kind, const char *name, const char *file,
    const char *function, u32 line);
/**
 * @brief Finish the profiler zone.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_zone_end(void);
/**
 * @brief Close an active profiling zone of the specified kind.
 * @param kind Kind used by this operation.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_zone_end_kind(LDKProfilerZoneKind kind);

/**
 * @brief Assign a profiler counter value at a source location.
 * @param counter Counter used by this operation.
 * @param name Name identifying the object or setting.
 * @param value Input value or current widget value.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_counter_set_source(
    LDKProfilerCounter *counter, const char *name, double value);
/**
 * @brief Increment a profiler counter at a source location.
 * @param counter Counter used by this operation.
 * @param name Name identifying the object or setting.
 * @param value Input value or current widget value.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_counter_add_source(
    LDKProfilerCounter *counter, const char *name, double value);

/**
 * @brief Set the profiler thread name.
 * @param name Name identifying the object or setting.
 * @group ldk.profiler
 */
LDK_API void ldk_profiler_thread_name_set(const char *name);

/** @group ldk.profiler */
#define LDK_PROFILE_CONCAT_INNER(a, b) a##b
/** @group ldk.profiler */
#define LDK_PROFILE_CONCAT(a, b) LDK_PROFILE_CONCAT_INNER(a, b)

/** @group ldk.profiler */
#define LDK_PROFILE_BEGIN(name)                                                \
  LDK_PROFILE_BEGIN_IMPL(name, __LINE__)

/** @group ldk.profiler */
#define LDK_PROFILE_BEGIN_IMPL(name, line)                                     \
  do                                                                           \
  {                                                                            \
    static LDKProfilerSource LDK_PROFILE_CONCAT(                               \
        ldk_profiler_source_, line) = {0};                                     \
    ldk_profiler_zone_begin_source(                                            \
        &LDK_PROFILE_CONCAT(ldk_profiler_source_, line),                       \
        LDK_PROFILER_ZONE_USER, (name), __FILE__, __func__, (u32)(line));      \
  } while (0)

/** @group ldk.profiler */
#define LDK_PROFILE_END() ldk_profiler_zone_end()

/** @group ldk.profiler */
#define LDK_PROFILE_COUNTER_SET(name, value)                                   \
  LDK_PROFILE_COUNTER_SET_IMPL(name, value, __LINE__)

/** @group ldk.profiler */
#define LDK_PROFILE_COUNTER_SET_IMPL(name, value, line)                        \
  do                                                                           \
  {                                                                            \
    static LDKProfilerCounter LDK_PROFILE_CONCAT(                              \
        ldk_profiler_counter_, line) = {0};                                    \
    ldk_profiler_counter_set_source(                                           \
        &LDK_PROFILE_CONCAT(ldk_profiler_counter_, line), (name),              \
        (double)(value));                                                       \
  } while (0)

/** @group ldk.profiler */
#define LDK_PROFILE_COUNTER_ADD(name, value)                                   \
  LDK_PROFILE_COUNTER_ADD_IMPL(name, value, __LINE__)

/** @group ldk.profiler */
#define LDK_PROFILE_COUNTER_ADD_IMPL(name, value, line)                        \
  do                                                                           \
  {                                                                            \
    static LDKProfilerCounter LDK_PROFILE_CONCAT(                              \
        ldk_profiler_counter_, line) = {0};                                    \
    ldk_profiler_counter_add_source(                                           \
        &LDK_PROFILE_CONCAT(ldk_profiler_counter_, line), (name),              \
        (double)(value));                                                       \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // LDK_PROFILER_H
