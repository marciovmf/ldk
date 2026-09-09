#include <module/ldk_system.h>
#include <ldk.h>

#ifndef LDK_ALLOC
#include <stdlib.h>
#define LDK_ALLOC(size) malloc(size)
#define LDK_FREE(ptr) free(ptr)
#endif

#include <stdx/stdx_array.h>
#include <string.h>

typedef struct LDKRegisteredSystem
{
  LDKSystemDesc desc;
  void* userdata;
  u32 registration_index;
  u8 is_initialized;
} LDKRegisteredSystem;

X_ARRAY_TYPE(LDKRegisteredSystem);
X_ARRAY_TYPE(u32);

typedef struct LDKSystemRegistryInternal
{
  XArray_LDKRegisteredSystem* systems;
  XArray_u32* buckets[LDK_SYSTEM_BUCKET_COUNT];
  u8 in_callback;
} LDKSystemRegistryInternal;

static int s_system_desc_has_any_callback(const LDKSystemDesc* desc)
{
  if (!desc)
  {
    return 0;
  }

  return desc->callbacks.initialize != NULL ||
         desc->callbacks.terminate != NULL ||
         desc->callbacks.pre_update != NULL ||
         desc->callbacks.update != NULL ||
         desc->callbacks.post_update != NULL ||
         desc->callbacks.render != NULL;
}

static int s_system_desc_has_bucket_callback(const LDKSystemDesc* desc, LDKSystemBucket bucket)
{
  if (!desc)
  {
    return 0;
  }

  switch (bucket)
  {
    case LDK_SYSTEM_BUCKET_PRE_UPDATE:
      return desc->callbacks.pre_update != NULL;
    case LDK_SYSTEM_BUCKET_UPDATE:
      return desc->callbacks.update != NULL;
    case LDK_SYSTEM_BUCKET_POST_UPDATE:
      return desc->callbacks.post_update != NULL;
    case LDK_SYSTEM_BUCKET_RENDER:
      return desc->callbacks.render != NULL;
    default:
      return 0;
  }
}

static i32 s_system_get_bucket_order(const LDKRegisteredSystem* system, LDKSystemBucket bucket)
{
  if (!system)
  {
    return 0;
  }

  switch (bucket)
  {
    case LDK_SYSTEM_BUCKET_PRE_UPDATE:
      return system->desc.pre_update_order;
    case LDK_SYSTEM_BUCKET_UPDATE:
      return system->desc.update_order;
    case LDK_SYSTEM_BUCKET_POST_UPDATE:
      return system->desc.post_update_order;
    case LDK_SYSTEM_BUCKET_RENDER:
      return system->desc.render_order;
    default:
      return 0;
  }
}

static inline LDKSystemRegistryInternal* s_system_registry_internal(LDKSystemRegistry* registry)
{
  return registry ? (LDKSystemRegistryInternal*)registry->internal : NULL;
}

static const LDKSystemRegistryInternal* s_system_registry_internal_const(const LDKSystemRegistry* registry)
{
  return registry ? (const LDKSystemRegistryInternal*)registry->internal : NULL;
}

static const LDKRegisteredSystem* s_system_registry_find_by_id_const(const LDKSystemRegistry* registry, u64 id)
{
  const LDKSystemRegistryInternal* internal = s_system_registry_internal_const(registry);
  u32 i;

  if (!internal || !internal->systems)
  {
    return NULL;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    const LDKRegisteredSystem* system =
        x_array_LDKRegisteredSystem_get(internal->systems, i);
    if (system->desc.id == id)
    {
      return system;
    }
  }

  return NULL;
}

static LDKRegisteredSystem* s_system_registry_find_by_id(LDKSystemRegistry* registry, u64 id)
{
  return (LDKRegisteredSystem*)s_system_registry_find_by_id_const(registry, id);
}

static void s_system_registry_clear_bucket_lists(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  u32 i;

  if (!internal)
  {
    return;
  }

  for (i = 0; i < LDK_SYSTEM_BUCKET_COUNT; ++i)
  {
    x_array_u32_clear(internal->buckets[i]);
  }
}

/* The caller holds the lifecycle guard. */
static void s_system_terminate(LDKRegisteredSystem* system)
{
  if (!system || !system->is_initialized)
  {
    return;
  }

  if (system->desc.callbacks.terminate)
  {
    system->desc.callbacks.terminate(system->userdata);
  }

  system->userdata = NULL;
  system->is_initialized = 0;
}

static void s_system_registry_terminate_started_systems(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  u32 i;

  if (!internal)
  {
    return;
  }

  for (i = x_array_LDKRegisteredSystem_count(internal->systems); i > 0; --i)
  {
    s_system_terminate(x_array_LDKRegisteredSystem_get(internal->systems, i - 1));
  }
}

static void s_system_registry_sort_bucket(LDKSystemRegistry* registry, LDKSystemBucket bucket)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  XArray_u32* bucket_list;
  u32 i;
  u32 j;

  if (!internal || bucket >= LDK_SYSTEM_BUCKET_COUNT)
  {
    return;
  }

  bucket_list = internal->buckets[bucket];
  for (i = 0; i < x_array_u32_count(bucket_list); ++i)
  {
    for (j = i + 1; j < x_array_u32_count(bucket_list); ++j)
    {
      u32* left_index_ptr = x_array_u32_get(bucket_list, i);
      u32* right_index_ptr = x_array_u32_get(bucket_list, j);
      LDKRegisteredSystem* left_system = x_array_LDKRegisteredSystem_get(internal->systems, *left_index_ptr);
      LDKRegisteredSystem* right_system = x_array_LDKRegisteredSystem_get(internal->systems, *right_index_ptr);
      i32 left_order = s_system_get_bucket_order(left_system, bucket);
      i32 right_order = s_system_get_bucket_order(right_system, bucket);
      i32 should_swap = 0;

      if (right_order < left_order)
      {
        should_swap = 1;
      }
      else if (right_order == left_order &&
               right_system->registration_index < left_system->registration_index)
      {
        should_swap = 1;
      }

      if (should_swap)
      {
        u32 tmp = *left_index_ptr;
        *left_index_ptr = *right_index_ptr;
        *right_index_ptr = tmp;
      }
    }
  }
}

static bool s_system_registry_build_bucket_lists(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  u32 system_index;
  u32 bucket_index;

  if (!internal)
  {
    return false;
  }

  s_system_registry_clear_bucket_lists(registry);
  for (system_index = 0; system_index < x_array_LDKRegisteredSystem_count(internal->systems); ++system_index)
  {
    const LDKRegisteredSystem* system =
        x_array_LDKRegisteredSystem_get(internal->systems, system_index);

    for (bucket_index = 0; bucket_index < LDK_SYSTEM_BUCKET_COUNT; ++bucket_index)
    {
      if (!s_system_desc_has_bucket_callback(&system->desc, (LDKSystemBucket)bucket_index))
      {
        continue;
      }

      if (x_array_u32_add(internal->buckets[bucket_index], system_index) != XARRAY_OK)
      {
        s_system_registry_clear_bucket_lists(registry);
        return false;
      }
    }
  }

  for (bucket_index = 0; bucket_index < LDK_SYSTEM_BUCKET_COUNT; ++bucket_index)
  {
    s_system_registry_sort_bucket(registry, (LDKSystemBucket)bucket_index);
  }

  return true;
}

static void s_system_registry_rebuild_registration_indices(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  u32 i;

  if (!internal)
  {
    return;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system = x_array_LDKRegisteredSystem_get(internal->systems, i);
    system->registration_index = i;
  }
}

bool ldk_system_registry_initialize(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return false;
  }

  memset(registry, 0, sizeof(*registry));
  internal = (LDKSystemRegistryInternal*)LDK_ALLOC(sizeof(*internal));
  if (!internal)
  {
    return false;
  }

  memset(internal, 0, sizeof(*internal));
  internal->systems = x_array_LDKRegisteredSystem_create(8);
  if (!internal->systems)
  {
    LDK_FREE(internal);
    return false;
  }

  for (i = 0; i < LDK_SYSTEM_BUCKET_COUNT; ++i)
  {
    internal->buckets[i] = x_array_u32_create(8);
    if (!internal->buckets[i])
    {
      u32 j;
      for (j = 0; j < i; ++j)
      {
        x_array_u32_destroy(internal->buckets[j]);
      }
      x_array_LDKRegisteredSystem_destroy(internal->systems);
      LDK_FREE(internal);
      return false;
    }
  }

  registry->internal = internal;
  registry->root = NULL;
  registry->is_initialized = 1;
  registry->is_started = 0;
  registry->is_paused = 0;
  return true;
}

void ldk_system_registry_terminate(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry || !registry->is_initialized)
  {
    return;
  }

  internal = s_system_registry_internal(registry);
  if (!internal || internal->in_callback)
  {
    return;
  }

  if (registry->is_started)
  {
    if (!ldk_system_registry_stop(registry))
    {
      return;
    }
  }

  for (i = 0; i < LDK_SYSTEM_BUCKET_COUNT; ++i)
  {
    x_array_u32_destroy(internal->buckets[i]);
  }
  x_array_LDKRegisteredSystem_destroy(internal->systems);
  LDK_FREE(internal);
  memset(registry, 0, sizeof(*registry));
}

bool ldk_system_registry_register(LDKSystemRegistry* registry, const LDKSystemDesc* desc)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  LDKRegisteredSystem system;

  if (!registry || !desc || !registry->is_initialized || registry->is_started ||
      !internal || internal->in_callback || desc->id == 0 ||
      !s_system_desc_has_any_callback(desc) ||
      s_system_registry_find_by_id_const(registry, desc->id) != NULL)
  {
    return false;
  }

  memset(&system, 0, sizeof(system));
  system.desc = *desc;
  system.registration_index = x_array_LDKRegisteredSystem_count(internal->systems);
  return x_array_LDKRegisteredSystem_add(internal->systems, system) == XARRAY_OK;
}

bool ldk_system_registry_unregister(LDKSystemRegistry* registry, u64 id)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  u32 i;

  if (!registry || !registry->is_initialized || registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system = x_array_LDKRegisteredSystem_get(internal->systems, i);
    if (system->desc.id == id)
    {
      x_array_LDKRegisteredSystem_delete_at(internal->systems, i);
      s_system_registry_rebuild_registration_indices(registry);
      return true;
    }
  }

  return false;
}

bool ldk_system_registry_find_by_id(LDKSystemRegistry* registry, u64 id, LDKSystemDesc* out)
{
  const LDKRegisteredSystem* system;

  if (!out)
  {
    return false;
  }

  system = s_system_registry_find_by_id_const(registry, id);
  if (!system)
  {
    return false;
  }

  memcpy(out, &system->desc, sizeof(*out));
  return true;
}

u32 ldk_system_registry_count(const LDKSystemRegistry* registry)
{
  const LDKSystemRegistryInternal* internal = s_system_registry_internal_const(registry);
  if (!registry || !registry->is_initialized || !internal)
  {
    return 0;
  }
  return x_array_LDKRegisteredSystem_count(internal->systems);
}

bool ldk_system_registry_at(const LDKSystemRegistry* registry, u32 index, LDKSystemDesc* out)
{
  const LDKSystemRegistryInternal* internal = s_system_registry_internal_const(registry);
  const LDKRegisteredSystem* system;

  if (!registry || !out || !registry->is_initialized || !internal ||
      index >= x_array_LDKRegisteredSystem_count(internal->systems))
  {
    return false;
  }

  system = x_array_LDKRegisteredSystem_get(internal->systems, index);
  memcpy(out, &system->desc, sizeof(*out));
  return true;
}

bool ldk_system_registry_clear(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);

  if (!registry || !registry->is_initialized || registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }

  x_array_LDKRegisteredSystem_clear(internal->systems);
  s_system_registry_clear_bucket_lists(registry);
  return true;
}

bool ldk_system_registry_start(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);

  if (!registry || !registry->is_initialized || registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }

  if (!s_system_registry_build_bucket_lists(registry))
  {
    return false;
  }

  registry->is_started = 1;
  registry->is_paused = 0;
  return true;
}

bool ldk_system_registry_stop(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);

  if (!registry || !registry->is_initialized || !internal || internal->in_callback)
  {
    return false;
  }

  if (!registry->is_started)
  {
    return true;
  }

  internal->in_callback = 1;
  s_system_registry_terminate_started_systems(registry);
  internal->in_callback = 0;
  s_system_registry_clear_bucket_lists(registry);
  registry->is_started = 0;
  registry->is_paused = 0;
  return true;
}

bool ldk_system_registry_system_start(LDKSystemRegistry* registry, u64 id)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  LDKRegisteredSystem* system;
  int result = 0;

  if (!registry || !registry->is_initialized || !registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }

  system = s_system_registry_find_by_id(registry, id);
  if (!system)
  {
    return false;
  }

  if (system->is_initialized)
  {
    return true;
  }

  internal->in_callback = 1;
  system->userdata = NULL;
  if (system->desc.callbacks.initialize)
  {
    result = system->desc.callbacks.initialize(&system->userdata);
  }

  if (result != 0)
  {
    /* A failed initializer must permit its terminator to release partial state. */
    if (system->desc.callbacks.terminate)
    {
      system->desc.callbacks.terminate(system->userdata);
    }
    system->userdata = NULL;
    system->is_initialized = 0;
    internal->in_callback = 0;
    return false;
  }

  system->is_initialized = 1;
  internal->in_callback = 0;
  return true;
}

bool ldk_system_registry_system_stop(LDKSystemRegistry* registry, u64 id)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  LDKRegisteredSystem* system;

  if (!registry || !registry->is_initialized || !registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }

  system = s_system_registry_find_by_id(registry, id);
  if (!system)
  {
    return false;
  }

  internal->in_callback = 1;
  s_system_terminate(system);
  internal->in_callback = 0;
  return true;
}

bool ldk_system_registry_system_is_started(const LDKSystemRegistry* registry, u64 id)
{
  const LDKRegisteredSystem* system = s_system_registry_find_by_id_const(registry, id);
  return registry && registry->is_initialized && registry->is_started &&
         system && system->is_initialized;
}

bool ldk_system_registry_pause(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  if (!registry || !registry->is_initialized || !registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }
  registry->is_paused = 1;
  return true;
}

bool ldk_system_registry_resume(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  if (!registry || !registry->is_initialized || !registry->is_started ||
      !internal || internal->in_callback)
  {
    return false;
  }
  registry->is_paused = 0;
  return true;
}

bool ldk_system_registry_is_paused(const LDKSystemRegistry* registry)
{
  return registry && registry->is_initialized && registry->is_started &&
         registry->is_paused;
}

bool ldk_system_registry_run_bucket(LDKSystemRegistry* registry, LDKSystemBucket bucket, float dt)
{
  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  XArray_u32* bucket_list;
  u32 i;

  if (!registry || !registry->is_initialized || !registry->is_started ||
      !internal || internal->in_callback || bucket >= LDK_SYSTEM_BUCKET_COUNT)
  {
    return false;
  }

  bucket_list = internal->buckets[bucket];
  internal->in_callback = 1;

  for (i = 0; i < x_array_u32_count(bucket_list); ++i)
  {
    u32* system_index_ptr = x_array_u32_get(bucket_list, i);
    LDKRegisteredSystem* system = x_array_LDKRegisteredSystem_get(internal->systems, *system_index_ptr);

    if (!system->is_initialized || !(system->desc.flags & LDK_SYSTEM_FLAG_ENABLED))
    {
      continue;
    }

    if (registry->is_paused &&
        !(system->desc.flags & LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED))
    {
      continue;
    }

    switch (bucket)
    {
      case LDK_SYSTEM_BUCKET_PRE_UPDATE:
        if (system->desc.callbacks.pre_update)
        {
          system->desc.callbacks.pre_update(system->userdata, dt);
        }
        break;
      case LDK_SYSTEM_BUCKET_UPDATE:
        if (system->desc.callbacks.update)
        {
          system->desc.callbacks.update(system->userdata, dt);
        }
        break;
      case LDK_SYSTEM_BUCKET_POST_UPDATE:
        if (system->desc.callbacks.post_update)
        {
          system->desc.callbacks.post_update(system->userdata, dt);
        }
        break;
      case LDK_SYSTEM_BUCKET_RENDER:
        if (system->desc.callbacks.render)
        {
          system->desc.callbacks.render(system->userdata, dt);
        }
        break;
      default:
        internal->in_callback = 0;
        return false;
    }
  }

  internal->in_callback = 0;
  return true;
}

bool ldk_system_registry_has(const LDKSystemRegistry* registry, u64 id)
{
  return s_system_registry_find_by_id_const(registry, id) != NULL;
}
