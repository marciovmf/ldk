
#define X_IMPL_ARRAY
#include <stdx/stdx_array.h>

#define X_IMPL_STRING
#include <stdx/stdx_string.h>

#define X_IMPL_FILESYSTEM
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
#include <ldk_os.h>

struct LDKRoot
{
  LDKEntitySystem       entity_system;
  LDKComponentRegistry  component_registry;
  LDKEventQueue         event_queue;
  XLogger               logger;
};

static LDKRoot g_engine;
static bool g_engine_initialized = false;

LDKRoot* ldk_engine(void)
{
  return &g_engine;
}

bool ldk_engine_is_initialized(void)
{
  return g_engine_initialized;
}

void* ldk_system_get(LDKSystemType system_type)
{
  X_ASSERT(g_engine_initialized);
  switch (system_type)
  {
    case LDK_SYSTEM_ENTITY:
      return &g_engine.entity_system;

    case LDK_SYSTEM_COMPONENT:
      return &g_engine.component_registry;

    case LDK_SYSTEM_EVENT:
      return &g_engine.event_queue;

    case LDK_SYSTEM_LOG:
      return &g_engine.logger;

    case LDK_SYSTEM_NONE:
    default:
      break;
  }

  return NULL;
}

bool ldk_engine_initialize(void)
{
  if (g_engine_initialized)
    return true;

  LDKRoot* e = &g_engine;
  memset(e, 0, sizeof(*e));

  ldk_os_initialize();

  x_log_init(&e->logger, XLOG_OUTPUT_BOTH, XLOG_LEVEL_DEBUG, "ldk.log");

  if (!ldk_entity_system_initialize(&e->entity_system, 1024, 1))
    return false;

  if (!ldk_component_registry_initialize(&e->component_registry))
  {
    ldk_entity_system_terminate(&e->entity_system);
    memset(e, 0, sizeof(*e));
    return false;
  }

  if (!ldk_event_queue_initialize(&e->event_queue))
  {
    ldk_component_registry_terminate(&e->component_registry);
    ldk_entity_system_terminate(&e->entity_system);
    memset(e, 0, sizeof(*e));
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

  ldk_component_registry_terminate(&e->component_registry);
  ldk_entity_system_terminate(&e->entity_system);
  ldk_event_queue_terminate(&e->event_queue);
  ldk_os_terminate();
  x_log_close(&e->logger);

  memset(e, 0, sizeof(*e));
  g_engine_initialized = false;
}

