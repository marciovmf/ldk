#include <component/ldk_instanced_mesh_source.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool ldk_instanced_mesh_source_reserve_instances(
    LDKInstancedMeshSource *source, u32 count)
{
  if (!source || count > UINT32_MAX / sizeof(Mat4))
  {
    return false;
  }
  if (count <= source->instance_capacity)
  {
    return true;
  }

  u32 capacity = source->instance_capacity ? source->instance_capacity : 16u;
  while (capacity < count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = count;
      break;
    }
    capacity *= 2u;
  }
  if (capacity < count || capacity > UINT32_MAX / sizeof(Mat4))
  {
    return false;
  }

  Mat4 *instances = realloc(source->instances, (size_t)capacity * sizeof(Mat4));
  if (!instances)
  {
    return false;
  }
  source->instances = instances;
  source->instance_capacity = capacity;
  return true;
}

bool ldk_instanced_mesh_source_resize_instances(
    LDKInstancedMeshSource *source, u32 count)
{
  if (!source || !ldk_instanced_mesh_source_reserve_instances(source, count))
  {
    return false;
  }

  for (u32 i = source->instance_count; i < count; ++i)
  {
    source->instances[i] = mat4_identity();
  }
  source->instance_count = count;
  return true;
}

bool ldk_instanced_mesh_source_set_instances(
    LDKInstancedMeshSource *source, const Mat4 *instances, u32 count)
{
  if (!source || (count && !instances) || count > UINT32_MAX / sizeof(Mat4))
  {
    return false;
  }
  for (u32 i = 0; i < count; ++i)
  {
    for (u32 j = 0; j < 16; ++j)
    {
      if (!isfinite(instances[i].m[j]))
      {
        return false;
      }
    }
  }
  if (!ldk_instanced_mesh_source_reserve_instances(source, count))
  {
    return false;
  }
  if (count)
  {
    memmove(source->instances, instances, (size_t)count * sizeof(Mat4));
  }
  source->instance_count = count;
  return true;
}

#ifdef LDK_ENGINE
static bool s_instanced_mesh_source_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  LDKInstancedMeshSource *target = component;
  const LDKInstancedMeshSource *initial = initial_value;
  LDKComponentDesc base = ldk_mesh_source_component_desc(0);
  if (!target)
  {
    return false;
  }

  // Component storage initially contains a shallow copy. Never own its pointers.
  target->instances = NULL;
  target->instance_count = 0;
  target->instance_capacity = 0;
  if (initial && !ldk_instanced_mesh_source_set_instances(
          target, initial->instances, initial->instance_count))
  {
    return false;
  }
  if (!base.attach(entities, components, entity, &target->source, index,
          initial ? &initial->source : NULL, user))
  {
    free(target->instances);
    target->instances = NULL;
    target->instance_count = 0;
    target->instance_capacity = 0;
    return false;
  }
  return true;
}

static void s_instanced_mesh_source_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  LDKInstancedMeshSource *source = component;
  if (!source)
  {
    return;
  }
  LDKComponentDesc base = ldk_mesh_source_component_desc(0);
  base.destroy(entities, components, entity, &source->source, index, user);
  free(source->instances);
  source->instances = NULL;
  source->instance_count = 0;
  source->instance_capacity = 0;
  if (ldk_entity_component_has(entities, entity, LDK_COMPONENT_TYPE_MESH_SOURCE))
  {
    ldk_entity_internal_flags_add(
        entities, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
  }
  else
  {
    ldk_entity_internal_flags_remove(
        entities, entity, LDK_ENTITY_INTERNAL_HAS_RENDERABLE);
  }
}

LDKComponentDesc ldk_instanced_mesh_source_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};
  desc.name = "InstancedMeshSource";
  desc.type = LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE;
  desc.entry_size = sizeof(LDKInstancedMeshSource);
  desc.initial_capacity = initial_capacity;
  desc.flags = LDK_COMPONENT_FLAG_HIDE_IN_INSPECTOR;
  desc.attach = s_instanced_mesh_source_attach;
  desc.destroy = s_instanced_mesh_source_destroy;
  return desc;
}
#endif
