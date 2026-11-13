#ifndef NACRE_DUNGEON_SYSTEM_H
#define NACRE_DUNGEON_SYSTEM_H

#include "nacre_dungeon_generator.h"
#include "nacre_dungeon_placement.h"

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_system.h>
#include <stdx/stdx_string.h>
#include <stdx/stdx_math.h>

/*
 * Scene-owned game system, discovered/registered by Comet.
 *
 * This is intentionally separate from the C-only geometry generator.
 * The generator owns the topology; this system instantiates scene prefabs
 * for the physical parts of the generated dungeon.
 */
int nacre_dungeon_system_initialize(void *data);
void nacre_dungeon_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void nacre_dungeon_system_terminate(void *data);

/*
 * The returned map is read-only and is owned by the active system.
 * Pointer is invalidated on regeneration or system shutdown.
 * NULL when the system has no successfully generated layout.
 */
const NacreDungeonMap *nacre_dungeon_system_map_get(void);
/* Return world-space center of the generated grid.  False without a map. */
bool nacre_dungeon_system_world_origin_get(Vec3 *out);
/*
 * Update the scene visibility from the player's logical tiles.
 * During a doorway crossing pass (source,destination); once the step finishes
 * pass (destination,destination). Both regions are temporarily visible during
 * a crossing. Non-rendering gameplay systems and the logical map are intact.
 */
bool nacre_dungeon_system_visible_tiles_set(u32 source, u32 destination);

//@system name=NacreDungeonSystem initialize=nacre_dungeon_system_initialize update=nacre_dungeon_system_update terminate=nacre_dungeon_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
typedef struct NacreDungeonSystem
{
  //@begin_group "Dungeon Generation"
  /** Deterministic UTF-8 seed. Default: LDK-001. */
  XSmallstr seed;
  //@inspect min=24 max=100
  u32 width;
  //@inspect min=20 max=80
  u32 height;
  //@inspect min=8 max=32
  u32 rooms_min;
  //@inspect min=8 max=32
  u32 rooms_max;
  //@inspect min=5 max=12
  u32 max_room_size;
  //@inspect min=2 max=5
  u32 max_corridor_tiles;
  //@inspect slider min=0 max=0.75
  float extra_connections;
  //@inspect slider min=0 max=0.6
  float irregular_rooms;
  //@inspect slider min=0 max=1
  float adjacent_rooms;
  //@inspect slider min=0 max=1
  float alternative_walls;
  //@inspect slider min=0 max=1
  float cracked_fraction;
  //@end_group

  //@begin_group "Dungeon Prefabs"
  /** Centered-at-base horizontal floor, one tile per instance. */
  LDKAssetPrefab floor_prefab;
  /** 1 m walls: the prefab mesh is already aligned with X or Z. */
  LDKAssetPrefab wall_x_1m_prefab;
  LDKAssetPrefab wall_z_1m_prefab;
  /** Optional 2 m walls: adjacent ordinary walls are combined when set. */
  LDKAssetPrefab wall_x_2m_prefab;
  LDKAssetPrefab wall_z_2m_prefab;
  /** A normal OPEN boundary becomes exactly one passage prefab. */
  LDKAssetPrefab passage_x_prefab;
  LDKAssetPrefab passage_z_prefab;
  /** Traversable alternate links (one-attack cracked wall or open hole). */
  LDKAssetPrefab hole_x_prefab;
  LDKAssetPrefab hole_z_prefab;
  LDKAssetPrefab cracked_x_prefab;
  LDKAssetPrefab cracked_z_prefab;
  //@end_group

  //@begin_group "Future Prefabs (not generated yet)"
  /** Reserved for future generator stages. Stage 1 has no door/gate IDs. */
  LDKAssetPrefab door_x_prefab;
  LDKAssetPrefab door_z_prefab;
  LDKAssetPrefab portcullis_x_prefab;
  LDKAssetPrefab portcullis_z_prefab;
  LDKAssetPrefab pillar_intact_prefab;
  LDKAssetPrefab pillar_broken_prefab;
  //@end_group

  //@begin_group "Scene Placement"
  /** World-space center of the generated dungeon, at floor level (Y). */
  Vec3 world_origin;
  //@end_group

  //@begin_group "Room Visibility"
  /** Show the full dungeon for debugging. Default: false (current room only). */
  bool show_entire_dungeon;
  //@inspect readonly runtime
  u32 visible_instance_count;
  //@inspect readonly runtime
  u32 active_visibility_region;
  //@end_group

  //@begin_group "Generation Control"
  /** One generation attempt on system start. */
  bool generate_on_start;
  /** Set true during play to regenerate with the current settings. */
  bool regenerate;
  //@end_group

  //@begin_group "Generation Result"
  //@inspect readonly runtime
  bool layout_generated;
  //@inspect readonly runtime
  bool layout_valid;
  /** NacreDungeonResult numerical value; see nacre_dungeon_generator.h. */
  //@inspect readonly runtime
  u32 last_result;
  //@inspect readonly runtime
  XSmallstr status;
  //@inspect readonly runtime
  u32 generated_entity_count;
  //@inspect readonly runtime
  u32 missing_prefab_pieces;
  //@inspect readonly runtime
  u32 planned_piece_count;
  //@inspect readonly runtime
  u32 generation_revision;
  //@inspect readonly runtime
  u32 room_count;
  //@inspect readonly runtime
  u32 link_count;
  //@inspect readonly runtime
  u32 edge_count;
  //@inspect readonly runtime
  u32 normalized_micro_corridors;
  //@inspect readonly runtime
  u32 shortest_path_length;
  //@inspect readonly runtime
  u32 entry_tile;
  //@inspect readonly runtime
  u32 exit_tile;
  //@end_group
} NacreDungeonSystem;

#endif // NACRE_DUNGEON_SYSTEM_H
