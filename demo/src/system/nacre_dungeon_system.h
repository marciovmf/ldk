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
/* Runtime action: break a neighboring cracked wall. Updates navigation AND
 * its rendered prefab atomically; does not reroll the generated layout. */
bool nacre_dungeon_system_cracked_wall_break(u32 from, u32 to);
/* Called only when gameplay has discovered a hidden entrance. */
bool nacre_dungeon_system_secret_open(u32 from, u32 to);
/* Every door/gate is a separate prefab placed in a persistent Passage
 * prefab. Opening plays its prefab-authored "open.anim" keyframe clip once
 * (never autoplay/loop). If the clip is absent, only the barrier visuals
 * disappear. Navigation unlocks on interaction, not at animation completion.
 * Portcullises still require the discovered totem and correct password. */
bool nacre_dungeon_system_door_open(u32 from, u32 to);
bool nacre_dungeon_system_portcullis_open(
    u32 from, u32 to, const NacreDungeonDirection sequence[4]);
/* Discovering reveals a totem's color and password. Copy the result to
 * avoid holding pointers across regeneration. */
bool nacre_dungeon_system_totem_discover(u32 tile, NacreDungeonTotem *out);
/* Keys and levers are distinct only in visual/presentation data here;
 * both can be activated at their tile. Completed objectives remain hidden. */
bool nacre_dungeon_system_objective_complete(u32 tile);
bool nacre_dungeon_system_exit_can_use(u32 player_tile);

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
  //@inspect min=0 max=3
  u32 secret_rooms_max;
  //@inspect min=0 max=3
  u32 cave_tunnels_max;
  //@inspect min=0 max=18
  u32 pillar_count;
  //@inspect slider min=0 max=1
  float pillar_broken_fraction;
  //@end_group

  //@begin_group "Exit Progression"
  /** 0=Free, 1=Keys, 2=Levers. Affects next regeneration. */
  //@inspect min=0 max=2
  u32 exit_requirement;
  /** Required objectives, 1..6; ignored for Free. */
  //@inspect min=1 max=6
  u32 objective_count;
  //@end_group

  //@begin_group "Dungeon Prefabs"
  /** Optional generic visual placeholder for any missing/unavailable prefab.
   * Its transform uses the same centered-at-base pivot as dungeon pieces.
   * Instances made from this slot are renamed to the missing piece type. */
  LDKAssetPrefab fallback_prefab;
  /** Centered-at-base horizontal floor, one tile per instance. */
  LDKAssetPrefab floor_prefab;
  /** All directional prefabs are authored spanning +X (pivot at base center).
   * Z-aligned placements reuse the same asset rotated 90 degrees around Y.
   * The optional 2 m wall is used only for safe, region-consistent merges. */
  LDKAssetPrefab wall_1m_prefab;
  LDKAssetPrefab wall_2m_prefab;
  /** Architectural passage, independent of any moving door or portcullis. */
  LDKAssetPrefab passage_prefab;
  LDKAssetPrefab hole_prefab;
  LDKAssetPrefab cracked_prefab;
  //@end_group

  //@begin_group "Additional Dungeon Prefabs"
  /** Regular doors open freely. Colored portcullises require passwords.
   * Door and portcullis prefabs contain only the moving blocker; both are
   * placed alongside a persistent passage, never instead of it. */
  LDKAssetPrefab door_prefab;
  LDKAssetPrefab portcullis_prefab;
  /** Optional colored overrides; absent ones use portcullis_prefab. */
  LDKAssetPrefab portcullis_red_prefab;
  LDKAssetPrefab portcullis_green_prefab;
  LDKAssetPrefab portcullis_blue_prefab;
  LDKAssetPrefab pillar_intact_prefab;
  LDKAssetPrefab pillar_broken_prefab;
  /** Optional floor-standing markers. Pivot at base, centered in tile. */
  LDKAssetPrefab totem_prefab;
  LDKAssetPrefab entry_prefab;
  LDKAssetPrefab exit_prefab;
  LDKAssetPrefab objective_key_prefab;
  LDKAssetPrefab objective_lever_prefab;
  /** Optional: different floor/walls for excavated cave segments. If absent,
   * the ordinary structural prefabs are used. */
  LDKAssetPrefab cave_floor_prefab;
  LDKAssetPrefab cave_wall_prefab;
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
  /** Pieces not instantiated because no specific or fallback prefab was live. */
  u32 missing_prefab_pieces;
  /** Number of active pieces using Fallback Prefab instead of a specific one. */
  //@inspect readonly runtime
  u32 fallback_instance_count;
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
  u32 secret_room_count;
  //@inspect readonly runtime
  u32 cave_count;
  //@inspect readonly runtime
  u32 pillar_instances;
  //@inspect readonly runtime
  u32 treasure_room_count;
  //@inspect readonly runtime
  u32 door_count;
  //@inspect readonly runtime
  u32 portcullis_count;
  //@inspect readonly runtime
  u32 totem_count;
  //@inspect readonly runtime
  u32 objectives_total;
  //@inspect readonly runtime
  u32 objectives_completed;
  //@inspect readonly runtime
  u32 shortest_path_length;
  //@inspect readonly runtime
  u32 entry_tile;
  //@inspect readonly runtime
  u32 exit_tile;
  //@end_group
} NacreDungeonSystem;

#endif // NACRE_DUNGEON_SYSTEM_H
