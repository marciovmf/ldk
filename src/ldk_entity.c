#include <ldk_entity.h>
#include <stdx/stdx_hpool.h>
#include <string.h>


static void s_ldk_entity_ctor(void* user, void* item)
{
  (void)user;

  memset(item, 0, sizeof(LDKEntityInfo));
}

bool ldk_entity_system_initialize(LDKEntitySystem* system, u32 page_capacity, u32 initial_pages)
{
  XHPoolConfig pool_config = {0};

  if (!system)
  {
    return false;
  }

  memset(system, 0, sizeof(*system));

  pool_config.page_capacity = page_capacity ? page_capacity : 1024;
  pool_config.initial_pages = initial_pages ? initial_pages : 1;

  if (!x_hpool_init(
        &system->pool,
        sizeof(LDKEntityInfo),
        pool_config,
        s_ldk_entity_ctor,
        NULL,
        NULL))
  {
    return false;
  }

  return true;
}

void ldk_entity_system_terminate(LDKEntitySystem* system)
{
  if (!system)
  {
    return;
  }

  x_hpool_term(&system->pool);
  memset(system, 0, sizeof(*system));
}

void ldk_entity_system_clear(LDKEntitySystem* system)
{
  if (!system)
  {
    return;
  }

  x_hpool_clear(&system->pool);
}

LDKEntity ldk_entity_create(LDKEntitySystem* system)
{
  if (!system)
  {
    return x_handle_null();
  }

  return x_hpool_alloc(&system->pool);
}

void ldk_entity_destroy(LDKEntitySystem* system, LDKEntity entity)
{
  if (!system)
  {
    return;
  }

  if (!x_hpool_is_alive(&system->pool, entity))
  {
    return;
  }

  x_hpool_free(&system->pool, entity);
}

bool ldk_entity_is_alive(LDKEntitySystem* system, LDKEntity entity)
{
  if (!system)
  {
    return false;
  }

  return x_hpool_is_alive(&system->pool, entity) != 0;
}

LDKEntityInfo* ldk_entity_get_info(LDKEntitySystem* system, LDKEntity entity)
{
  if (!system)
  {
    return NULL;
  }

  return (LDKEntityInfo*)x_hpool_get(&system->pool, entity);
}

const LDKEntityInfo* ldk_entity_get_info_const(LDKEntitySystem* system, LDKEntity entity)
{
  if (!system)
  {
    return NULL;
  }

  return (const LDKEntityInfo*)x_hpool_get(&system->pool, entity);
}

u32 ldk_entity_alive_count(LDKEntitySystem* system)
{
  if (!system)
  {
    return 0;
  }

  return x_hpool_alive_count(&system->pool);
}

void ldk_entity_set_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->flags = flags;
}

u16 ldk_entity_get_flags(LDKEntitySystem* system, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return 0;
  }

  return info->flags;
}

void ldk_entity_add_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->flags |= flags;
}

void ldk_entity_remove_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->flags &= (u16)~flags;
}

bool ldk_entity_has_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return false;
  }

  return (info->flags & flags) == flags;
}

void ldk_entity_set_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags = flags;
}

u16 ldk_entity_get_internal_flags(LDKEntitySystem* system, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return 0;
  }

  return info->internal_flags;
}

void ldk_entity_add_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags |= flags;
}

void ldk_entity_remove_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags &= (u16)~flags;
}

bool ldk_entity_has_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return false;
  }

  return (info->internal_flags & flags) == flags;
}

bool ldk_entity_set_name(LDKEntitySystem* system, LDKEntity entity, const char* name)
{
#ifdef _DEBUG
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);
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
  (void)system;
  (void)entity;
  (void)name;
  return false;
#endif
}

const char* ldk_entity_get_name(LDKEntitySystem* system, LDKEntity entity)
{
#ifdef _DEBUG
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return NULL;
  }

  return (const char*)info->name;
#else
  (void)system;
  (void)entity;
  return NULL;
#endif
}

u32 ldk_entity_component_count(LDKEntitySystem* system, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);

  if (!info)
  {
    return 0;
  }

  return info->components.component_count;
}

bool ldk_entity_find_component(
    LDKEntitySystem* system,
    LDKEntity entity,
    u32 component_type,
    u32* out_slot,
    u32* out_component_index)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(system, entity);
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

bool ldk_entity_has_component(
    LDKEntitySystem* system,
    LDKEntity entity,
    u32 component_type)
{
  return ldk_entity_find_component(system, entity, component_type, NULL, NULL);
}

bool ldk_entity_add_component_ref(
    LDKEntitySystem* system,
    LDKEntity entity,
    u32 component_type,
    u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);
  u32 count = 0;

  if (!info)
  {
    return false;
  }

  if (ldk_entity_has_component(system, entity, component_type))
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
  info->components.component_count = (u8)(count + 1);

  return true;
}

bool ldk_entity_update_component_ref(
    LDKEntitySystem* system,
    LDKEntity entity,
    u32 component_type,
    u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);
  u32 slot = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_find_component(system, entity, component_type, &slot, NULL))
  {
    return false;
  }

  info->components.component_index[slot] = component_index;

  return true;
}

bool ldk_entity_remove_component_ref(
    LDKEntitySystem* system,
    LDKEntity entity,
    u32 component_type)
{
  LDKEntityInfo* info = ldk_entity_get_info(system, entity);
  u32 slot = 0;
  u32 count = 0;
  u32 last = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_find_component(system, entity, component_type, &slot, NULL))
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
  info->components.component_count = (u8)(count - 1);

  return true;
}

void ldk_entity_foreach(
    LDKEntitySystem* system,
    LDKEntityIterFn fn,
    void* user)
{
  XHPoolIter it = {0};
  LDKEntityInfo* info = NULL;
  LDKEntity entity = x_handle_null();

  if (!system)
  {
    return;
  }

  if (!fn)
  {
    return;
  }

  for (info = (LDKEntityInfo*)x_hpool_iter_begin(&system->pool, &it, &entity);
      info;
      info = (LDKEntityInfo*)x_hpool_iter_next(&system->pool, &it, &entity))
  {
    if (!fn(entity, info, user))
    {
      break;
    }
  }
}
