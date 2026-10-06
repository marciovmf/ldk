#include <system/ldk_terrain_follower_system.h>

#include <component/ldk_terrain_follower.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <system/ldk_terrain_system.h>
#include <stdx/stdx_math.h>

#include <math.h>

void ldk_terrain_follower_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  (void)data;
  (void)dt;

  if (!group || !ldk_terrain_system_is_active())
  {
    return;
  }

  for (u32 i = 0u; i < group->count; ++i)
  {
    LDKEntity entity = group->entities[i];
    LDKTerrainFollowerComponent *follower =
        (LDKTerrainFollowerComponent *)ldk_ecs_component_get(
            entity, LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER);
    Vec3 local_position;
    Vec3 world_position;
    LDKEntity parent;
    Mat4 parent_world = mat4_identity();
    float terrain_height;

    if (!follower ||
        !ldk_transform_get_local_position(entity, &local_position))
    {
      continue;
    }

    parent = ldk_transform_get_parent(entity);
    world_position = local_position;
    if (!x_handle_is_null(parent))
    {
      if (!ldk_scenegraph_update_entity(parent) ||
          !ldk_transform_get_world_matrix(parent, &parent_world))
      {
        continue;
      }
      world_position = mat4_mul_point(parent_world, local_position);
    }

    if (!ldk_terrain_height_at_world(
            world_position.x, world_position.z, &terrain_height))
    {
      continue;
    }

    world_position.y = terrain_height +
        (isfinite(follower->height_offset) ? follower->height_offset : 0.0f);

    if (!x_handle_is_null(parent))
    {
      bool inverse_ok = false;
      Mat4 inverse_parent = mat4_inverse_full(parent_world, &inverse_ok);
      if (!inverse_ok)
      {
        continue;
      }
      local_position = mat4_mul_point(inverse_parent, world_position);
    }
    else
    {
      local_position = world_position;
    }

    (void)ldk_transform_set_local_position(entity, local_position);
  }
}
