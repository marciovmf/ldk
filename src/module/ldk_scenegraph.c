#include <module/ldk_scenegraph.h>
#include <module/ldk_ecs.h>
#include <component/ldk_transform.h>
#include <stdx/stdx_array.h>

static bool s_entity_eq(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static LDKTransform* s_scenegraph_get_transform(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  return ldk_entity_get_transform(
      entity_registry,
      component_registry,
      entity);
}

static const LDKTransform* s_scenegraph_get_transform_const(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  return ldk_entity_get_transform_const(
      entity_registry,
      component_registry,
      entity);
}

static bool s_scenegraph_mark_subtree_dirty(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  LDKTransform* transform = s_scenegraph_get_transform(
      entity_registry,
      component_registry,
      entity);

  if (!transform)
  {
    return false;
  }

  transform->flags |= LDK_TRANSFORM_FLAG_WORLD_DIRTY;

  LDKEntity child = transform->first_child;

  while (!x_handle_is_null(child))
  {
    const LDKTransform* child_transform = s_scenegraph_get_transform_const(
        entity_registry,
        component_registry,
        child);

    if (!child_transform)
    {
      return false;
    }

    LDKEntity next_child = child_transform->next_sibling;

    if (!s_scenegraph_mark_subtree_dirty(
          entity_registry,
          component_registry,
          child))
    {
      return false;
    }

    child = next_child;
  }

  return true;
}

static bool s_scenegraph_is_ancestor(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity,
    LDKEntity possible_ancestor)
{
  LDKEntity current = entity;

  while (!x_handle_is_null(current))
  {
    const LDKTransform* transform = s_scenegraph_get_transform_const(
        entity_registry,
        component_registry,
        current);

    if (!transform)
    {
      return false;
    }

    if (s_entity_eq(transform->parent, possible_ancestor))
    {
      return true;
    }

    current = transform->parent;
  }

  return false;
}

static bool s_scenegraph_unlink_from_parent(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKTransform* child)
{
  if (!child)
  {
    return false;
  }

  if (x_handle_is_null(child->parent))
  {
    child->prev_sibling = x_handle_null();
    child->next_sibling = x_handle_null();
    return true;
  }

  if (x_handle_is_null(child->prev_sibling))
  {
    LDKTransform* parent_transform = s_scenegraph_get_transform(
        entity_registry,
        component_registry,
        child->parent);

    if (!parent_transform)
    {
      return false;
    }

    parent_transform->first_child = child->next_sibling;
  }
  else
  {
    LDKTransform* prev_transform = s_scenegraph_get_transform(
        entity_registry,
        component_registry,
        child->prev_sibling);

    if (!prev_transform)
    {
      return false;
    }

    prev_transform->next_sibling = child->next_sibling;
  }

  if (!x_handle_is_null(child->next_sibling))
  {
    LDKTransform* next_transform = s_scenegraph_get_transform(
        entity_registry,
        component_registry,
        child->next_sibling);

    if (!next_transform)
    {
      return false;
    }

    next_transform->prev_sibling = child->prev_sibling;
  }

  child->parent = x_handle_null();
  child->prev_sibling = x_handle_null();
  child->next_sibling = x_handle_null();

  return true;
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
    LDKTransform* child_transform = s_scenegraph_get_transform(
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
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry();

  (void)dt;

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  XArray* store = ldk_component_get_store(
      component_registry,
      LDK_COMPONENT_TYPE_TRANSFORM);

  XArray* owners = ldk_component_get_owners(
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
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  LDKTransform* transform = s_scenegraph_get_transform(
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

    current_transform = s_scenegraph_get_transform_const(
        entity_registry, component_registry, root);
    if (!current_transform)
    {
      return false;
    }
  }

  LDKTransform* root_transform = s_scenegraph_get_transform(
      entity_registry, component_registry, root);

  if (!root_transform)
  {
    return false;
  }

  return s_scenegraph_update_subtree(entity_registry, component_registry,
      root_transform, mat4_identity(), false, false);
}

bool ldk_scenegraph_set_parent(LDKEntity child_entity, LDKEntity parent_entity)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  LDKTransform* child_transform = s_scenegraph_get_transform(
      entity_registry,
      component_registry,
      child_entity);

  if (!child_transform)
  {
    return false;
  }

  if (!x_handle_is_null(parent_entity))
  {
    if (s_entity_eq(child_entity, parent_entity))
    {
      return false;
    }

    LDKTransform* parent_transform = s_scenegraph_get_transform(
        entity_registry,
        component_registry,
        parent_entity);

    if (!parent_transform)
    {
      return false;
    }

    if (s_scenegraph_is_ancestor(
          entity_registry,
          component_registry,
          parent_entity,
          child_entity))
    {
      return false;
    }
  }

  if (s_entity_eq(child_transform->parent, parent_entity))
  {
    return true;
  }

  if (!s_scenegraph_unlink_from_parent(
        entity_registry,
        component_registry,
        child_transform))
  {
    return false;
  }

  if (!x_handle_is_null(parent_entity))
  {
    LDKTransform* parent_transform = s_scenegraph_get_transform(
        entity_registry,
        component_registry,
        parent_entity);

    if (!parent_transform)
    {
      return false;
    }

    child_transform->parent = parent_entity;
    child_transform->prev_sibling = x_handle_null();
    child_transform->next_sibling = parent_transform->first_child;

    if (!x_handle_is_null(parent_transform->first_child))
    {
      LDKTransform* first_child_transform = s_scenegraph_get_transform(
          entity_registry,
          component_registry,
          parent_transform->first_child);

      if (!first_child_transform)
      {
        return false;
      }

      first_child_transform->prev_sibling = child_entity;
    }

    parent_transform->first_child = child_entity;
  }

  return s_scenegraph_mark_subtree_dirty(
      entity_registry,
      component_registry,
      child_entity);
}

bool ldk_scenegraph_detach(LDKEntity entity)
{
  return ldk_scenegraph_set_parent(entity, x_handle_null());
}

LDKEntity ldk_scenegraph_get_parent(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = ldk_ecs_entity_registry();
  LDKComponentRegistry* component_registry = ldk_ecs_component_registry();

  if (!entity_registry || !component_registry)
  {
    return x_handle_null();
  }

  const LDKTransform* transform = s_scenegraph_get_transform_const(
      entity_registry,
      component_registry,
      entity);

  if (!transform)
  {
    return x_handle_null();
  }

  return transform->parent;
}
