/**
 * @file ldk_transform.c
 * @brief Transform component data and transform helper API.
 */

#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>

static int s_entity_eq(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static LDKTransform* s_transform_get_ptr(LDKEntity entity)
{
  return (LDKTransform*)ldk_ecs_get_component(entity, LDK_COMPONENT_TYPE_TRANSFORM);
}

static const LDKTransform* s_transform_get_ptr_const(LDKEntity entity)
{
  return (const LDKTransform*)ldk_ecs_get_component_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
}

static bool s_transform_mark_subtree_dirty(LDKEntity entity)
{
  LDKTransform* transform = s_transform_get_ptr(entity);

  if (!transform)
  {
    return false;
  }

  transform->flags |= LDK_TRANSFORM_FLAG_WORLD_DIRTY;

  LDKEntity child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform* child_transform = s_transform_get_ptr_const(child);

    if (!child_transform)
    {
      break;
    }

    LDKEntity next_child = child_transform->next_sibling;
    s_transform_mark_subtree_dirty(child);
    child = next_child;
  }

  return true;
}

static bool s_transform_is_ancestor(LDKEntity entity, LDKEntity possible_ancestor)
{
  LDKEntity current = entity;

  while (!x_handle_is_null(current))
  {
    const LDKTransform* transform = s_transform_get_ptr_const(current);

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

static bool s_transform_unlink_from_parent(LDKTransform* child)
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
    LDKTransform* parent_transform = s_transform_get_ptr(child->parent);

    if (parent_transform)
    {
      parent_transform->first_child = child->next_sibling;
    }
  }
  else
  {
    LDKTransform* prev_transform = s_transform_get_ptr(child->prev_sibling);

    if (prev_transform)
    {
      prev_transform->next_sibling = child->next_sibling;
    }
  }

  if (!x_handle_is_null(child->next_sibling))
  {
    LDKTransform* next_transform = s_transform_get_ptr(child->next_sibling);

    if (next_transform)
    {
      next_transform->prev_sibling = child->prev_sibling;
    }
  }

  child->parent = x_handle_null();
  child->prev_sibling = x_handle_null();
  child->next_sibling = x_handle_null();
  return true;
}

static bool s_transform_orphan_children(LDKTransform* transform)
{
  if (!transform)
  {
    return false;
  }

  LDKEntity child = transform->first_child;
  transform->first_child = x_handle_null();

  while (!x_handle_is_null(child))
  {
    LDKTransform* child_transform = s_transform_get_ptr(child);

    if (!child_transform)
    {
      break;
    }

    LDKEntity next_child = child_transform->next_sibling;
    child_transform->parent = x_handle_null();
    child_transform->prev_sibling = x_handle_null();
    child_transform->next_sibling = x_handle_null();
    child_transform->flags |= LDK_TRANSFORM_FLAG_WORLD_DIRTY;
    s_transform_mark_subtree_dirty(child);
    child = next_child;
  }

  return true;
}

static bool s_transform_attach(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, const void* initial_value, void* user)
{
  LDKTransform* transform = (LDKTransform*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !transform)
  {
    return false;
  }

  if (!initial_value)
  {
    *transform = ldk_transform_make_default();
  }

  ldk_entity_add_internal_flags(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM);
  return true;
}

static void s_transform_destroy(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, void* user)
{
  LDKTransform* transform = (LDKTransform*)component;

  (void)component_registry;
  (void)component_index;
  (void)user;

  if (!entity_registry || !transform)
  {
    return;
  }

  s_transform_unlink_from_parent(transform);
  s_transform_orphan_children(transform);
  ldk_entity_remove_internal_flags(entity_registry, entity, LDK_ENTITY_INTERNAL_HAS_TRANSFORM);
}

LDKTransform ldk_transform_make_default(void)
{
  LDKTransform transform = {0};

  transform.local_position = vec3_make(0.0f, 0.0f, 0.0f);
  transform.local_rotation = quat_id();
  transform.local_scale = vec3_make(1.0f, 1.0f, 1.0f);
  transform.world_matrix = mat4_identity();
  transform.parent = x_handle_null();
  transform.first_child = x_handle_null();
  transform.next_sibling = x_handle_null();
  transform.prev_sibling = x_handle_null();
  transform.flags = LDK_TRANSFORM_FLAG_WORLD_DIRTY;
  return transform;
}

bool ldk_transform_set_local_position(LDKEntity entity, Vec3 position)
{
  LDKTransform* transform = s_transform_get_ptr(entity);

  if (!transform)
  {
    return false;
  }

  transform->local_position = position;
  return s_transform_mark_subtree_dirty(entity);
}

bool ldk_transform_set_local_rotation(LDKEntity entity, Quat rotation)
{
  LDKTransform* transform = s_transform_get_ptr(entity);

  if (!transform)
  {
    return false;
  }

  transform->local_rotation = rotation;
  return s_transform_mark_subtree_dirty(entity);
}

bool ldk_transform_set_local_scale(LDKEntity entity, Vec3 scale)
{
  LDKTransform* transform = s_transform_get_ptr(entity);

  if (!transform)
  {
    return false;
  }

  transform->local_scale = scale;
  return s_transform_mark_subtree_dirty(entity);
}

bool ldk_transform_get_local_position(LDKEntity entity, Vec3* out_position)
{
  if (!out_position)
  {
    return false;
  }

  const LDKTransform* transform = s_transform_get_ptr_const(entity);
  if (!transform)
  {
    return false;
  }

  *out_position = transform->local_position;
  return true;
}

bool ldk_transform_get_local_rotation(LDKEntity entity, Quat* out_rotation)
{
  if (!out_rotation)
  {
    return false;
  }

  const LDKTransform* transform = s_transform_get_ptr_const(entity);
  if (!transform)
  {
    return false;
  }

  *out_rotation = transform->local_rotation;
  return true;
}

bool ldk_transform_get_local_scale(LDKEntity entity, Vec3* out_scale)
{
  if (!out_scale)
  {
    return false;
  }

  const LDKTransform* transform = s_transform_get_ptr_const(entity);
  if (!transform)
  {
    return false;
  }

  *out_scale = transform->local_scale;
  return true;
}

bool ldk_transform_get_world_matrix(LDKEntity entity, Mat4* out_world_matrix)
{
  if (!out_world_matrix)
  {
    return false;
  }

  const LDKTransform* transform = s_transform_get_ptr_const(entity);
  if (!transform)
  {
    return false;
  }

  *out_world_matrix = transform->world_matrix;
  return true;
}

LDKEntity ldk_transform_get_parent(LDKEntity entity)
{
  const LDKTransform* transform = s_transform_get_ptr_const(entity);

  if (!transform)
  {
    return x_handle_null();
  }

  return transform->parent;
}

bool ldk_transform_set_parent(LDKEntity child_entity, LDKEntity parent_entity)
{
  LDKTransform* child_transform = s_transform_get_ptr(child_entity);

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

    LDKTransform* parent_transform = s_transform_get_ptr(parent_entity);
    if (!parent_transform)
    {
      return false;
    }

    if (s_transform_is_ancestor(parent_entity, child_entity))
    {
      return false;
    }
  }

  if (s_entity_eq(child_transform->parent, parent_entity))
  {
    return true;
  }

  s_transform_unlink_from_parent(child_transform);

  if (!x_handle_is_null(parent_entity))
  {
    LDKTransform* parent_transform = s_transform_get_ptr(parent_entity);

    if (!parent_transform)
    {
      return false;
    }

    child_transform->parent = parent_entity;
    child_transform->prev_sibling = x_handle_null();
    child_transform->next_sibling = parent_transform->first_child;

    if (!x_handle_is_null(parent_transform->first_child))
    {
      LDKTransform* first_child_transform = s_transform_get_ptr(parent_transform->first_child);

      if (first_child_transform)
      {
        first_child_transform->prev_sibling = child_entity;
      }
    }

    parent_transform->first_child = child_entity;
  }

  return s_transform_mark_subtree_dirty(child_entity);
}

bool ldk_transform_mark_dirty(LDKEntity entity)
{
  return s_transform_mark_subtree_dirty(entity);
}

#ifdef LDK_ENGINE
LDKComponentDesc ldk_transform_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};

  desc.name = "Transform";
  desc.type = LDK_COMPONENT_TYPE_TRANSFORM;
  desc.entry_size = sizeof(LDKTransform);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_transform_attach;
  desc.destroy = s_transform_destroy;
  desc.user = NULL;
  return desc;
}
#endif // LDK_ENGINE
