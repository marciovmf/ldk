#include <module/ldk_system.h>
#include <module/ldk_ecs.h>
#include <module/ldk_entity.h>
#include <component/ldk_transform.h>
#include <ldk.h>


// ---------------------------------------------------------------------------
// ECS lifecycle
// ---------------------------------------------------------------------------

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
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();
  LDKSystemRegistry* system_registry = ldk_ecs_system_registry_get();

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


// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------

LDKEntity ldk_ecs_entity_create(void)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return x_handle_null();
  }

  LDKEntity entity = ldk_entity_create(entity_registry);

  if (x_handle_is_null(entity))
  {
    return entity;
  }

  // Entities always have a transform component
  if (!ldk_entity_component_add(entity_registry, component_registry,
        entity, LDK_COMPONENT_TYPE_TRANSFORM, NULL))
  {
    ldk_entity_destroy(entity_registry, entity);
    return x_handle_null();
  }

  return entity;
}

void ldk_ecs_entity_destroy(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

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


// ---------------------------------------------------------------------------
// Component management
// ---------------------------------------------------------------------------

void* ldk_ecs_component_add(LDKEntity entity, u32 component_type, const void* initial_value)
{
  //We disallow attaching TRANSFORMS via this facade sice it always attach a
  //transform when creating an entity in ldk_ecs_create_entity()
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    return NULL;
  }

  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return NULL;
  }

  return ldk_entity_component_add(
      entity_registry,
      component_registry,
      entity,
      component_type,
      initial_value);
}

void* ldk_ecs_component_get(LDKEntity entity, u32 component_type)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return NULL;
  }

  return ldk_entity_component_get(
      entity_registry,
      component_registry,
      entity,
      component_type);
}

const void* ldk_ecs_component_get_const(LDKEntity entity, u32 component_type)
{
  return (const void*) ldk_ecs_component_get(entity, component_type);
}

bool ldk_ecs_component_remove(LDKEntity entity, u32 component_type)
{
  // Transform components can not be removed
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    return false;
  }

  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return false;
  }


  return ldk_entity_component_remove(
      entity_registry,
      component_registry,
      entity,
      component_type);
}

bool ldk_ecs_component_register(const LDKComponentDesc* desc)
{
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!component_registry)
  {
    return false;
  }
  return ldk_component_register(component_registry, desc);
}


// ---------------------------------------------------------------------------
// System management
// ---------------------------------------------------------------------------

bool ldk_ecs_system_register(const LDKSystemDesc* desc)
{
  LDKSystemRegistry* system_registry = ldk_ecs_system_registry_get();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_register(system_registry, desc);
}

bool ldk_ecs_system_unregister(u64 id)
{
  LDKSystemRegistry* system_registry = ldk_ecs_system_registry_get();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_unregister(system_registry, id);
}


// ---------------------------------------------------------------------------
//  Engine internal utility
// ---------------------------------------------------------------------------

#ifdef LDK_ENGINE

LDKEntityRegistry* ldk_ecs_entity_registry_get(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->entity;
}

LDKComponentRegistry* ldk_ecs_component_registry_get(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->component;
}

LDKSystemRegistry* ldk_ecs_system_registry_get(void)
{
  LDKECS* ecs = (LDKECS*)ldk_module_get(LDK_MODULE_ECS);
  return &ecs->system;
}

bool ldk_ecs_system_registry_start(LDKECS* context)
{
  return ldk_system_registry_start(&context->system);
}

bool ldk_ecs_system_registry_stop(LDKECS* context)
{
  return ldk_system_registry_stop(&context->system);
}

bool ldk_ecs_system_bucket_run(LDKECS* context, LDKSystemBucket bucket, float dt)
{
  if (!context || !context->system.is_started)
  {
    return false;
  }

  return ldk_system_registry_run_bucket(&context->system, bucket, dt);
}

#endif
