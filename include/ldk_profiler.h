/**
 * @file   ldk_profiler.h
 * @brief  Lightweight engine profiling instrumentation.
 *
 * The engine owns the collector in shared and monolithic builds.
 */

#ifndef LDK_PROFILER_H
#define LDK_PROFILER_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LDKProfilerZoneKind
{
  LDK_PROFILER_ZONE_USER = 0,
  LDK_PROFILER_ZONE_FRAME,
  LDK_PROFILER_ZONE_PHASE,
  LDK_PROFILER_ZONE_BUCKET,
  LDK_PROFILER_ZONE_SYSTEM
} LDKProfilerZoneKind;

typedef struct LDKProfilerSource
{
  volatile i32 id;
} LDKProfilerSource;

typedef struct LDKProfilerCounter
{
  volatile i32 id;
} LDKProfilerCounter;

LDK_API bool ldk_profiler_initialize(const char *runtree_path);
LDK_API void ldk_profiler_terminate(void);
/* Control capture only at a frame boundary, with producers quiescent. */
LDK_API bool ldk_profiler_capture_start(const char *path);
LDK_API void ldk_profiler_capture_stop(void);
/* Suspend between editor frames/while paused without closing the file. */
LDK_API void ldk_profiler_capture_collect(bool enabled);
LDK_API void ldk_profiler_capture_toggle(void);
LDK_API bool ldk_profiler_capture_is_active(void);
LDK_API const char *ldk_profiler_capture_path_get(void);

LDK_API void ldk_profiler_frame_begin(void);
LDK_API void ldk_profiler_frame_end(void);

LDK_API void ldk_profiler_zone_begin_source(LDKProfilerSource *source,
    LDKProfilerZoneKind kind, const char *name, const char *file,
    const char *function, u32 line);
LDK_API void ldk_profiler_zone_end(void);
LDK_API void ldk_profiler_zone_end_kind(LDKProfilerZoneKind kind);

LDK_API void ldk_profiler_counter_set_source(
    LDKProfilerCounter *counter, const char *name, double value);
LDK_API void ldk_profiler_counter_add_source(
    LDKProfilerCounter *counter, const char *name, double value);

LDK_API void ldk_profiler_thread_name_set(const char *name);

#define LDK_PROFILE_CONCAT_INNER(a, b) a##b
#define LDK_PROFILE_CONCAT(a, b) LDK_PROFILE_CONCAT_INNER(a, b)

#define LDK_PROFILE_BEGIN(name)                                                \
  LDK_PROFILE_BEGIN_IMPL(name, __LINE__)

#define LDK_PROFILE_BEGIN_IMPL(name, line)                                     \
  do                                                                           \
  {                                                                            \
    static LDKProfilerSource LDK_PROFILE_CONCAT(                               \
        ldk_profiler_source_, line) = {0};                                     \
    ldk_profiler_zone_begin_source(                                            \
        &LDK_PROFILE_CONCAT(ldk_profiler_source_, line),                       \
        LDK_PROFILER_ZONE_USER, (name), __FILE__, __func__, (u32)(line));      \
  } while (0)

#define LDK_PROFILE_END() ldk_profiler_zone_end()

#define LDK_PROFILE_COUNTER_SET(name, value)                                   \
  LDK_PROFILE_COUNTER_SET_IMPL(name, value, __LINE__)

#define LDK_PROFILE_COUNTER_SET_IMPL(name, value, line)                        \
  do                                                                           \
  {                                                                            \
    static LDKProfilerCounter LDK_PROFILE_CONCAT(                              \
        ldk_profiler_counter_, line) = {0};                                    \
    ldk_profiler_counter_set_source(                                           \
        &LDK_PROFILE_CONCAT(ldk_profiler_counter_, line), (name),              \
        (double)(value));                                                       \
  } while (0)

#define LDK_PROFILE_COUNTER_ADD(name, value)                                   \
  LDK_PROFILE_COUNTER_ADD_IMPL(name, value, __LINE__)

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
