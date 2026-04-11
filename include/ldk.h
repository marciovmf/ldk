/**
 * @file   ldk.h
 * @brief  Engine root and global system access
 *
 * Entry point for the LDK engine. Provides initialization, shutdown,
 * and access to core subsystems.
 */

#ifndef LDK_H
#define LDK_H

#include <ldk_common.h>
#include <stdx/stdx_log.h>

#ifdef __cplusplus
extern "C" {
#endif

  //
  // Utility logging macros
  //

#define ldk_log_raw(out, level, fg, bg, components, fmt, ...) \
  x_log_raw(ldk_system_get(LDK_SYSTEM_LOG), (out), (level), (fg), (bg), (components), fmt, ##__VA_ARGS__)

#define ldk_log_debug(fmt, ...) \
  x_log_debug(ldk_system_get(LDK_SYSTEM_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_info(fmt, ...) \
  x_log_info(ldk_system_get(LDK_SYSTEM_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_warning(fmt, ...) \
  x_log_warning(ldk_system_get(LDK_SYSTEM_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_error(fmt, ...) \
  x_log_error(ldk_system_get(LDK_SYSTEM_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_fatal(fmt, ...) \
  x_log_fatal(ldk_system_get(LDK_SYSTEM_LOG), fmt, ##__VA_ARGS__)

  typedef struct LDKRoot LDKRoot;

  typedef enum LDKSystemType
  {
    LDK_SYSTEM_NONE = 0,
    LDK_SYSTEM_ENTITY,
    LDK_SYSTEM_COMPONENT,
    LDK_SYSTEM_EVENT,
    LDK_SYSTEM_LOG,
  } LDKSystemType;

  LDK_API bool ldk_engine_initialize(void);
  LDK_API void ldk_engine_terminate(void);

  LDK_API LDKRoot* ldk_engine(void);
  LDK_API bool ldk_engine_is_initialized(void);
  LDK_API void* ldk_system_get(LDKSystemType system_type);

#ifdef __cplusplus
}
#endif

#endif //LDK_H
