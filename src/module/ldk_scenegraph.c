#include <module/ldk_scenegraph.h>
#include <module/ldk_ecs.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_array.h>

static LDKTransform* s_scenegraph_transform_get(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  return ldk_entity_transform_get(
      entity_registry,
      component_registry,
      entity);
}

static const LDKTransform* s_scenegraph_transform_get_const(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  return ldk_entity_transform_get_const(
      entity_registry,
      component_registry,
      entity);
}

static bool s_scenegraph_update_subtree(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKTransform* transform,
    Mat4 parent_world, bool has_parent, bool parent_dirty)
{
  bool local_dirty = false;
  bool world_dirty = false;

  if (!entity_registry || !component_registry || !transform)
  {
    return false;
  }

  local_dirty = (transform->flags & LDK_TRANSFORM_FLAG_WORLD_DIRTY) != 0;
  world_dirty = parent_dirty || local_dirty;

  if (world_dirty)
  {
    Mat4 local_matrix = mat4_compose(
        transform->local_position,
        transform->local_rotation,
        transform->local_scale);

    if (has_parent)
    {
      transform->world_matrix = mat4_mul(parent_world, local_matrix);
    }
    else
    {
      transform->world_matrix = local_matrix;
    }

    transform->flags &= ~LDK_TRANSFORM_FLAG_WORLD_DIRTY;
  }

  LDKEntity child = transform->first_child;

  while (!x_handle_is_null(child))
  {
    LDKTransform* child_transform = s_scenegraph_transform_get(
        entity_registry,
        component_registry,
        child);

    if (!child_transform)
    {
      return false;
    }

    LDKEntity next_child = child_transform->next_sibling;

    if (!s_scenegraph_update_subtree(
          entity_registry,
          component_registry,
          child_transform,
          transform->world_matrix,
          true,
          world_dirty))
    {
      return false;
    }

    child = next_child;
  }

  return true;
}

bool ldk_scenegraph_update(float dt)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  (void)dt;

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  XArray* store = ldk_component_store_get(
      component_registry,
      LDK_COMPONENT_TYPE_TRANSFORM);

  XArray* owners = ldk_component_owners_get(
      component_registry,
      LDK_COMPONENT_TYPE_TRANSFORM);

  if (!store || !owners)
  {
    return false;
  }

  if (x_array_count(store) != x_array_count(owners))
  {
    return false;
  }

  for (u32 i = 0; i < x_array_count(store); ++i)
  {
    LDKTransform* transform = (LDKTransform*)x_array_get(store, i);

    if (!transform)
    {
      return false;
    }

    if (!x_handle_is_null(transform->parent))
    {
      continue;
    }

    if (!s_scenegraph_update_subtree(
          entity_registry,
          component_registry,
          transform,
          mat4_identity(),
          false,
          false))
    {
      return false;
    }
  }

  return true;
}

bool ldk_scenegraph_update_entity(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  LDKTransform* transform = s_scenegraph_transform_get(
      entity_registry, component_registry, entity);

  if (!transform)
  {
    return false;
  }

  LDKEntity root = entity;
  const LDKTransform* current_transform = transform;

  while (!x_handle_is_null(current_transform->parent))
  {
    root = current_transform->parent;

    current_transform = s_scenegraph_transform_get_const(
        entity_registry, component_registry, root);
    if (!current_transform)
    {
      return false;
    }
  }

  LDKTransform* root_transform = s_scenegraph_transform_get(
      entity_registry, component_registry, root);

  if (!root_transform)
  {
    return false;
  }

  return s_scenegraph_update_subtree(entity_registry, component_registry,
      root_transform, mat4_identity(), false, false);
}

bool ldk_scenegraph_set_parent(
    LDKEntity child_entity, LDKEntity parent_entity)
{
  return ldk_transform_set_parent(child_entity, parent_entity);
}

bool ldk_scenegraph_detach(LDKEntity entity)
{
  return ldk_transform_set_parent(entity, x_handle_null());
}

LDKEntity ldk_scenegraph_get_parent(LDKEntity entity)
{
  return ldk_transform_get_parent(entity);
}
