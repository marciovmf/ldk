#include <module/ldk_entity.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <stdx/stdx_hpool.h>
#include <stdx/stdx_array.h>
#include <string.h>


#ifdef LDK_ENGINE

static void s_entity_ctor(void* user, void* item)
{
  (void)user;
  LDKEntityInfo* info = (LDKEntityInfo*)item;
  memset(info, 0, sizeof(*info));
  info->transform_index = LDK_ENTITY_INVALID_COMPONENT_INDEX;
}

static bool s_entity_component_ref_add(LDKEntityRegistry* module, LDKEntity entity,
    u32 component_type, u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);
  u32 count = 0;

  if (!info)
  {
    return false;
  }

  if (ldk_entity_component_has(module, entity, component_type))
  {
    return false;
  }

  count = info->components.component_count;

  if (count >= LDK_ENTITY_MAX_COMPONENTS)
  {
    return false;
  }

  info->components.component_type[count] = component_type;
  info->components.component_index[count] = component_index;
  info->components.component_count = (u16)(count + 1);
  info->components.version += 1;

  // For faster entity/transform lookup we keep the transform index in the entityInfo 
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    info->transform_index = component_index;
  }

  return true;
}

static bool s_entity_component_ref_update(LDKEntityRegistry* module, LDKEntity entity,
    u32 component_type, u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);
  u32 slot = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_component_find(module, entity, component_type, &slot, NULL))
  {
    return false;
  }

  info->components.component_index[slot] = component_index;

  // For faster entity/transform lookup we keep the transform index in the entityInfo 
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    info->transform_index = component_index;
  }

  return true;
}

static bool s_entity_component_ref_remove(LDKEntityRegistry* module, LDKEntity entity, u32 component_type)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);
  u32 slot = 0;
  u32 count = 0;
  u32 last = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_component_find(module, entity, component_type, &slot, NULL))
  {
    return false;
  }

  count = info->components.component_count;
  last = count - 1;

  if (slot != last)
  {
    info->components.component_type[slot] =
      info->components.component_type[last];

    info->components.component_index[slot] =
      info->components.component_index[last];
  }

  info->components.component_type[last] = 0;
  info->components.component_index[last] = 0;
  info->components.component_count = (u16)(count - 1);
  info->components.version += 1;

  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    info->transform_index = LDK_ENTITY_INVALID_COMPONENT_INDEX;
  }

  return true;
}

bool ldk_entity_module_initialize(LDKEntityRegistry* module, u32 page_capacity, u32 initial_pages)
{
  XHPoolConfig pool_config = {0};

  if (!module)
  {
    return false;
  }

  memset(module, 0, sizeof(*module));

  pool_config.page_capacity = page_capacity ? page_capacity : 1024;
  pool_config.initial_pages = initial_pages ? initial_pages : 1;

  if (!x_hpool_init(
        &module->pool,
        sizeof(LDKEntityInfo),
        pool_config,
        s_entity_ctor,
        NULL,
        NULL))
  {
    return false;
  }

  return true;
}

void ldk_entity_module_terminate(LDKEntityRegistry* module)
{
  if (!module)
  {
    return;
  }

  x_hpool_term(&module->pool);
  memset(module, 0, sizeof(*module));
}

void ldk_entity_module_clear(LDKEntityRegistry* module)
{
  if (!module)
  {
    return;
  }

  x_hpool_clear(&module->pool);
}

LDKEntity ldk_entity_create(LDKEntityRegistry* module)
{
  if (!module)
  {
    return x_handle_null();
  }

  return x_hpool_alloc(&module->pool);
}

void ldk_entity_destroy(LDKEntityRegistry* module, LDKEntity entity)
{
  if (!module)
  {
    return;
  }

  if (!x_hpool_is_alive(&module->pool, entity))
  {
    return;
  }

  x_hpool_free(&module->pool, entity);
}

bool ldk_entity_is_alive(LDKEntityRegistry* module, LDKEntity entity)
{
  if (!module)
  {
    return false;
  }

  return x_hpool_is_alive(&module->pool, entity) != 0;
}

LDKEntityInfo* ldk_entity_info_get(LDKEntityRegistry* module, LDKEntity entity)
{
  if (!module)
  {
    return NULL;
  }

  return (LDKEntityInfo*)x_hpool_get(&module->pool, entity);
}

const LDKEntityInfo* ldk_entity_info_get_const(LDKEntityRegistry* module, LDKEntity entity)
{
  if (!module)
  {
    return NULL;
  }

  return (const LDKEntityInfo*)x_hpool_get(&module->pool, entity);
}

u32 ldk_entity_alive_count(LDKEntityRegistry* module)
{
  if (!module)
  {
    return 0;
  }

  return x_hpool_alive_count(&module->pool);
}

void ldk_entity_flags_set(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->flags = flags;
}

u16 ldk_entity_flags_get(LDKEntityRegistry* module, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->flags;
}

void ldk_entity_flags_add(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->flags |= flags;
}

void ldk_entity_flags_remove(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->flags &= (u16)~flags;
}

bool ldk_entity_flags_has(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return false;
  }

  return (info->flags & flags) == flags;
}

void ldk_entity_internal_flags_remove(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags &= (u16)~flags;
}

bool ldk_entity_internal_flags_has(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return false;
  }

  return (info->internal_flags & flags) == flags;
}

bool ldk_entity_name_set(LDKEntityRegistry* module, LDKEntity entity, const char* name)
{
#if defined(_DEBUG) || defined(LDK_EDITOR)
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);
  size_t len = 0;

  if (!info)
  {
    return false;
  }

  if (!name)
  {
    info->name[0] = 0;
    return true;
  }

  len = strlen(name);

  if (len >= LDK_ENTITY_NAME_MAX_LEN)
  {
    len = LDK_ENTITY_NAME_MAX_LEN - 1;
  }

  memcpy(info->name, name, len);
  info->name[len] = 0;

  return true;
#else
  (void)module;
  (void)entity;
  (void)name;
  return false;
#endif
}

const char* ldk_entity_name_get(LDKEntityRegistry* module, LDKEntity entity)
{
#if defined(_DEBUG) || defined(LDK_EDITOR)
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return NULL;
  }

  return (const char*)info->name;
#else
  (void)module;
  (void)entity;
  return NULL;
#endif
}

u32 ldk_entity_component_count(LDKEntityRegistry* module, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->components.component_count;
}

bool ldk_entity_component_find(LDKEntityRegistry* module, LDKEntity entity, u32 component_type,
    u32* out_slot, u32* out_component_index)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);
  u32 i = 0;
  u32 count = 0;

  if (!info)
  {
    return false;
  }

  count = info->components.component_count;

  for (i = 0; i < count; ++i)
  {
    if (info->components.component_type[i] == component_type)
    {
      if (out_slot)
      {
        *out_slot = i;
      }

      if (out_component_index)
      {
        *out_component_index = info->components.component_index[i];
      }

      return true;
    }
  }

  return false;
}

bool ldk_entity_component_has(LDKEntityRegistry* module, LDKEntity entity, u32 component_type)
{
  return ldk_entity_component_find(module, entity, component_type, NULL, NULL);
}

LDKTransform* ldk_entity_transform_get(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity)
{
  LDKEntityInfo* info = ldk_entity_info_get(entity_module, entity);

  if (!info || !component_module)
  {
    return NULL;
  }

  if (info->transform_index == LDK_ENTITY_INVALID_COMPONENT_INDEX)
  {
    return NULL;
  }

  return (LDKTransform*)ldk_component_get(component_module, LDK_COMPONENT_TYPE_TRANSFORM, info->transform_index);
}

const LDKTransform* ldk_entity_transform_get_const(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity)
{
  return (const LDKTransform*)ldk_entity_transform_get(entity_module, component_module, entity);
}

bool ldk_entity_component_ref_get(LDKEntityRegistry* module, LDKEntity entity,
    u32 component_type, LDKComponentRef* out_ref)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);
  u32 slot = 0;

  if (!info || !out_ref)
  {
    return false;
  }

  if (!ldk_entity_component_find(module, entity, component_type, &slot, NULL))
  {
    return false;
  }

  out_ref->entity = entity;
  out_ref->version = info->components.version;
  out_ref->slot_index = (u16)slot;

  return true;
}

bool ldk_component_ref_is_valid(LDKEntityRegistry* entity_system, LDKComponentRef ref)
{
  LDKEntityInfo* info = ldk_entity_info_get(entity_system, ref.entity);

  if (!info)
  {
    return false;
  }

  if (ref.version != info->components.version)
  {
    return false;
  }

  return ref.slot_index < info->components.component_count;
}

void* ldk_component_ref_get(LDKEntityRegistry* entity_system,
    struct LDKComponentRegistry* component_registry, LDKComponentRef ref)
{
  LDKEntityInfo* info = ldk_entity_info_get(entity_system, ref.entity);
  XArray* store = NULL;
  u32 component_index = 0;
  u32 component_type = 0;

  if (!info || !component_registry)
  {
    return NULL;
  }

  if (ref.version != info->components.version)
  {
    return NULL;
  }

  if (ref.slot_index >= info->components.component_count)
  {
    return NULL;
  }

  component_type = info->components.component_type[ref.slot_index];
  component_index = info->components.component_index[ref.slot_index];
  store = ldk_component_store_get((LDKComponentRegistry*)component_registry, component_type);

  if (!store || component_index >= x_array_count(store))
  {
    return NULL;
  }

  return x_array_get(store, component_index);
}

const void* ldk_component_ref_get_const(LDKEntityRegistry* entity_system,
    struct LDKComponentRegistry* component_registry, LDKComponentRef ref)
{
  return ldk_component_ref_get(entity_system, component_registry, ref);
}

void ldk_entity_foreach(LDKEntityRegistry* module, LDKEntityIterFn fn, void* user)
{
  XHPoolIter it = {0};
  LDKEntityInfo* info = NULL;
  LDKEntity entity = x_handle_null();

  if (!module)
  {
    return;
  }

  if (!fn)
  {
    return;
  }

  for (info = (LDKEntityInfo*)x_hpool_iter_begin(&module->pool, &it, &entity);
      info;
      info = (LDKEntityInfo*)x_hpool_iter_next(&module->pool, &it, &entity))
  {
    if (!fn(entity, info, user))
    {
      break;
    }
  }
}

typedef struct LDKComponentAddTransaction
{
  u32 added[LDK_ENTITY_MAX_COMPONENTS];
  u32 added_count;
  u32 stack[LDK_ENTITY_MAX_COMPONENTS];
  u32 stack_count;
} LDKComponentAddTransaction;

static bool s_component_add_stack_contains(
    const LDKComponentAddTransaction* transaction, u32 component_type)
{
  if (!transaction)
  {
    return false;
  }

  for (u32 i = 0; i < transaction->stack_count; ++i)
  {
    if (transaction->stack[i] == component_type)
    {
      return true;
    }
  }
  return false;
}

static void* s_entity_component_add_internal(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity,
    u32 component_type, const void* initial_value, bool allow_existing,
    LDKComponentAddTransaction* transaction)
{
  LDKComponentDesc desc = {0};
  u32 component_index = 0;
  void* component = NULL;

  if (!entity_module || !component_module || !transaction)
  {
    return NULL;
  }

  if (ldk_entity_component_has(entity_module, entity, component_type))
  {
    return allow_existing
        ? ldk_entity_component_get(
              entity_module, component_module, entity, component_type)
        : NULL;
  }

  if (!ldk_component_desc_get(component_module, component_type, &desc))
  {
    ldk_log_error("Component type %u is not registered.\n", component_type);
    return NULL;
  }

  if (s_component_add_stack_contains(transaction, component_type) ||
      transaction->stack_count >= LDK_ENTITY_MAX_COMPONENTS)
  {
    ldk_log_error("Component dependency cycle detected while adding %s.\n",
        desc.name ? desc.name : "<unnamed component>");
    return NULL;
  }

  transaction->stack[transaction->stack_count++] = component_type;
  for (u32 i = 0; i < desc.required_component_count; ++i)
  {
    u32 required_type = desc.required_components[i];
    if (!required_type)
    {
      ldk_log_error("Component %s has an invalid required component.\n",
          desc.name ? desc.name : "<unnamed component>");
      --transaction->stack_count;
      return NULL;
    }

    if (!ldk_entity_component_has(entity_module, entity, required_type) &&
        !s_entity_component_add_internal(entity_module, component_module,
            entity, required_type, NULL, true, transaction))
    {
      LDKComponentDesc required_desc = {0};
      const char* required_name = "<unregistered component>";
      if (ldk_component_desc_get(
              component_module, required_type, &required_desc) &&
          required_desc.name)
      {
        required_name = required_desc.name;
      }
      ldk_log_error("Failed to add required component %s for %s.\n",
          required_name, desc.name ? desc.name : "<unnamed component>");
      --transaction->stack_count;
      return NULL;
    }
  }
  --transaction->stack_count;

  component = ldk_component_create(
      component_module, component_type, &component_index);
  if (!component)
  {
    return NULL;
  }

  if (!s_entity_component_ref_add(
        entity_module, entity, component_type, component_index))
  {
    ldk_component_destroy(
        component_module, entity_module, component_type, component_index);
    return NULL;
  }

  {
    XArray* owners = ldk_component_owners_get(component_module, component_type);
    LDKEntity* owner = NULL;

    if (!owners)
    {
      s_entity_component_ref_remove(entity_module, entity, component_type);
      ldk_component_destroy(
          component_module, entity_module, component_type, component_index);
      return NULL;
    }

    owner = (LDKEntity*)x_array_get(owners, component_index);
    if (!owner)
    {
      s_entity_component_ref_remove(entity_module, entity, component_type);
      ldk_component_destroy(
          component_module, entity_module, component_type, component_index);
      return NULL;
    }

    *owner = entity;
  }

  if (!ldk_component_attach(component_module, entity_module, entity,
          component_type, component_index, initial_value))
  {
    s_entity_component_ref_remove(entity_module, entity, component_type);
    ldk_component_destroy(
        component_module, entity_module, component_type, component_index);
    return NULL;
  }

  if (transaction->added_count >= LDK_ENTITY_MAX_COMPONENTS)
  {
    ldk_entity_component_remove(
        entity_module, component_module, entity, component_type);
    return NULL;
  }

  transaction->added[transaction->added_count++] = component_type;
  return component;
}

void* ldk_entity_component_add(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity,
    u32 component_type, const void* initial_value)
{
  LDKComponentAddTransaction transaction = {0};
  void* component = s_entity_component_add_internal(entity_module,
      component_module, entity, component_type, initial_value, false,
      &transaction);

  if (component)
  {
    return component;
  }

  while (transaction.added_count > 0u)
  {
    u32 added_type = transaction.added[--transaction.added_count];
    if (!ldk_entity_component_remove(
            entity_module, component_module, entity, added_type))
    {
      LDKComponentDesc desc = {0};
      (void)ldk_component_desc_get(component_module, added_type, &desc);
      ldk_log_error("Failed to roll back required component %s.\n",
          desc.name ? desc.name : "<unnamed component>");
    }
  }

  return NULL;
}

void* ldk_entity_component_get(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity, u32 component_type)
{
  u32 component_index = 0;

  if (!entity_module)
  {
    return NULL;
  }

  if (!component_module)
  {
    return NULL;
  }

  if (!ldk_entity_component_find(
        entity_module,
        entity,
        component_type,
        NULL,
        &component_index))
  {
    return NULL;
  }

  return ldk_component_get(
      component_module,
      component_type,
      component_index);
}

bool ldk_entity_component_remove(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity, u32 component_type)
{
  XArray* owners = NULL;
  XArray* store = NULL;
  u32 component_index = 0;
  u32 last_index = 0;
  LDKEntity moved_entity = x_handle_null();
  bool had_move = false;

  if (!entity_module)
  {
    return false;
  }

  if (!component_module)
  {
    return false;
  }

  if (!ldk_entity_component_find(
        entity_module,
        entity,
        component_type,
        NULL,
        &component_index))
  {
    return false;
  }

  store = ldk_component_store_get(component_module, component_type);
  owners = ldk_component_owners_get(component_module, component_type);

  if (!store || !owners)
  {
    return false;
  }

  if (x_array_count(store) != x_array_count(owners))
  {
    return false;
  }

  if (component_index >= (u32)x_array_count(store))
  {
    return false;
  }

  last_index = (u32)x_array_count(store) - 1;

  if (component_index != last_index)
  {
    LDKEntity* moved_owner = (LDKEntity*)x_array_get(owners, last_index);

    if (!moved_owner)
    {
      return false;
    }

    moved_entity = *moved_owner;
    had_move = true;
  }

  ldk_component_destroy_data(
      component_module,
      entity_module,
      entity,
      component_type,
      component_index);

  if (!ldk_component_destroy(
        component_module,
        entity_module,
        component_type,
        component_index))
  {
    return false;
  }

  if (had_move)
  {
    if (!s_entity_component_ref_update(
          entity_module,
          moved_entity,
          component_type,
          component_index))
    {
      return false;
    }
  }

  return s_entity_component_ref_remove(
      entity_module,
      entity,
      component_type);
}

void ldk_entity_internal_flags_set(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags = flags;
}

u16 ldk_entity_internal_flags_get(LDKEntityRegistry* module, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_info_get_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->internal_flags;
}

void ldk_entity_internal_flags_add(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_info_get(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags |= flags;
}

LDKEntityIterator ldk_entity_iterator_begin(LDKEntityRegistry* registry)
{
  LDKEntityIterator iterator = {0};
  LDKEntityInfo* info = NULL;

  iterator.registry = registry;
  iterator.current = x_handle_null();
  iterator.has_current = 0;

  if (!registry)
  {
    return iterator;
  }

  info = (LDKEntityInfo*)x_hpool_iter_begin(
      &registry->pool,
      &iterator.iter,
      &iterator.current);

  if (info)
  {
    iterator.has_current = 1;
  }

  return iterator;
}

bool ldk_entity_iterator_next(LDKEntityIterator* iterator, LDKEntity* out_entity)
{
  LDKEntityInfo* info = NULL;

  if (!iterator)
  {
    return false;
  }

  if (!out_entity)
  {
    return false;
  }

  if (!iterator->registry)
  {
    return false;
  }

  if (iterator->has_current)
  {
    *out_entity = iterator->current;
    iterator->has_current = 0;
    return true;
  }

  info = (LDKEntityInfo*)x_hpool_iter_next(
      &iterator->registry->pool,
      &iterator->iter,
      &iterator->current);

  if (!info)
  {
    return false;
  }

  *out_entity = iterator->current;
  return true;
}

void ldk_entity_iterator_end(LDKEntityIterator* iterator)
{
  if (!iterator)
  {
    return;
  }

  memset(iterator, 0, sizeof(*iterator));
}

#endif// LDK_ENGINE
