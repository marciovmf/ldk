#include <ldk_common.h>
#include <ldk_entity.h>
#include <ldk_component.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_hashtable.h>

#include <string.h>

static LDKEntity* s_ldk_component_entry_entity(void* entry)
{
  return (LDKEntity*)entry;
}

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

  entry.desc.name = name;
  entry.desc.type = type;
  entry.desc.entry_size = entry_size;
  entry.desc.initial_capacity = initial_capacity;
  entry.store = store;

  if (!x_hashtable_u32_registered_component_set(registry->table, type, entry))
  {
    x_array_destroy(store);
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

bool ldk_component_remove_entity(
    LDKComponentRegistry* registry,
    LDKEntityRegistry* entity_system,
    LDKEntity entity,
    u32 component_type)
{
  LDKRegisteredComponent registered_component;
  XArray* store;
  void* slot_ptr;
  void* last_ptr;
  u32 component_index;
  u32 last_index;
  LDKEntity moved_entity;

  if (!registry || !registry->table || !entity_system)
  {
    return false;
  }

  if (!ldk_entity_find_component(entity_system, entity, component_type, NULL, &component_index))
  {
    return false;
  }

  if (!x_hashtable_u32_registered_component_get(
        registry->table,
        component_type,
        &registered_component))
  {
    return false;
  }

  store = registered_component.store;

  if (!store)
  {
    return false;
  }

  if (component_index >= x_array_count(store))
  {
    return false;
  }

  last_index = x_array_count(store) - 1;
  slot_ptr = x_array_get(store, component_index);
  last_ptr = x_array_get(store, last_index);

  if (!slot_ptr || !last_ptr)
  {
    return false;
  }

  if (component_index != last_index)
  {
    moved_entity = *s_ldk_component_entry_entity(last_ptr);
    memcpy(slot_ptr, last_ptr, registered_component.desc.entry_size);
    ldk_entity_update_component_ref(entity_system, moved_entity, component_type, component_index);
  }

  x_array_pop(store);
  ldk_entity_remove_component_ref(entity_system, entity, component_type);

  return true;
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
