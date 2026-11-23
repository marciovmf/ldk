/**
 * # Terrain system
 *
 * Generic configurable height-field terrain system.
 */

#ifndef LDK_TERRAIN_SYSTEM_H
#define LDK_TERRAIN_SYSTEM_H

#include <ldk_asset.h>
#include <ldk_terrain.h>
#include <ldk_color.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set a direct runtime heightmap when no Terrain Asset is assigned.
 * Kept for procedural terrain providers until the shared Terrain Asset
 * creation API is integrated. The heightmap is borrowed from AssetManager.
 * Has no effect on terrain entities with a .terrain asset assigned.
 * @group ldk.system.terrain_system
 */
LDK_API bool ldk_terrain_heightmap_set(LDKAssetImage image);
/**
 * @brief Check whether the terrain system is active.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.system.terrain_system
 */
LDK_API bool ldk_terrain_system_is_active(void);
/**
 * @brief Revision of the currently queryable terrain geometry.
 * Returns zero when no terrain geometry is ready. Includes the flat default
 * terrain when no heightmap is assigned. Changes when terrain geometry or
 * its world-space mapping changes.
 * @return Nonzero terrain geometry revision, or zero if unavailable.
 * @group ldk.system.terrain_system
 */
LDK_API u64 ldk_terrain_geometry_revision_get(void);
/**
 * @brief Query terrain height at a world-space X/Z point.
 * @param world_x World-space X coordinate.
 * @param world_z World-space Z coordinate.
 * @param out_height Receives the surface height in world-space units.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.terrain_system
 */
LDK_API bool ldk_terrain_height_at_world(
    float world_x, float world_z, float *out_height);

/**
 * @brief Query the geometric normal of the heightfield triangle at world X/Z.
 * @return False if there is no active terrain at the position.
 * @group ldk.system.terrain_system
 */
LDK_API bool ldk_terrain_normal_at_world(
    float world_x, float world_z, Vec3 *out_normal);

/** Query a specific terrain entity rather than the highest surface at X/Z.
 * Only upright terrain transforms are supported by height queries.
 */
LDK_API bool ldk_terrain_entity_height_at_world(
    LDKEntity entity, float world_x, float world_z, float *out_height);
LDK_API bool ldk_terrain_entity_normal_at_world(
    LDKEntity entity, float world_x, float world_z, Vec3 *out_normal);

/** Release cached Editor-mode terrain resources before replacing the scene. */
LDK_API void ldk_terrain_system_editor_clear(void);

/**
 * @brief Initialize the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @return System initialization status code.
 * @group ldk.system.terrain_system
 */
int ldk_terrain_system_initialize(void *data);
/**
 * @brief Update the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group ldk.system.terrain_system
 */
void ldk_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
/** Comet calls this in Edit mode using the scene-owned system data.
 * It shares the same renderer/cache with the normal Play update.
 */
//@editor_callback system=LDKTerrainSystem flags=OnEditorUpdate
LDK_GAME_API void ldk_terrain_system_on_editor_update(u64 system_id, void *data, float dt);

/**
 * @brief Shut down the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @group ldk.system.terrain_system
 */
void ldk_terrain_system_terminate(void *data);

/** @group ldk.system.terrain_system */
//@system name=LDKTerrainSystem initialize=ldk_terrain_system_initialize update=ldk_terrain_system_update terminate=ldk_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
typedef struct LDKTerrainSystem
{
  //@begin_group "Geometry"
  /** Number of rendered mesh cells per chunk. */
  //@inspect min=1 max=256
  u32 chunk_size;
  /** Source heightmap cells per mesh vertex (1 = full resolution). */
  //@inspect min=1 max=256
  u32 mesh_sample_step;
  //@end_group

  //@begin_group "Rendering"
  bool cull;
  bool casts_shadows;
  //@end_group

  //@begin_group "Runtime"
  //@inspect readonly runtime
  u32 data_width;
  //@inspect readonly runtime
  u32 data_height;
  //@inspect readonly runtime
  u32 chunk_count;
  //@inspect readonly runtime
  u32 visible_chunk_count;
  //@inspect readonly runtime
  u32 mesh_update_count;
  //@inspect readonly runtime
  bool has_geometry;
  //@end_group
} LDKTerrainSystem;

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_SYSTEM_H
