#include <ldk_common.h>
#include <ldk_entity.h>
#include <ldk_component.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_hashtable.h>

#include <string.h>


bool ldk_component_registry_initialize(LDKComponentRegistry* registry)
{
  if (!registry)
  {
    return false;
  }

  memset(registry, 0, sizeof(*registry));

  registry->table = x_hashtable_u32_registered_component_create();

  return registry->table != NULL;
}

void ldk_component_registry_terminate(LDKComponentRegistry* registry)
{
  XHashtableIter it;
  void* key;
  void* value;

  if (!registry || !registry->table)
  {
    return;
  }

  if (x_hashtable_iter_begin((XHashtable*)registry->table, &it))
  {
    while (x_hashtable_iter_next(&it, &key, &value))
    {
      LDKRegisteredComponent* comp = (LDKRegisteredComponent*)value;

      if (comp->store)
      {
        x_array_destroy(comp->store);
      }

      if (comp->owners)
      {
        x_array_destroy(comp->owners);
      }
    }
  }

  x_hashtable_u32_registered_component_destroy(registry->table);
  memset(registry, 0, sizeof(*registry));
}

bool ldk_component_register(
    LDKComponentRegistry* registry,
    const char* name,
    u32 type,
    u32 entry_size,
    u32 initial_capacity)
{
  LDKRegisteredComponent entry;
  XArray* owners;
  XArray* store;

  if (!registry || !registry->table)
  {
    return false;
  }

  if (!type || !entry_size)
  {
    return false;
  }

  if (x_hashtable_u32_registered_component_has(registry->table, type))
  {
    return false;
  }

  store = x_array_create(entry_size, initial_capacity);
  if (!store)
  {
    return false;
  }

  owners = x_array_create(sizeof(LDKEntity), initial_capacity);
  if (!owners)
  {
    x_array_destroy(store);
    x_array_destroy(owners);
    return false;
  }

  entry.desc.name = name;
  entry.desc.type = type;
  entry.desc.entry_size = entry_size;
  entry.desc.initial_capacity = initial_capacity;
  entry.store = store;
  entry.owners = owners;

  if (!x_hashtable_u32_registered_component_set(registry->table, type, entry))
  {
    x_array_destroy(store);
    x_array_destroy(owners);
    return false;
  }

  return true;
}

bool ldk_component_is_registered(LDKComponentRegistry* registry, u32 type)
{
  if (!registry || !registry->table)
  {
    return false;
  }

  return x_hashtable_u32_registered_component_has(registry->table, type);
}

XArray* ldk_component_get_store(LDKComponentRegistry* registry, u32 type)
{
  LDKRegisteredComponent entry;

  if (!registry || !registry->table)
  {
    return NULL;
  }

  if (!x_hashtable_u32_registered_component_get(registry->table, type, &entry))
  {
    return NULL;
  }

  return entry.store;
}


XArray* ldk_component_get_owners(LDKComponentRegistry* registry, u32 type)
{
  LDKRegisteredComponent entry;

  if (!registry || !registry->table)
  {
    return NULL;
  }

  if (!x_hashtable_u32_registered_component_get(registry->table, type, &entry))
  {
    return NULL;
  }

  return entry.owners;
}

bool ldk_component_remove_entity(
    LDKComponentRegistry* registry,
    LDKEntityRegistry* entity_system,
    LDKEntity entity,
    u32 component_type)
{
    if (!registry)
    {
        return false;
    }

    if (!entity_system)
    {
        return false;
    }

    return ldk_entity_remove_component(
        entity_system,
        registry,
        entity,
        component_type);
}

void ldk_component_registry_remove_all(
    LDKComponentRegistry* registry,
    LDKEntityRegistry* entity_system,
    LDKEntity entity)
{
  LDKEntityInfo* info;

  if (!registry || !entity_system)
  {
    return;
  }

  info = ldk_entity_get_info(entity_system, entity);
  if (!info)
  {
    return;
  }

  while (info->components.component_count > 0)
  {
    u32 last;
    u32 component_type;

    last = (u32)info->components.component_count - 1;
    component_type = info->components.component_type[last];

    if (!ldk_component_remove_entity(registry, entity_system, entity, component_type))
    {
      break;
    }
  }
}

void* ldk_component_create(
    LDKComponentRegistry* module,
    u32 component_type,
    u32* component_index)
{
  LDKRegisteredComponent registered_component = {0};

  if (!module)
  {
    return NULL;
  }

  if (!module->table)
  {
    return NULL;
  }

  if (!component_index)
  {
    return NULL;
  }

  if (!x_hashtable_u32_registered_component_get(
        module->table,
        component_type,
        &registered_component))
  {
    return NULL;
  }

  if (!registered_component.store)
  {
    return NULL;
  }

  if (!registered_component.owners)
  {
    return NULL;
  }

  if (x_array_count(registered_component.store) != x_array_count(registered_component.owners))
  {
    return NULL;
  }

  u32 new_index = (u32)x_array_count(registered_component.store);
  LDKEntity owner = x_handle_null();
  void* component = NULL;

  x_array_push(registered_component.store, NULL);
  x_array_push(registered_component.owners, &owner);
  component = x_array_get(registered_component.store, new_index);

  if (!component)
  {
    x_array_pop(registered_component.owners);
    x_array_pop(registered_component.store);
    return NULL;
  }

  memset(component, 0, registered_component.desc.entry_size);
  *component_index = new_index;
  return component;
}

void* ldk_component_get(
    LDKComponentRegistry* module,
    u32 component_type,
    u32 component_index)
{
  LDKRegisteredComponent registered_component = {0};

  if (!module)
  {
    return NULL;
  }

  if (!module->table)
  {
    return NULL;
  }

  if (!x_hashtable_u32_registered_component_get(
        module->table,
        component_type,
        &registered_component))
  {
    return NULL;
  }

  if (!registered_component.store)
  {
    return NULL;
  }

  if (component_index >= (u32)x_array_count(registered_component.store))
  {
    return NULL;
  }

  return x_array_get(registered_component.store, component_index);
}

bool ldk_component_destroy(
    LDKComponentRegistry* module,
    LDKEntityRegistry* entity_module,
    u32 component_type,
    u32 component_index)
{
    LDKRegisteredComponent registered_component = {0};

    (void)entity_module;

    if (!module)
    {
        return false;
    }

    if (!module->table)
    {
        return false;
    }

    if (!x_hashtable_u32_registered_component_get(
            module->table,
            component_type,
            &registered_component))
    {
        return false;
    }

    if (!registered_component.store)
    {
        return false;
    }

    if (!registered_component.owners)
    {
        return false;
    }

    if (x_array_count(registered_component.store) != x_array_count(registered_component.owners))
    {
        return false;
    }

    if (component_index >= (u32)x_array_count(registered_component.store))
    {
        return false;
    }

    {
        u32 last_index = (u32)x_array_count(registered_component.store) - 1;

        if (component_index != last_index)
        {
            void* dst_component = x_array_get(registered_component.store, component_index);
            void* src_component = x_array_get(registered_component.store, last_index);
            void* dst_owner = x_array_get(registered_component.owners, component_index);
            void* src_owner = x_array_get(registered_component.owners, last_index);

            if (!dst_component || !src_component || !dst_owner || !src_owner)
            {
                return false;
            }

            memcpy(
                dst_component,
                src_component,
                registered_component.desc.entry_size);

            memcpy(
                dst_owner,
                src_owner,
                sizeof(LDKEntity));
        }

        x_array_pop(registered_component.store);
        x_array_pop(registered_component.owners);
    }

    return true;
}
