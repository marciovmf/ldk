#include <system/ldk_scenegraph.h>
#include <component/ldk_transform.h>
#include <ldk.h>

static bool s_scenegraph_update_subtree(
    LDKEntityRegistry* entity_registry,
    LDKComponentRegistry* component_registry,
    LDKEntity entity,
    Mat4 parent_world,
    int has_parent)
{
  LDKComponentRef ref = {0};
  LDKTransform* transform = NULL;
  Mat4 local_matrix = {0};
  LDKEntity child = {0};

  if (!ldk_entity_get_component_ref(entity_registry, entity, LDK_COMPONENT_TYPE_TRANSFORM, &ref))
  {
    return false;
  }

  transform = (LDKTransform*)ldk_component_ref_get(entity_registry, component_registry, ref);
  if (!transform)
  {
    return false;
  }

  local_matrix = mat4_compose(
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
  child = transform->first_child;

  while (!x_handle_is_null(child))
  {
    LDKComponentRef child_ref = {0};
    const LDKTransform* child_transform = NULL;
    LDKEntity next_child = {0};

    if (!ldk_entity_get_component_ref(entity_registry, child, LDK_COMPONENT_TYPE_TRANSFORM, &child_ref))
    {
      break;
    }

    child_transform = (const LDKTransform*)ldk_component_ref_get_const(entity_registry, component_registry, child_ref);
    if (!child_transform)
    {
      break;
    }

    next_child = child_transform->next_sibling;
    s_scenegraph_update_subtree(
        entity_registry,
        component_registry,
        child,
        transform->world_matrix,
        1);
    child = next_child;
  }

  return true;
}

bool ldk_scenegraph_update(float dt)
{
  LDKComponentRegistry* component_registry = NULL;
  LDKEntityRegistry* entity_registry = NULL;
  XArray* owners = NULL;
  XArray* store = NULL;
  u32 i = 0;

  (void)dt;

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

  store = ldk_component_get_store(component_registry, LDK_COMPONENT_TYPE_TRANSFORM);
  owners = ldk_component_get_owners(component_registry, LDK_COMPONENT_TYPE_TRANSFORM);

  if (!store || !owners)
  {
    return false;
  }

  if (x_array_count(store) != x_array_count(owners))
  {
    return false;
  }

  for (i = 0; i < x_array_count(store); ++i)
  {
    LDKEntity* entity_ptr = NULL;
    LDKTransform* transform = NULL;

    entity_ptr = (LDKEntity*)x_array_get(owners, i);
    transform = (LDKTransform*)x_array_get(store, i);

    if (!entity_ptr || !transform)
    {
      continue;
    }

    if (!x_handle_is_null(transform->parent))
    {
      continue;
    }

    s_scenegraph_update_subtree(
        entity_registry,
        component_registry,
        *entity_ptr,
        mat4_identity(),
        0);
  }

  return true;
}

static int s_scenegraph_initialize(void** userdata)
{
  if (userdata)
  {
    *userdata = NULL;
  }

  return 0;
}

static void s_scenegraph_update_callback(void* userdata, float dt)
{
  (void)userdata;
  ldk_scenegraph_update(dt);
}

const LDKSystemDesc* ldk_scenegraph_system_desc(void)
{
  static const LDKSystemDesc desc =
  {
    LDK_SYSTEM_ID_SCENEGRAPH,
    "SceneGraph",
    LDK_SYSTEM_FLAG_ENABLED | LDK_SYSTEM_FLAG_ENGINE_NATIVE,
    0,
    0,
    0,
    0,
    {
      s_scenegraph_initialize,
      NULL,
      NULL,
      s_scenegraph_update_callback,
      NULL,
      NULL
    }
  };

  return &desc;
}
