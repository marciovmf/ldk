#include "module/ldk_system.h"
#include <module/ldk_ecs.h>
#include <ldk.h>

static LDKEntityRegistry* s_ecs_entity_registry(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->entity;
}

static LDKComponentRegistry* s_ecs_component_registry(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->component;
}

static LDKSystemRegistry* s_ecs_system_registry(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->system;
}

LDKEntityRegistry* ldk_ecs_entity_registry(void)
{
  return s_ecs_entity_registry();
}

LDKComponentRegistry* ldk_ecs_component_registry(void)
{
  return s_ecs_component_registry();
}

LDKSystemRegistry* ldk_ecs_system_registry(void)
{
  return s_ecs_system_registry();
}

bool ldk_ecs_initialize(LDKECS* context, u32 entity_page_capacity, u32 entity_initial_pages)
{
  if (!context)
  {
    return false;
  }

  LDKEntityRegistry* entity_registry = &context->entity;
  LDKComponentRegistry* component_registry = &context->component;
  LDKSystemRegistry* system_registry = &context->system;

  if (!entity_registry || !component_registry || !system_registry)
  {
    return false;
  }

  if (!ldk_entity_module_initialize(entity_registry, entity_page_capacity, entity_initial_pages))
  {
    return false;
  }

  if (!ldk_component_registry_initialize(component_registry))
  {
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  if (!ldk_system_registry_initialize(system_registry))
  {
    ldk_component_registry_terminate(component_registry);
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  return true;
}

void ldk_ecs_terminate(void)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();
  LDKComponentRegistry* component_registry = s_ecs_component_registry();
  LDKSystemRegistry* system_registry = s_ecs_system_registry();

  if (system_registry)
  {
    ldk_system_registry_terminate(system_registry);
  }

  if (component_registry)
  {
    ldk_component_registry_terminate(component_registry);
  }

  if (entity_registry)
  {
    ldk_entity_module_terminate(entity_registry);
  }
}

LDKEntity ldk_ecs_create_entity(void)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();

  if (!entity_registry)
  {
    return x_handle_null();
  }

  return ldk_entity_create(entity_registry);
}

void ldk_ecs_destroy_entity(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();
  LDKComponentRegistry* component_registry = s_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return;
  }

  if (!ldk_entity_is_alive(entity_registry, entity))
  {
    return;
  }

  ldk_component_registry_remove_all(component_registry, entity_registry, entity);
  ldk_entity_destroy(entity_registry, entity);
}

void* ldk_ecs_add_component(LDKEntity entity, u32 component_type, const void* initial_value)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();
  LDKComponentRegistry* component_registry = s_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return NULL;
  }

  return ldk_entity_add_component(
      entity_registry,
      component_registry,
      entity,
      component_type,
      initial_value);
}

void* ldk_ecs_get_component(LDKEntity entity, u32 component_type)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();
  LDKComponentRegistry* component_registry = s_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return NULL;
  }

  return ldk_entity_get_component(
      entity_registry,
      component_registry,
      entity,
      component_type);
}


const void* ldk_ecs_get_component_const(LDKEntity entity, u32 component_type)
{
  return (const void*) ldk_ecs_get_component(entity, component_type);
}

bool ldk_ecs_remove_component(LDKEntity entity, u32 component_type)
{
  LDKEntityRegistry* entity_registry = s_ecs_entity_registry();
  LDKComponentRegistry* component_registry = s_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  return ldk_entity_remove_component(
      entity_registry,
      component_registry,
      entity,
      component_type);
}

bool ldk_ecs_register_component(const char* name, u32 type, u32 entry_size,
    u32 initial_capacity, LDKComponentAttachFn attach, LDKComponentDestroyFn destroy, void* user)
{
  LDKComponentRegistry* component_registry = s_ecs_component_registry();

  if (!component_registry)
  {
    return false;
  }

  return ldk_component_register(
      component_registry,
      name,
      type,
      entry_size,
      initial_capacity,
      attach,
      destroy,
      user);
}

bool ldk_ecs_register_system(const LDKSystemDesc* desc)
{
  LDKSystemRegistry* system_registry = s_ecs_system_registry();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_register(system_registry, desc);
}

bool ldk_ecs_unregister_system(u64 id)
{
  LDKSystemRegistry* system_registry = s_ecs_system_registry();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_unregister(system_registry, id);
}

#ifdef LDK_ENGINE
bool ldk_ecs_system_registry_start(LDKECS* context)
{
  return ldk_system_registry_start(&context->system);
}
#endif

#ifdef LDK_ENGINE
bool ldk_ecs_system_registry_stop(LDKECS* context)
{
  return ldk_system_registry_stop(&context->system);
}
#endif

#ifdef LDK_ENGINE
bool ldk_ecs_run_system_bucket(LDKECS* context, LDKSystemBucket bucket, float dt)
{
  if (!context || !context->system.is_started)
  {
    return false;
  }

  return ldk_system_registry_run_bucket(&context->system, bucket, dt);
}
#endif

