#include <ldk_entity.h>
#include <stdx/stdx_hpool.h>
#include <string.h>


static void s_ldk_entity_ctor(void* user, void* item)
{
  (void)user;

  memset(item, 0, sizeof(LDKEntityInfo));
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
        s_ldk_entity_ctor,
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

LDKEntityInfo* ldk_entity_get_info(LDKEntityRegistry* module, LDKEntity entity)
{
  if (!module)
  {
    return NULL;
  }

  return (LDKEntityInfo*)x_hpool_get(&module->pool, entity);
}

const LDKEntityInfo* ldk_entity_get_info_const(LDKEntityRegistry* module, LDKEntity entity)
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

void ldk_entity_set_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->flags = flags;
}

u16 ldk_entity_get_flags(LDKEntityRegistry* module, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->flags;
}

void ldk_entity_add_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->flags |= flags;
}

void ldk_entity_remove_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->flags &= (u16)~flags;
}

bool ldk_entity_has_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

  if (!info)
  {
    return false;
  }

  return (info->flags & flags) == flags;
}

void ldk_entity_set_internal_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags = flags;
}

u16 ldk_entity_get_internal_flags(LDKEntityRegistry* module, LDKEntity entity)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->internal_flags;
}

void ldk_entity_add_internal_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags |= flags;
}

void ldk_entity_remove_internal_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);

  if (!info)
  {
    return;
  }

  info->internal_flags &= (u16)~flags;
}

bool ldk_entity_has_internal_flags(LDKEntityRegistry* module, LDKEntity entity, u16 flags)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

  if (!info)
  {
    return false;
  }

  return (info->internal_flags & flags) == flags;
}

bool ldk_entity_set_name(LDKEntityRegistry* module, LDKEntity entity, const char* name)
{
#ifdef _DEBUG
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);
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

const char* ldk_entity_get_name(LDKEntityRegistry* module, LDKEntity entity)
{
#ifdef _DEBUG
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

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
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);

  if (!info)
  {
    return 0;
  }

  return info->components.component_count;
}

bool ldk_entity_find_component(
    LDKEntityRegistry* module,
    LDKEntity entity,
    u32 component_type,
    u32* out_slot,
    u32* out_component_index)
{
  const LDKEntityInfo* info = ldk_entity_get_info_const(module, entity);
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
    LDKEntityRegistry* module,
    LDKEntity entity,
    u32 component_type)
{
  return ldk_entity_find_component(module, entity, component_type, NULL, NULL);
}

bool ldk_entity_add_component_ref(
    LDKEntityRegistry* module,
    LDKEntity entity,
    u32 component_type,
    u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);
  u32 count = 0;

  if (!info)
  {
    return false;
  }

  if (ldk_entity_has_component(module, entity, component_type))
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
    LDKEntityRegistry* module,
    LDKEntity entity,
    u32 component_type,
    u32 component_index)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);
  u32 slot = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_find_component(module, entity, component_type, &slot, NULL))
  {
    return false;
  }

  info->components.component_index[slot] = component_index;

  return true;
}

bool ldk_entity_remove_component_ref(
    LDKEntityRegistry* module,
    LDKEntity entity,
    u32 component_type)
{
  LDKEntityInfo* info = ldk_entity_get_info(module, entity);
  u32 slot = 0;
  u32 count = 0;
  u32 last = 0;

  if (!info)
  {
    return false;
  }

  if (!ldk_entity_find_component(module, entity, component_type, &slot, NULL))
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
    LDKEntityRegistry* module,
    LDKEntityIterFn fn,
    void* user)
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
