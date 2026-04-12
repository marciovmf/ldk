/**
 * @file   ldk.h
 * @brief  Engine root and global module access
 *
 * Entry point for the LDK engine. Provides initialization, shutdown,
 * and access to core submodules.
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
  x_log_raw(ldk_module_get(LDK_MODULE_LOG), (out), (level), (fg), (bg), (components), fmt, ##__VA_ARGS__)

#define ldk_log_debug(fmt, ...) \
  x_log_debug(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_info(fmt, ...) \
  x_log_info(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_warning(fmt, ...) \
  x_log_warning(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_error(fmt, ...) \
  x_log_error(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_fatal(fmt, ...) \
  x_log_fatal(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

  typedef struct LDKRoot LDKRoot;

  typedef enum LDKModuleType
  {
    LDK_MODULE_NONE = 0,
    LDK_MODULE_ENTITY,
    LDK_MODULE_COMPONENT,
    LDK_MODULE_SYSTEM,
    LDK_MODULE_EVENT,
    LDK_MODULE_LOG,
  } LDKModuleType;

  LDK_API bool ldk_engine_initialize(void); // initializes the engine
  LDK_API void ldk_engine_terminate(void);  // finalizes the engine
  LDK_API LDKRoot* ldk_root_get(void);      // Returns the global engine root
  LDK_API bool ldk_engine_is_initialized(void); // Checkes if the engine was initialized
  LDK_API void* ldk_module_get(LDKModuleType module_type);  // Returns the conetxt pointer of a given engine module

#ifdef __cplusplus
}
#endif

#endif //LDK_H
