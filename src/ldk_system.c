#include <ldk_system.h>
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

typedef struct LDKSystemBucketList
{
  u32* system_indices;
  u32 count;
  u32 capacity;
} LDKSystemBucketList;

X_ARRAY_TYPE(LDKRegisteredSystem);
X_ARRAY_TYPE(u32);

typedef struct LDKSystemRegistryInternal
{
  XArray_LDKRegisteredSystem* systems;
  XArray_u32* buckets[LDK_SYSTEM_BUCKET_COUNT];
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
  return (LDKSystemRegistryInternal*)registry->internal;
}

static const LDKSystemRegistryInternal* s_system_registry_internal_const(const LDKSystemRegistry* registry)
{
  return (const LDKSystemRegistryInternal*)registry->internal;
}

static const LDKRegisteredSystem* s_system_registry_find_by_id_const(const LDKSystemRegistry* registry, u64 id)
{
  const LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return NULL;
  }

  internal = s_system_registry_internal_const(registry);
  if (!internal)
  {
    return NULL;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    const LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i);
    if (system->desc.id == id)
    {
      return system;
    }
  }

  return NULL;
}

static void s_system_registry_clear_bucket_lists(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return;
  }

  for (i = 0; i < LDK_SYSTEM_BUCKET_COUNT; ++i)
  {
    x_array_u32_clear(internal->buckets[i]);
  }
}

static void s_system_registry_terminate_started_systems(LDKSystemRegistry* registry)
{
  LDKRoot* engine;
  u32 count;
  u32 i;

  if (!registry)
  {
    return;
  }

  LDKSystemRegistryInternal* internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return;
  }

  engine = ldk_root_get();
  count = x_array_LDKRegisteredSystem_count(internal->systems);

  for (i = count; i > 0; --i)
  {
    LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i - 1);

    if (!system->is_initialized)
    {
      continue;
    }

    if (system->desc.callbacks.terminate)
    {
      system->desc.callbacks.terminate(engine, system->userdata);
    }

    system->userdata = NULL;
    system->is_initialized = 0;
  }
}

static void s_system_registry_sort_bucket(LDKSystemRegistry* registry, LDKSystemBucket bucket)
{
  LDKSystemRegistryInternal* internal;
  XArray_u32* bucket_list;
  u32 i;
  u32 j;

  if (!registry || bucket >= LDK_SYSTEM_BUCKET_COUNT)
  {
    return;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return;
  }

  bucket_list = internal->buckets[bucket];

  // Just a dirty bubble-sort-ish sort here. Should be fine because nubmer of elements is very likely to be tiny.
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
        u32 tmp;

        tmp = *left_index_ptr;
        *left_index_ptr = *right_index_ptr;
        *right_index_ptr = tmp;
      }
    }
  }
}

static int s_system_registry_build_bucket_lists(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 system_index;
  u32 bucket_index;

  if (!registry)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return 1;
  }

  s_system_registry_clear_bucket_lists(registry);

  for (system_index = 0; system_index < x_array_LDKRegisteredSystem_count(internal->systems); ++system_index)
  {
    const LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, system_index);

    for (bucket_index = 0; bucket_index < LDK_SYSTEM_BUCKET_COUNT; ++bucket_index)
    {
      XArrayError result;

      if (!s_system_desc_has_bucket_callback(&system->desc, (LDKSystemBucket)bucket_index))
      {
        continue;
      }

      result = x_array_u32_push(internal->buckets[bucket_index], system_index);
      if (result != XARRAY_OK)
      {
        return 1;
      }
    }
  }

  for (bucket_index = 0; bucket_index < LDK_SYSTEM_BUCKET_COUNT; ++bucket_index)
  {
    s_system_registry_sort_bucket(registry, (LDKSystemBucket)bucket_index);
  }

  return 0;
}

static void s_system_registry_rebuild_registration_indices(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i);
    system->registration_index = i;
  }
}

int ldk_system_registry_initialize(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return 1;
  }

  memset(registry, 0, sizeof(*registry));

  internal = (LDKSystemRegistryInternal*)LDK_ALLOC(sizeof(*internal));
  if (!internal)
  {
    return 1;
  }

  memset(internal, 0, sizeof(*internal));

  internal->systems = x_array_LDKRegisteredSystem_create(8);
  if (!internal->systems)
  {
    LDK_FREE(internal);
    return 1;
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
      return 1;
    }
  }

  registry->internal = internal;
  registry->is_initialized = 1;
  registry->is_started = 0;

  return 0;
}

void ldk_system_registry_terminate(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry || !registry->is_initialized)
  {
    return;
  }

  if (registry->is_started)
  {
    ldk_system_registry_stop(registry);
  }

  internal = s_system_registry_internal(registry);
  if (internal)
  {
    for (i = 0; i < LDK_SYSTEM_BUCKET_COUNT; ++i)
    {
      if (internal->buckets[i])
      {
        x_array_u32_destroy(internal->buckets[i]);
      }
    }

    if (internal->systems)
    {
      x_array_LDKRegisteredSystem_destroy(internal->systems);
    }

    LDK_FREE(internal);
  }

  memset(registry, 0, sizeof(*registry));
}

int ldk_system_registry_register(
    LDKSystemRegistry* registry,
    const LDKSystemDesc* desc)
{
  LDKSystemRegistryInternal* internal;
  LDKRegisteredSystem system;
  XArrayError result;

  if (!registry || !desc)
  {
    return 1;
  }

  if (!registry->is_initialized || registry->is_started)
  {
    return 1;
  }

  if (desc->id == 0)
  {
    return 1;
  }

  if (!s_system_desc_has_any_callback(desc))
  {
    return 1;
  }

  if (s_system_registry_find_by_id_const(registry, desc->id) != NULL)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal || !internal->systems)
  {
    return 1;
  }

  memset(&system, 0, sizeof(system));
  system.desc = *desc;
  system.userdata = NULL;
  system.registration_index = x_array_LDKRegisteredSystem_count(internal->systems);
  system.is_initialized = 0;

  result = x_array_LDKRegisteredSystem_push(internal->systems, system);
  if (result != XARRAY_OK)
  {
    return 1;
  }

  return 0;
}

int ldk_system_registry_unregister(
    LDKSystemRegistry* registry,
    u64 id)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return 1;
  }

  if (!registry->is_initialized || registry->is_started)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal || !internal->systems)
  {
    return 1;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i);

    if (system->desc.id == id)
    {
      x_array_LDKRegisteredSystem_delete_at(internal->systems, i);
      s_system_registry_rebuild_registration_indices(registry);
      return 0;
    }
  }

  return 1;
}

bool ldk_system_registry_find_by_id(LDKSystemRegistry* registry, u64 id, LDKSystemDesc* out)
{
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry)
  {
    return false;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return false;
  }

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i);
    if (system->desc.id == id)
    {
      return system;
      memcpy(out, &system->desc, sizeof(LDKSystemDesc));
      return true;
    }
  }

  return false;
}

int ldk_system_registry_clear(LDKSystemRegistry* registry)
{
  LDKSystemRegistryInternal* internal;

  if (!registry)
  {
    return 1;
  }

  if (!registry->is_initialized || registry->is_started)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal || !internal->systems)
  {
    return 1;
  }

  x_array_LDKRegisteredSystem_clear(internal->systems);
  s_system_registry_clear_bucket_lists(registry);

  return 0;
}

int ldk_system_registry_start(LDKSystemRegistry* registry)
{
  LDKRoot* engine;
  LDKSystemRegistryInternal* internal;
  u32 i;

  if (!registry || !registry->is_initialized || registry->is_started)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return 1;
  }

  if (s_system_registry_build_bucket_lists(registry))
  {
    return 1;
  }

  engine = ldk_root_get();

  for (i = 0; i < x_array_LDKRegisteredSystem_count(internal->systems); ++i)
  {
    LDKRegisteredSystem* system;
    int result;

    system = x_array_LDKRegisteredSystem_get(internal->systems, i);

    if (!system->desc.callbacks.initialize)
    {
      system->is_initialized = 1;
      continue;
    }

    result = system->desc.callbacks.initialize(engine, &system->userdata);
    if (result != 0)
    {
      s_system_registry_terminate_started_systems(registry);
      s_system_registry_clear_bucket_lists(registry);
      return 1;
    }

    system->is_initialized = 1;
  }

  registry->is_started = 1;
  return 0;
}

int ldk_system_registry_stop(LDKSystemRegistry* registry)
{
  if (!registry || !registry->is_initialized)
  {
    return 1;
  }

  if (!registry->is_started)
  {
    return 0;
  }

  s_system_registry_terminate_started_systems(registry);
  s_system_registry_clear_bucket_lists(registry);

  registry->is_started = 0;
  return 0;
}

int ldk_system_registry_run_bucket(
    LDKSystemRegistry* registry,
    LDKSystemBucket bucket,
    float dt)
{
  LDKSystemRegistryInternal* internal;
  LDKRoot* engine;
  XArray_u32* bucket_list;
  u32 i;

  if (!registry || !registry->is_initialized || !registry->is_started)
  {
    return 1;
  }

  if (bucket >= LDK_SYSTEM_BUCKET_COUNT)
  {
    return 1;
  }

  internal = s_system_registry_internal(registry);
  if (!internal)
  {
    return 1;
  }

  engine = ldk_root_get();
  bucket_list = internal->buckets[bucket];

  for (i = 0; i < x_array_u32_count(bucket_list); ++i)
  {
    u32* system_index_ptr;
    LDKRegisteredSystem* system;

    system_index_ptr = x_array_u32_get(bucket_list, i);
    system = x_array_LDKRegisteredSystem_get(internal->systems, *system_index_ptr);

    if (!(system->desc.flags & LDK_SYSTEM_FLAG_ENABLED))
    {
      continue;
    }

    switch (bucket)
    {
      case LDK_SYSTEM_BUCKET_PRE_UPDATE:
        if (system->desc.callbacks.pre_update)
        {
          system->desc.callbacks.pre_update(engine, system->userdata, dt);
        }
        break;

      case LDK_SYSTEM_BUCKET_UPDATE:
        if (system->desc.callbacks.update)
        {
          system->desc.callbacks.update(engine, system->userdata, dt);
        }
        break;

      case LDK_SYSTEM_BUCKET_POST_UPDATE:
        if (system->desc.callbacks.post_update)
        {
          system->desc.callbacks.post_update(engine, system->userdata, dt);
        }
        break;

      case LDK_SYSTEM_BUCKET_RENDER:
        if (system->desc.callbacks.render)
        {
          system->desc.callbacks.render(engine, system->userdata, dt);
        }
        break;

      default:
        return 1;
    }
  }

  return 0;
}

int ldk_system_registry_has(
    const LDKSystemRegistry* registry,
    u64 id)
{
  return s_system_registry_find_by_id_const(registry, id) != NULL;
}
