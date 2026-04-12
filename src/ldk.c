
#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_FILEMODULE
#include <stdx/stdx_filesystem.h>

#define X_IMPL_HASHTABLE
#include <stdx/stdx_hashtable.h>

#define X_IMPL_HPOOL
#include <stdx/stdx_hpool.h>

#define X_IMPL_LOG
#include <stdx/stdx_log.h>

#include <ldk.h>

#include <ldk_entity.h>
#include <ldk_component.h>
#include <ldk_system.h>
#include <ldk_os.h>

struct LDKRoot
{
  LDKEntityRegistry     entity_registry;
  LDKComponentRegistry  component_registry;
  LDKSystemRegistry     system_registry;
  LDKEventQueue         event_queue;
  XLogger               logger;
};

static LDKRoot g_engine;
static bool g_engine_initialized = false;

LDKRoot* ldk_root_get(void)
{
  return &g_engine;
}

bool ldk_engine_is_initialized(void)
{
  return g_engine_initialized;
}

void* ldk_module_get(LDKModuleType module_type)
{
  X_ASSERT(g_engine_initialized);
  switch (module_type)
  {
    case LDK_MODULE_ENTITY:
      return &g_engine.entity_registry;

    case LDK_MODULE_COMPONENT:
      return &g_engine.component_registry;

    case LDK_MODULE_SYSTEM:
      return &g_engine.system_registry;

    case LDK_MODULE_EVENT:
      return &g_engine.event_queue;

    case LDK_MODULE_LOG:
      return &g_engine.logger;

    case LDK_MODULE_NONE:
    default:
      break;
  }

  return NULL;
}

void s_terminate_all_modules(LDKRoot* e)
{
  ldk_component_registry_terminate(&e->component_registry);
  ldk_entity_module_terminate(&e->entity_registry);
  ldk_system_registry_terminate(&e->system_registry);
  ldk_event_queue_terminate(&e->event_queue);
  ldk_os_terminate();
  x_log_close(&e->logger);
}

bool ldk_engine_initialize(void)
{
  if (g_engine_initialized)
    return true;

  LDKRoot* e = &g_engine;
  memset(e, 0, sizeof(*e));

  x_log_init(&e->logger, XLOG_OUTPUT_BOTH, XLOG_LEVEL_DEBUG, "ldk.log");
  x_log_info(&e->logger,"Initializing LDK");
  ldk_os_initialize();

  bool engine_init_failed = false;

  if (!ldk_event_queue_initialize(&e->event_queue))
  {
    ldk_log_error("Failed to initialize module: Event Queue.");
    engine_init_failed = true;
  }

  if (!ldk_entity_module_initialize(&e->entity_registry, 1024, 1))
  {
    ldk_log_error("Failed to initialize module: Entity Registry.");
    engine_init_failed = true;
  }

  if (!ldk_component_registry_initialize(&e->component_registry))
  {
    ldk_log_error("Failed to initialize module: Component Registry.");
    engine_init_failed = true;
  }

  if (!ldk_system_registry_initialize(&e->system_registry))
  {
    ldk_log_error("Failed to initialize module: System Registry.");
    engine_init_failed = true;
  }

  if (engine_init_failed)
  {
    s_terminate_all_modules(e);
    return false;
  }

  g_engine_initialized = true;
  return true;
}

void ldk_engine_terminate(void)
{
  LDKRoot* e = &g_engine;

  if (!g_engine_initialized)
  {
    return;
  }

  s_terminate_all_modules(e);

  memset(e, 0, sizeof(*e));
  g_engine_initialized = false;
}

