/**
 * @file ldk_component_transform.c
 * @brief Transform component and transform related API
 *
 * Defines the transform component and exposes functions for transform
 * operations.
 */


#include <component/ldk_transform.h>
#include <ldk.h>

static int s_entity_eq(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static bool s_transform_get_modules(LDKEntityRegistry** out_entity_registry,
    LDKComponentRegistry** out_component_registry)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!out_entity_registry || !out_component_registry)
  {
    return false;
  }

  if (!ldk_engine_is_initialized())
  {
    return false;
  }

  entity_registry = (LDKEntityRegistry*)ldk_module_get(LDK_MODULE_ENTITY);
  component_registry = (LDKComponentRegistry*)ldk_module_get(LDK_MODULE_COMPONENT);

  if (!entity_registry || !component_registry)
  {
    return false;
  }

  *out_entity_registry = entity_registry;
  *out_component_registry = component_registry;
  return true;
}

static bool s_transform_get_ref(LDKEntityRegistry* entity_registry, LDKEntity entity, LDKComponentRef* out_ref)
{
  if (!entity_registry || !out_ref)
  {
    return false;
  }

  return ldk_entity_get_component_ref(
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_TRANSFORM,
      out_ref);
}

static LDKTransform* s_transform_from_ref(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKComponentRef ref)
{
  return (LDKTransform*)ldk_component_ref_get(entity_registry, component_registry, ref);
}

static const LDKTransform* s_transform_from_ref_const(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKComponentRef ref)
{
  return (const LDKTransform*)ldk_component_ref_get_const(entity_registry, component_registry, ref);
}

static LDKTransform* s_transform_get_ptr(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_transform_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_transform_from_ref(entity_registry, component_registry, ref);
}

static const LDKTransform* s_transform_get_ptr_const(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  LDKComponentRef ref = {0};

  if (!s_transform_get_ref(entity_registry, entity, &ref))
  {
    return NULL;
  }

  return s_transform_from_ref_const(entity_registry, component_registry, ref);
}

static bool s_transform_mark_subtree_dirty(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity)
{
  LDKTransform* transform = NULL;
  LDKEntity child = {0};

  transform = s_transform_get_ptr(entity_registry, component_registry, entity);
  if (!transform)
  {
    return false;
  }

  transform->flags |= LDK_TRANSFORM_FLAG_WORLD_DIRTY;
  child = transform->first_child;

  while (!x_handle_is_null(child))
  {
    const LDKTransform* child_transform = NULL;
    LDKEntity next_child = {0};

    child_transform = s_transform_get_ptr_const(entity_registry, component_registry, child);
    if (!child_transform)
    {
      break;
    }

    next_child = child_transform->next_sibling;
    s_transform_mark_subtree_dirty(entity_registry, component_registry, child);
    child = next_child;
  }

  return true;
}

static bool s_transform_is_ancestor(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity, LDKEntity possible_ancestor)
{
  LDKEntity current = entity;

  while (!x_handle_is_null(current))
  {
    const LDKTransform* transform = NULL;

    transform = s_transform_get_ptr_const(entity_registry, component_registry, current);
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

static bool s_transform_unlink_from_parent(LDKEntityRegistry* entity_registry,
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
    LDKTransform* parent_transform = NULL;

    parent_transform = s_transform_get_ptr(entity_registry, component_registry, child->parent);
    if (parent_transform)
    {
      parent_transform->first_child = child->next_sibling;
    }
  }
  else
  {
    LDKTransform* prev_transform = NULL;

    prev_transform = s_transform_get_ptr(entity_registry, component_registry, child->prev_sibling);
    if (prev_transform)
    {
      prev_transform->next_sibling = child->next_sibling;
    }
  }

  if (!x_handle_is_null(child->next_sibling))
  {
    LDKTransform* next_transform = NULL;

    next_transform = s_transform_get_ptr(entity_registry, component_registry, child->next_sibling);
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

static bool s_transform_orphan_children(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKTransform* transform)
{
  LDKEntity child = {0};

  if (!transform)
  {
    return false;
  }

  child = transform->first_child;
  transform->first_child = x_handle_null();

  while (!x_handle_is_null(child))
  {
    LDKTransform* child_transform = NULL;
    LDKEntity next_child = {0};

    child_transform = s_transform_get_ptr(entity_registry, component_registry, child);
    if (!child_transform)
    {
      break;
    }

    next_child = child_transform->next_sibling;
    child_transform->parent = x_handle_null();
    child_transform->prev_sibling = x_handle_null();
    child_transform->next_sibling = x_handle_null();
    child_transform->flags |= LDK_TRANSFORM_FLAG_WORLD_DIRTY;
    s_transform_mark_subtree_dirty(entity_registry, component_registry, child);
    child = next_child;
  }

  return true;
}

static bool s_transform_attach(LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry, LDKEntity entity, void* component,
    u32 component_index, const void* initial_value, void* user)
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

  ldk_entity_add_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_TRANSFORM);

  return true;
}

static void s_transform_destroy(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry,
    LDKEntity entity, void* component, u32 component_index, void* user)
{
  LDKTransform* transform = (LDKTransform*)component;

  (void)component_index;
  (void)user;

  if (!entity_registry || !component_registry || !transform)
  {
    return;
  }

  s_transform_unlink_from_parent(entity_registry, component_registry, transform);
  s_transform_orphan_children(entity_registry, component_registry, transform);

  ldk_entity_remove_internal_flags(
      entity_registry,
      entity,
      LDK_ENTITY_INTERNAL_HAS_TRANSFORM);
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

bool ldk_transform_register(LDKComponentRegistry* registry, u32 initial_capacity)
{
  if (!registry)
  {
    return false;
  }

  return ldk_component_register(
      registry,
      "Transform",
      LDK_COMPONENT_TYPE_TRANSFORM,
      sizeof(LDKTransform),
      initial_capacity,
      s_transform_attach,
      s_transform_destroy,
      NULL);
}

bool ldk_transform_attach(LDKEntity entity, const LDKTransform* initial_value)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  return ldk_entity_add_component(
      entity_registry,
      component_registry,
      entity,
      LDK_COMPONENT_TYPE_TRANSFORM,
      initial_value) != NULL;
}

bool ldk_transform_detach(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  return ldk_component_remove_entity(
      component_registry,
      entity_registry,
      entity,
      LDK_COMPONENT_TYPE_TRANSFORM);
}

bool ldk_transform_get(LDKEntity entity, LDKTransform* out_transform)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  const LDKTransform* transform = NULL;

  if (!out_transform)
  {
    return false;
  }

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  transform = s_transform_get_ptr_const(entity_registry, component_registry, entity);
  if (!transform)
  {
    return false;
  }

  *out_transform = *transform;
  return true;
}

bool ldk_transform_set(LDKEntity entity, const LDKTransform* transform)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKTransform* current = NULL;

  if (!transform)
  {
    return false;
  }

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  current = s_transform_get_ptr(entity_registry, component_registry, entity);
  if (!current)
  {
    return false;
  }

  current->local_position = transform->local_position;
  current->local_rotation = transform->local_rotation;
  current->local_scale = transform->local_scale;
  return s_transform_mark_subtree_dirty(entity_registry, component_registry, entity);
}

bool ldk_transform_set_local_position(LDKEntity entity, Vec3 position)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKTransform* transform = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  transform = s_transform_get_ptr(entity_registry, component_registry, entity);
  if (!transform)
  {
    return false;
  }

  transform->local_position = position;
  return s_transform_mark_subtree_dirty(entity_registry, component_registry, entity);
}

bool ldk_transform_set_local_rotation(LDKEntity entity, Quat rotation)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKTransform* transform = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  transform = s_transform_get_ptr(entity_registry, component_registry, entity);
  if (!transform)
  {
    return false;
  }

  transform->local_rotation = rotation;
  return s_transform_mark_subtree_dirty(entity_registry, component_registry, entity);
}

bool ldk_transform_set_local_scale(LDKEntity entity, Vec3 scale)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKTransform* transform = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  transform = s_transform_get_ptr(entity_registry, component_registry, entity);
  if (!transform)
  {
    return false;
  }

  transform->local_scale = scale;
  return s_transform_mark_subtree_dirty(entity_registry, component_registry, entity);
}

bool ldk_transform_get_local_position(LDKEntity entity, Vec3* out_position)
{
  LDKTransform transform = {0};

  if (!out_position)
  {
    return false;
  }

  if (!ldk_transform_get(entity, &transform))
  {
    return false;
  }

  *out_position = transform.local_position;
  return true;
}

bool ldk_transform_get_local_rotation(LDKEntity entity, Quat* out_rotation)
{
  LDKTransform transform = {0};

  if (!out_rotation)
  {
    return false;
  }

  if (!ldk_transform_get(entity, &transform))
  {
    return false;
  }

  *out_rotation = transform.local_rotation;
  return true;
}

bool ldk_transform_get_local_scale(LDKEntity entity, Vec3* out_scale)
{
  LDKTransform transform = {0};

  if (!out_scale)
  {
    return false;
  }

  if (!ldk_transform_get(entity, &transform))
  {
    return false;
  }

  *out_scale = transform.local_scale;
  return true;
}

bool ldk_transform_get_world_matrix(LDKEntity entity, Mat4* out_world_matrix)
{
  LDKTransform transform = {0};

  if (!out_world_matrix)
  {
    return false;
  }

  if (!ldk_transform_get(entity, &transform))
  {
    return false;
  }

  *out_world_matrix = transform.world_matrix;
  return true;
}

LDKEntity ldk_transform_get_parent(LDKEntity entity)
{
  LDKTransform transform = {0};

  if (!ldk_transform_get(entity, &transform))
  {
    return x_handle_null();
  }

  return transform.parent;
}

bool ldk_transform_set_parent(LDKEntity child_entity, LDKEntity parent_entity)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;
  LDKTransform* child_transform = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  child_transform = s_transform_get_ptr(entity_registry, component_registry, child_entity);
  if (!child_transform)
  {
    return false;
  }

  if (!x_handle_is_null(parent_entity))
  {
    LDKTransform* parent_transform = NULL;

    if (s_entity_eq(child_entity, parent_entity))
    {
      return false;
    }

    parent_transform = s_transform_get_ptr(entity_registry, component_registry, parent_entity);
    if (!parent_transform)
    {
      return false;
    }

    if (s_transform_is_ancestor(entity_registry, component_registry, parent_entity, child_entity))
    {
      return false;
    }
  }

  if (s_entity_eq(child_transform->parent, parent_entity))
  {
    return true;
  }

  s_transform_unlink_from_parent(entity_registry, component_registry, child_transform);

  if (!x_handle_is_null(parent_entity))
  {
    LDKTransform* parent_transform = NULL;

    parent_transform = s_transform_get_ptr(entity_registry, component_registry, parent_entity);
    if (!parent_transform)
    {
      return false;
    }

    child_transform->parent = parent_entity;
    child_transform->prev_sibling = x_handle_null();
    child_transform->next_sibling = parent_transform->first_child;

    if (!x_handle_is_null(parent_transform->first_child))
    {
      LDKTransform* first_child_transform = NULL;

      first_child_transform = s_transform_get_ptr(entity_registry, component_registry, parent_transform->first_child);
      if (first_child_transform)
      {
        first_child_transform->prev_sibling = child_entity;
      }
    }

    parent_transform->first_child = child_entity;
  }

  return s_transform_mark_subtree_dirty(entity_registry, component_registry, child_entity);
}

bool ldk_transform_mark_dirty(LDKEntity entity)
{
  LDKEntityRegistry* entity_registry = NULL;
  LDKComponentRegistry* component_registry = NULL;

  if (!s_transform_get_modules(&entity_registry, &component_registry))
  {
    return false;
  }

  return s_transform_mark_subtree_dirty(entity_registry, component_registry, entity);
}
