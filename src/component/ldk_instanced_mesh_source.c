#include <component/ldk_instanced_mesh_source.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
  Mat4 *copy = NULL;
  if (count)
  {
    copy = malloc((size_t)count * sizeof(Mat4));
    if (!copy)
    {
      return false;
    }
    memcpy(copy, instances, (size_t)count * sizeof(Mat4));
  }
  free(source->instances);
  source->instances = copy;
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
  desc.attach = s_instanced_mesh_source_attach;
  desc.destroy = s_instanced_mesh_source_destroy;
  return desc;
}
#endif
