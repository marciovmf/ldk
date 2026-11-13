/**
 * @file nacre_dungeon_system.c
 * @brief Builds the first Nacre dungeon geometry as prefab-owned LDK entities.
 *
 * Ownership and rollback:
 * - The C11 generator is independent from LDK and cannot create entities.
 * - A new layout, placement list and generated entity subtree are prepared
 *   before replacing the currently visible dungeon.
 * - Each generated prefab's root is parented under one system-owned root.
 * - LDK entity destruction orphans children; destroy the FULL subtree
 *   recursively, including nested prefab children, on reroll and shutdown.
 * - Missing/unassigned prefab slots use the generic fallback when configured;
 *   otherwise only the corresponding pieces are skipped. A structural
 *   instantiate/transform error rolls back the new subtree and keeps the
 *   previous dungeon intact.
 *
 * Coordinates:
 * - The generator grid spans [0,width] x [0,height] in XZ, Y up.
 * - Local prefab pivots are at the center of the base and authored along X.
 *   Z placements rotate the prefab root +90 degrees about Y (pivot unchanged).
 * - The whole dungeon is centered on system->world_origin.
 *
 * Features: rooms (including peripheral treasures), caves, secret rooms,
 * pillars, doors, independent color-coded portcullises, totems and objectives.
 */
#include "nacre_dungeon_system.h"
#include "nacre_dungeon_visibility.h"
#include "nacre_dungeon_progression.h"

#include <component/ldk_transform.h>
#include <component/ldk_keyframe_animation_source.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <stdlib.h>
#include <string.h>

/* A generated prefab can contain several nested renderers or lights. Record
 * their authored enabled flags so hiding a room never enables a component
 * that its prefab deliberately disabled. We do not disable transforms or
 * gameplay/interaction components: visibility is purely presentation. */
typedef struct NacreDungeonDrawable
{
  LDKEntity entity;
  u32 type;
  bool authored_enabled;
} NacreDungeonDrawable;

typedef struct NacreDungeonVisualPiece
{
  /* One logical placement, whether its prefab slot was assigned or not.
   * Retaining identity permits swapping ONE broken-wall prefab in place. */
  NacreDungeonPlacement placement;
  LDKEntity root;
  u16 regions[4];
  u8 region_count;
  u32 first_drawable;
  u32 drawable_count;
  bool visible;
  bool using_fallback;
} NacreDungeonVisualPiece;

typedef struct NacreDungeonSceneVisibility
{
  NacreDungeonVisibilityMap region_map;
  NacreDungeonVisualPiece *pieces;
  u32 piece_count;
  NacreDungeonDrawable *drawables;
  u32 drawable_count;
  u32 drawable_capacity;
  u16 active_region;
  u16 crossing_region;
  u32 visible_instance_count;
  bool applied;
  bool last_show_entire_dungeon;
} NacreDungeonSceneVisibility;

typedef struct NacreDungeonRuntime
{
  NacreDungeonSystem *owner;
  NacreDungeonMap map;
  /* The authored topology is validated before it is committed. Runtime
   * opening of optional shortcuts can legitimately shorten the BFS route,
   * so do not re-run the original generation-only route assertion. */
  bool generated_layout_valid;
  Vec3 generated_origin;
  LDKEntity generated_root;
  u32 generated_entity_count;
  u32 missing_prefab_pieces;
  u32 fallback_instance_count;
  u32 planned_piece_count;
  NacreDungeonSceneVisibility visibility;
} NacreDungeonRuntime;

/* One scene-owned instance, matching the current LDK single active scene. */
static NacreDungeonRuntime nacre_dungeon_runtime;

/* Register only components whose enabled flag affects presentation in
 * LDK. The renderer already respects the ECS disabled flag, so no renderer
 * special cases, transform hacks, entity destruction or prefab duplication. */
static const u32 nacre_dungeon_drawable_types[] = {
  LDK_COMPONENT_TYPE_MESH_SOURCE,
  LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE,
  LDK_COMPONENT_TYPE_LINE,
  LDK_COMPONENT_TYPE_TRAIL,
  LDK_COMPONENT_TYPE_TEXT3D,
  LDK_COMPONENT_TYPE_PARTICLE_EMITTER,
  LDK_COMPONENT_TYPE_POINT_LIGHT,
  LDK_COMPONENT_TYPE_SPOT_LIGHT
};

static void nacre_dungeon_visibility_scene_release(
    NacreDungeonSceneVisibility *scene)
{
  nacre_dungeon_visibility_release(&scene->region_map);
  free(scene->pieces);
  free(scene->drawables);
  memset(scene, 0, sizeof(*scene));
}

static bool nacre_dungeon_visibility_collect(
    NacreDungeonSceneVisibility *scene, LDKEntity entity)
{
  for (size_t i = 0u; i < sizeof(nacre_dungeon_drawable_types) /
      sizeof(nacre_dungeon_drawable_types[0]); ++i)
  {
    const u32 type = nacre_dungeon_drawable_types[i];
    if (!ldk_ecs_component_get_const(entity, type)) continue;
    if (scene->drawable_count == scene->drawable_capacity)
    {
      const u32 next_capacity = scene->drawable_capacity ?
          scene->drawable_capacity * 2u : 256u;
      if (next_capacity <= scene->drawable_capacity)
        return false;
      void *memory = realloc(scene->drawables,
          (size_t)next_capacity * sizeof(*scene->drawables));
      if (!memory) return false;
      scene->drawables = (NacreDungeonDrawable *)memory;
      scene->drawable_capacity = next_capacity;
    }
    scene->drawables[scene->drawable_count++] = (NacreDungeonDrawable){
      entity, type, ldk_ecs_component_is_enabled(entity, type)};
  }

  const LDKTransform *transform = (const LDKTransform *)
      ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform) return true;
  LDKEntity child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform = (const LDKTransform *)
        ldk_ecs_component_get_const(child, LDK_COMPONENT_TYPE_TRANSFORM);
    /* Record the sibling before walking the subtree. */
    const LDKEntity next = child_transform ?
        child_transform->next_sibling : x_handle_null();
    if (!nacre_dungeon_visibility_collect(scene, child)) return false;
    child = next;
  }
  return true;
}

static void nacre_dungeon_system_apply_visibility(
    NacreDungeonSystem *system)
{
  NacreDungeonSceneVisibility *scene = &nacre_dungeon_runtime.visibility;
  if (!scene->pieces) return;
  const bool show_all = system->show_entire_dungeon;
  u32 visible_count = 0u;
  for (u32 i = 0u; i < scene->piece_count; ++i)
  {
    NacreDungeonVisualPiece *piece = &scene->pieces[i];
    bool visible = show_all;
    if (!visible)
    {
      for (u32 j = 0u; j < piece->region_count; ++j)
      {
        const u16 region = piece->regions[j];
        if (region == scene->active_region ||
            (scene->crossing_region && region == scene->crossing_region))
        {
          visible = true;
          break;
        }
      }
    }
    if (visible && !x_handle_is_null(piece->root)) ++visible_count;
    if (scene->applied && piece->visible == visible) continue;
    for (u32 j = 0u; j < piece->drawable_count; ++j)
    {
      const NacreDungeonDrawable *drawable =
          &scene->drawables[piece->first_drawable + j];
      const bool enabled = visible && drawable->authored_enabled;
      if (ldk_ecs_component_is_enabled(drawable->entity, drawable->type) !=
          enabled)
        (void)ldk_ecs_component_enabled_set(
            drawable->entity, drawable->type, enabled);
    }
    piece->visible = visible;
  }
  scene->applied = true;
  scene->last_show_entire_dungeon = show_all;
  scene->visible_instance_count = visible_count;
  system->visible_instance_count = visible_count;
  system->active_visibility_region = scene->active_region;
}

static void nacre_dungeon_system_set_status(
    NacreDungeonSystem *system, const char *text)
{
  x_smallstr_from_cstr(&system->status, text);
}

static bool nacre_dungeon_system_prefab_live(
    LDKAssetManager *assets, LDKAssetPrefab prefab)
{
  return assets && !x_handle_is_null(prefab.h) &&
      ldk_asset_manager_prefab_is_alive(assets, prefab);
}

/* A freshly created unconfigured Comet system may be zeroed instead of
 * containing the canonical null asset handle. Normalize only zero handles.
 * IMPORTANT: never erase valid prefab references that a user assigned in the
 * inspector before the first Play / system initialization. */
static void nacre_dungeon_system_normalize_prefabs(NacreDungeonSystem *system)
{
  LDKAssetPrefab *prefabs[] = {
    &system->fallback_prefab,
    &system->floor_prefab,
    &system->wall_1m_prefab, &system->wall_2m_prefab,
    &system->passage_prefab, &system->hole_prefab, &system->cracked_prefab,
    &system->door_prefab, &system->portcullis_prefab,
    &system->portcullis_red_prefab, &system->portcullis_green_prefab,
    &system->portcullis_blue_prefab,
    &system->pillar_intact_prefab, &system->pillar_broken_prefab,
    &system->totem_prefab, &system->entry_prefab, &system->exit_prefab,
    &system->objective_key_prefab, &system->objective_lever_prefab,
    &system->cave_floor_prefab, &system->cave_wall_prefab
  };
  for (size_t i = 0; i < sizeof(prefabs) / sizeof(prefabs[0]); ++i)
  {
    if (prefabs[i]->h.index == 0u && prefabs[i]->h.version == 0u)
    {
      *prefabs[i] = ldk_asset_prefab_null();
    }
  }
}

/* Comet/scene serialization can provide a PARTIALLY initialized instance.
 * The former all-or-nothing test skipped initialization as soon as just ONE
 * dimension or room-count field was set, leaving the remaining required
 * positive integers at zero and causing NACRE_DUNGEON_INVALID_CONFIG.
 *
 * Zero is invalid for every integer below, so only fill those missing fields.
 * Never overwrite nonzero authoring choices, including inconsistent ones:
 * validation must report those to the user instead of silently fixing them.
 * Probability zero is VALID in partially configured instances; only fully
 * pristine instances receive the default probabilities. Prefab references
 * must never be changed by default initialization. */
static void nacre_dungeon_system_apply_defaults(NacreDungeonSystem *system)
{
  NacreDungeonConfig defaults;
  const bool new_instance =
      system->width == 0u && system->height == 0u &&
      system->rooms_min == 0u && system->rooms_max == 0u &&
      system->max_room_size == 0u && system->max_corridor_tiles == 0u;
  nacre_dungeon_generator_config_defaults(&defaults);
  if (x_smallstr_length(&system->seed) == 0u)
  {
    x_smallstr_from_cstr(&system->seed, defaults.seed);
  }
  if (system->width == 0u) system->width = defaults.width;
  if (system->height == 0u) system->height = defaults.height;
  if (system->rooms_min == 0u) system->rooms_min = defaults.rooms_min;
  if (system->rooms_max == 0u) system->rooms_max = defaults.rooms_max;
  if (system->max_room_size == 0u) system->max_room_size = defaults.max_room_size;
  if (system->max_corridor_tiles == 0u)
    system->max_corridor_tiles = defaults.max_corridor_tiles;
  if (system->objective_count == 0u)
    system->objective_count = defaults.objective_count;
  if (new_instance)
  {
    system->extra_connections = (float)defaults.extra_connections;
    system->irregular_rooms = (float)defaults.irregular_rooms;
    system->adjacent_rooms = (float)defaults.adjacent_rooms;
    system->alternative_walls = (float)defaults.alternative_walls;
    system->cracked_fraction = (float)defaults.cracked_fraction;
    system->secret_rooms_max = defaults.secret_rooms_max;
    system->cave_tunnels_max = defaults.cave_tunnels_max;
    system->pillar_count = defaults.pillar_count;
    system->pillar_broken_fraction = (float)defaults.pillar_broken_fraction;
    system->exit_requirement = defaults.exit_requirement;
    system->generate_on_start = true;
  }
}

static bool nacre_dungeon_system_get_config(
    const NacreDungeonSystem *system, NacreDungeonConfig *config)
{
  const char *seed = x_smallstr_cstr(&system->seed);
  size_t bytes;
  if (!seed)
  {
    return false;
  }
  bytes = strlen(seed);
  if (bytes == 0u || bytes > NACRE_DUNGEON_MAX_SEED_LENGTH)
  {
    return false;
  }
  nacre_dungeon_generator_config_defaults(config);
  memcpy(config->seed, seed, bytes + 1u);
  config->width = system->width;
  config->height = system->height;
  config->rooms_min = system->rooms_min;
  config->rooms_max = system->rooms_max;
  config->max_room_size = system->max_room_size;
  config->max_corridor_tiles = system->max_corridor_tiles;
  config->extra_connections = (double)system->extra_connections;
  config->irregular_rooms = (double)system->irregular_rooms;
  config->adjacent_rooms = (double)system->adjacent_rooms;
  config->alternative_walls = (double)system->alternative_walls;
  config->cracked_fraction = (double)system->cracked_fraction;
  config->secret_rooms_max = system->secret_rooms_max;
  config->cave_tunnels_max = system->cave_tunnels_max;
  config->pillar_count = system->pillar_count;
  config->pillar_broken_fraction = (double)system->pillar_broken_fraction;
  config->exit_requirement = system->exit_requirement;
  config->objective_count = system->objective_count;
  return true;
}

/* The placement kind retains its world orientation for collision and culling,
 * but the user assigns only ONE X-authored prefab for each geometry type. */
static LDKAssetPrefab nacre_dungeon_system_part_prefab(
    const NacreDungeonSystem *system, NacreDungeonPartKind kind)
{
  switch (kind)
  {
    case NACRE_DUNGEON_PART_FLOOR: return system->floor_prefab;
    case NACRE_DUNGEON_PART_WALL_X_1M:
    case NACRE_DUNGEON_PART_WALL_Z_1M: return system->wall_1m_prefab;
    case NACRE_DUNGEON_PART_WALL_X_2M:
    case NACRE_DUNGEON_PART_WALL_Z_2M: return system->wall_2m_prefab;
    case NACRE_DUNGEON_PART_PASSAGE_X:
    case NACRE_DUNGEON_PART_PASSAGE_Z: return system->passage_prefab;
    case NACRE_DUNGEON_PART_HOLE_X:
    case NACRE_DUNGEON_PART_HOLE_Z: return system->hole_prefab;
    case NACRE_DUNGEON_PART_CRACKED_X:
    case NACRE_DUNGEON_PART_CRACKED_Z: return system->cracked_prefab;
    case NACRE_DUNGEON_PART_SECRET_X:
    case NACRE_DUNGEON_PART_SECRET_Z: return system->wall_1m_prefab;
    case NACRE_DUNGEON_PART_PILLAR_INTACT: return system->pillar_intact_prefab;
    case NACRE_DUNGEON_PART_PILLAR_BROKEN: return system->pillar_broken_prefab;
    case NACRE_DUNGEON_PART_DOOR_X:
    case NACRE_DUNGEON_PART_DOOR_Z: return system->door_prefab;
    case NACRE_DUNGEON_PART_PORTCULLIS_X:
    case NACRE_DUNGEON_PART_PORTCULLIS_Z: return system->portcullis_prefab;
    case NACRE_DUNGEON_PART_TOTEM: return system->totem_prefab;
    case NACRE_DUNGEON_PART_ENTRY: return system->entry_prefab;
    case NACRE_DUNGEON_PART_EXIT: return system->exit_prefab;
    case NACRE_DUNGEON_PART_OBJECTIVE_KEY: return system->objective_key_prefab;
    case NACRE_DUNGEON_PART_OBJECTIVE_LEVER: return system->objective_lever_prefab;
    case NACRE_DUNGEON_PART_CAVE_FLOOR:
      return x_handle_is_null(system->cave_floor_prefab.h) ?
          system->floor_prefab : system->cave_floor_prefab;
    case NACRE_DUNGEON_PART_CAVE_WALL_X:
    case NACRE_DUNGEON_PART_CAVE_WALL_Z:
      return x_handle_is_null(system->cave_wall_prefab.h) ?
          system->wall_1m_prefab : system->cave_wall_prefab;
    default: return ldk_asset_prefab_null();
  }
}

/* Rotate only directional X-authored geometry. The logical placement retains
 * X/Z for region ownership; floor tiles and props keep prefab orientation. */
static bool nacre_dungeon_system_part_along_z(NacreDungeonPartKind kind)
{
  switch (kind)
  {
    case NACRE_DUNGEON_PART_WALL_Z_1M:
    case NACRE_DUNGEON_PART_WALL_Z_2M:
    case NACRE_DUNGEON_PART_PASSAGE_Z:
    case NACRE_DUNGEON_PART_HOLE_Z:
    case NACRE_DUNGEON_PART_CRACKED_Z:
    case NACRE_DUNGEON_PART_SECRET_Z:
    case NACRE_DUNGEON_PART_CAVE_WALL_Z:
    case NACRE_DUNGEON_PART_DOOR_Z:
    case NACRE_DUNGEON_PART_PORTCULLIS_Z:
      return true;
    default:
      return false;
  }
}

/* Preserve an authored root rotation while adding the grid orientation.
 * Apply this to the instantiated ROOT, so nested 'open' animations stay
 * in prefab-local space and work identically on X and Z passages. */
static bool nacre_dungeon_system_instance_place(
    LDKEntity instance, LDKEntity parent, Vec3 position,
    NacreDungeonPartKind kind)
{
  if (!ldk_transform_set_parent(instance, parent) ||
      !ldk_transform_set_local_position(instance, position)) return false;
  if (!nacre_dungeon_system_part_along_z(kind)) return true;

  Quat authored;
  if (!ldk_transform_get_local_rotation(instance, &authored)) return false;
  const Quat yaw = quat_axis_angle(
      vec3_make(0.0f, 1.0f, 0.0f), STDXM_PI * .5f);
  return ldk_transform_set_local_rotation(instance, quat_mul(yaw, authored));
}

/* Per-color gate prefabs are optional: colors are authored in the logical
 * passage record, not inferred from a sprite or material. Derive the two
 * neighboring tile IDs from the already placed one-meter edge. Never try to
 * color a generic wall or infer the color from its visibility owner. */
static LDKAssetPrefab nacre_dungeon_system_placement_prefab(
    const NacreDungeonSystem *system, const NacreDungeonMap *map,
    const NacreDungeonPlacement *piece, LDKAssetManager *assets)
{
  const bool along_x = piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_X;
  const bool along_z = piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_Z;
  LDKAssetPrefab fallback = nacre_dungeon_system_part_prefab(
      system, piece->kind);
  if ((!along_x && !along_z) || !map || !map->passages)
    return fallback;

  const u32 ix = (u32)piece->x;
  const u32 iz = (u32)piece->z;
  u32 a, b;
  if (along_x)
  {
    /* Horizontal wall: center (cell_x + .5, boundary_z). */
    if (ix >= map->width || iz == 0u || iz >= map->height)
      return fallback;
    a = (iz - 1u) * map->width + ix;
    b = a + map->width;
  }
  else
  {
    /* Vertical wall: center (boundary_x, cell_z + .5). */
    if (ix == 0u || ix >= map->width || iz >= map->height)
      return fallback;
    a = iz * map->width + ix - 1u;
    b = a + 1u;
  }

  for (u32 i = 0u; i < map->passage_count; ++i)
  {
    const NacreDungeonPassage *passage = &map->passages[i];
    if (passage->kind != NACRE_DUNGEON_PASSAGE_PORTCULLIS ||
        !((passage->a == a && passage->b == b) ||
          (passage->a == b && passage->b == a)))
      continue;

    LDKAssetPrefab selected = ldk_asset_prefab_null();
    switch (passage->color)
    {
      case NACRE_DUNGEON_COLOR_RED:
        selected = system->portcullis_red_prefab;
        break;
      case NACRE_DUNGEON_COLOR_GREEN:
        selected = system->portcullis_green_prefab;
        break;
      case NACRE_DUNGEON_COLOR_BLUE:
        selected = system->portcullis_blue_prefab;
        break;
      default:
        break;
    }
    return nacre_dungeon_system_prefab_live(assets, selected) ?
        selected : fallback;
  }
  return fallback;
}

/* Logical type names are used only for placeholder entity names. An
 * unassigned asset handle has no filename to query; these identifiers match
 * the corresponding authoring slots / prefab naming convention. */
static const char *nacre_dungeon_system_part_name(NacreDungeonPartKind kind)
{
  static const char *const names[NACRE_DUNGEON_PART_KIND_COUNT] = {
    "nacre_floor",
    "nacre_wall_1m", "nacre_wall_1m",
    "nacre_wall_2m", "nacre_wall_2m",
    "nacre_passage", "nacre_passage",
    "nacre_hole", "nacre_hole",
    "nacre_cracked", "nacre_cracked",
    "nacre_wall_1m", "nacre_wall_1m", /* Secret uses wall slot. */
    "nacre_pillar_intact", "nacre_pillar_broken",
    "nacre_cave_floor", "nacre_cave_wall", "nacre_cave_wall",
    "nacre_door", "nacre_door",
    "nacre_portcullis", "nacre_portcullis",
    "nacre_totem", "nacre_entry", "nacre_exit",
    "nacre_objective_key", "nacre_objective_lever"
  };
  return (unsigned)kind < NACRE_DUNGEON_PART_KIND_COUNT ?
      names[kind] : "nacre_unknown";
}

typedef struct NacreDungeonSelectedPrefab
{
  LDKAssetPrefab asset;
  const char *missing_name;
  bool using_fallback;
} NacreDungeonSelectedPrefab;

/* Use specialized prefab chains first (e.g., red gate -> ordinary gate,
 * cave floor -> normal floor). Only a missing LAST usable specialization
 * selects the all-purpose fallback. This preserves existing visuals for
 * projects which already use the prior optional-slot conventions. */
static NacreDungeonSelectedPrefab nacre_dungeon_system_resolve_prefab(
    const NacreDungeonSystem *system, const NacreDungeonMap *map,
    const NacreDungeonPlacement *piece, LDKAssetManager *assets)
{
  NacreDungeonSelectedPrefab result;
  result.asset = nacre_dungeon_system_placement_prefab(
      system, map, piece, assets);
  result.missing_name = nacre_dungeon_system_part_name(piece->kind);
  result.using_fallback = false;
  if (nacre_dungeon_system_prefab_live(assets, result.asset))
    return result;

  /* A gate's color is part of its identity even if both its color-specific
   * and ordinary portcullis prefab are absent. Query the passage at this
   * exact boundary, not the region or wall material. */
  const bool along_x = piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_X;
  const bool along_z = piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_Z;
  if ((along_x || along_z) && map && map->passages)
  {
    const u32 ix = (u32)piece->x;
    const u32 iz = (u32)piece->z;
    u32 a = 0u, b = 0u;
    bool valid = false;
    if (along_x && ix < map->width && iz > 0u && iz < map->height)
    {
      a = (iz - 1u) * map->width + ix;
      b = a + map->width;
      valid = true;
    }
    else if (along_z && ix > 0u && ix < map->width && iz < map->height)
    {
      a = iz * map->width + ix - 1u;
      b = a + 1u;
      valid = true;
    }
    if (valid)
    {
      for (u32 i = 0u; i < map->passage_count; ++i)
      {
        const NacreDungeonPassage *passage = &map->passages[i];
        if (passage->kind != NACRE_DUNGEON_PASSAGE_PORTCULLIS ||
            !((passage->a == a && passage->b == b) ||
              (passage->a == b && passage->b == a))) continue;
        switch (passage->color)
        {
          case NACRE_DUNGEON_COLOR_RED:
            result.missing_name = "nacre_portcullis_red";
            break;
          case NACRE_DUNGEON_COLOR_GREEN:
            result.missing_name = "nacre_portcullis_green";
            break;
          case NACRE_DUNGEON_COLOR_BLUE:
            result.missing_name = "nacre_portcullis_blue";
            break;
          default: break;
        }
        break;
      }
    }
  }

  if (nacre_dungeon_system_prefab_live(assets, system->fallback_prefab))
  {
    result.asset = system->fallback_prefab;
    result.using_fallback = true;
  }
  return result;
}

/* The current LDK source component addresses clips by index. Its asset
 * metadata, however, retains the original .anim path. Resolve the explicit
 * prefab-authored clip named "open.anim" without assuming it occupies slot 0.
 * This does not mutate a shared prefab or asset: only the instantiated source
 * component's playback state changes. */
static i32 nacre_dungeon_system_open_clip_find(
    const LDKKeyFrameAnimationSource *source, LDKAssetManager *assets)
{
  if (!source || !assets || !source->animations) return -1;
  for (u32 i = 0u; i < source->animation_count; ++i)
  {
    LDKAssetKeyframeAnimation animation = source->animations[i];
    if (!ldk_asset_manager_keyframe_animation_is_alive(assets, animation))
      continue;
    const LDKAssetHandle handle = {.h = animation.h};
    const LDKAssetInfo *info = ldk_asset_get_info_const(assets, handle);
    if (!info || !info->asset_path.length) continue;
    const char *name = info->asset_path.buf;
    for (const char *c = name; *c; ++c)
      if (*c == '/' || *c == '\\') name = c + 1;
    if (strcmp(name, "open.anim") == 0 || strcmp(name, "open") == 0)
      return (i32)i;
  }
  return -1;
}

/* The animation source can be attached to a nested entity in a prefab.
 * An open clip is prepared before its first update: it must never autoplay,
 * and it must hold the last frame rather than looping closed again. */
static bool nacre_dungeon_system_open_animation(
    LDKEntity entity, LDKAssetManager *assets, bool play)
{
  LDKKeyFrameAnimationSource *source = (LDKKeyFrameAnimationSource *)
      ldk_ecs_component_get(entity, LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE);
  if (source)
  {
    const i32 clip = nacre_dungeon_system_open_clip_find(source, assets);
    if (clip >= 0)
    {
      source->play_on_start = false;
      source->loop = false;
      if (ldk_keyframe_animation_source_set_current(entity, clip))
      {
        if (play)
        {
          if (ldk_keyframe_animation_source_play(entity)) return true;
        }
        else
        {
          ldk_keyframe_animation_source_stop(entity);
          return true;
        }
      }
    }
  }

  const LDKTransform *transform = (const LDKTransform *)
      ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
  LDKEntity child = transform ? transform->first_child : x_handle_null();
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform = (const LDKTransform *)
        ldk_ecs_component_get_const(child, LDK_COMPONENT_TYPE_TRANSFORM);
    const LDKEntity next = child_transform ?
        child_transform->next_sibling : x_handle_null();
    if (nacre_dungeon_system_open_animation(child, assets, play)) return true;
    child = next;
  }
  return false;
}

static bool nacre_dungeon_system_is_animated_barrier(NacreDungeonPartKind kind)
{
  return kind == NACRE_DUNGEON_PART_DOOR_X ||
      kind == NACRE_DUNGEON_PART_DOOR_Z ||
      kind == NACRE_DUNGEON_PART_PORTCULLIS_X ||
      kind == NACRE_DUNGEON_PART_PORTCULLIS_Z;
}

/* LDK's ldk_ecs_entity_destroy() does NOT delete children. A prefab can
 * itself contain several child entities. Do not leak them on regeneration. */
static void nacre_dungeon_system_destroy_subtree(LDKEntity entity)
{
  if (x_handle_is_null(entity) ||
      !ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    return;
  }
  for (;;)
  {
    const LDKTransform *transform = (const LDKTransform *)
        ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!transform || x_handle_is_null(transform->first_child))
    {
      break;
    }
    nacre_dungeon_system_destroy_subtree(transform->first_child);
  }
  ldk_ecs_entity_destroy(entity);
}

/* Instantiate into a separate subtree. Do not mutate the old scene on
 * failure: the caller swaps roots only after the full build succeeds. */
static bool nacre_dungeon_system_instantiate(
    NacreDungeonSystem *system, const NacreDungeonMap *map,
    const NacreDungeonPlacementList *placements,
    NacreDungeonSceneVisibility *scene,
    LDKEntity *out_root, u32 *out_created, u32 *out_missing,
    u32 *out_fallback)
{
  LDKAssetManager *assets = (LDKAssetManager *)
      ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKEntity root = x_handle_null();
  u32 created = 0u, missing = 0u, fallback_count = 0u;
  bool any_live = false;

  /* Missing slots are legal. Each one uses the generic placeholder when
   * available; otherwise it remains a logical but not visual placement. */
  for (u32 i = 0; i < placements->count; ++i)
  {
    const NacreDungeonSelectedPrefab selected =
        nacre_dungeon_system_resolve_prefab(
            system, map, &placements->items[i], assets);
    if (nacre_dungeon_system_prefab_live(assets, selected.asset))
    {
      any_live = true;
    }
    else
    {
      ++missing;
    }
  }

  /* One record per logical placement, including pieces with no visual asset.
   * Region ownership comes from the LOGICAL map and exact placement
   * coordinates, not the render mesh or material. */
  scene->pieces = (NacreDungeonVisualPiece *)calloc(
      placements->count ? placements->count : 1u, sizeof(*scene->pieces));
  if (!scene->pieces) return false;

  if (!any_live)
  {
    *out_root = root;
    *out_created = 0u;
    *out_missing = missing;
    *out_fallback = 0u;
    return true;
  }

  root = ldk_ecs_entity_create();
  if (x_handle_is_null(root))
  {
    return false;
  }
  (void)ldk_ecs_entity_name_set(root, "Nacre Dungeon [generated]");
  if (!ldk_transform_set_local_position(root, system->world_origin))
  {
    nacre_dungeon_system_destroy_subtree(root);
    return false;
  }

  /* Build one visual record for EVERY logical placement. Keeping records for
   * missing prefab slots makes subsequent runtime wall interactions safe even
   * if the user has not assigned all assets yet. */
  scene->piece_count = placements->count;
  for (u32 i = 0; i < placements->count; ++i)
  {
    const NacreDungeonPlacement *piece = &placements->items[i];
    NacreDungeonVisualPiece *visual = &scene->pieces[i];
    visual->placement = *piece;
    visual->root = x_handle_null();
    visual->region_count = (u8)nacre_dungeon_visibility_piece_regions(
        map, &scene->region_map, piece, visual->regions);
    const NacreDungeonSelectedPrefab selected =
        nacre_dungeon_system_resolve_prefab(system, map, piece, assets);
    if (!nacre_dungeon_system_prefab_live(assets, selected.asset)) continue;

    LDKEntity instance = ldk_ecs_prefab_instantiate(selected.asset);
    if (x_handle_is_null(instance))
    {
      nacre_dungeon_system_destroy_subtree(root);
      return false;
    }
    const Vec3 local = vec3_make(
        piece->x - (float)map->width * .5f,
        0.0f,
        piece->z - (float)map->height * .5f);
    if (!nacre_dungeon_system_instance_place(
            instance, root, local, piece->kind))
    {
      /* The instance may not have been parented if the first operation
       * failed; this handles both outcomes safely. */
      if (!x_handle_is_null(ldk_transform_get_parent(instance)) &&
          !x_handle_is_null(root))
      {
        nacre_dungeon_system_destroy_subtree(root);
      }
      else
      {
        nacre_dungeon_system_destroy_subtree(instance);
        nacre_dungeon_system_destroy_subtree(root);
      }
      return false;
    }
    /* The prefab's authored root name describes the placeholder itself.
     * Rename only substituted instances to the MISSING logical prefab. */
    if (selected.using_fallback &&
        !ldk_ecs_entity_name_set(instance, selected.missing_name))
    {
      nacre_dungeon_system_destroy_subtree(root);
      return false;
    }
    /* Prepare the authored "open.anim" clip while the gate is still
     * closed. Do not autoplay the opening on room creation. */
    if (nacre_dungeon_system_is_animated_barrier(piece->kind))
      (void)nacre_dungeon_system_open_animation(instance, assets, false);
    visual->root = instance;
    visual->using_fallback = selected.using_fallback;
    if (selected.using_fallback) ++fallback_count;
    visual->first_drawable = scene->drawable_count;
    if (!nacre_dungeon_visibility_collect(scene, instance))
    {
      nacre_dungeon_system_destroy_subtree(root);
      return false;
    }
    visual->drawable_count = scene->drawable_count - visual->first_drawable;
    ++created;
  }

  *out_root = root;
  *out_created = created;
  *out_missing = missing;
  *out_fallback = fallback_count;
  return true;
}

static void nacre_dungeon_system_publish_map(NacreDungeonSystem *system)
{
  const NacreDungeonMap *map = &nacre_dungeon_runtime.map;
  system->layout_generated = map->cells != NULL;
  system->layout_valid = system->layout_generated &&
      nacre_dungeon_runtime.generated_layout_valid;
  system->generated_entity_count = nacre_dungeon_runtime.generated_entity_count;
  system->missing_prefab_pieces = nacre_dungeon_runtime.missing_prefab_pieces;
  system->fallback_instance_count = nacre_dungeon_runtime.fallback_instance_count;
  system->planned_piece_count = nacre_dungeon_runtime.planned_piece_count;
  if (!system->layout_generated)
  {
    system->room_count = 0u;
    system->link_count = 0u;
    system->edge_count = 0u;
    system->normalized_micro_corridors = 0u;
    system->secret_room_count = 0u;
    system->cave_count = 0u;
    system->pillar_instances = 0u;
    system->treasure_room_count = 0u;
    system->door_count = 0u;
    system->portcullis_count = 0u;
    system->totem_count = 0u;
    system->objectives_total = 0u;
    system->objectives_completed = 0u;
    system->shortest_path_length = 0u;
    system->entry_tile = 0u;
    system->exit_tile = 0u;
    return;
  }
  system->room_count = map->room_count;
  system->link_count = map->link_count;
  system->edge_count = map->edge_count;
  system->normalized_micro_corridors = map->normalized_micro_corridors;
  system->secret_room_count = map->secret_room_count;
  system->cave_count = map->cave_count;
  system->pillar_instances = map->pillar_count;
  system->treasure_room_count = map->treasure_count;
  system->totem_count = map->totem_count;
  system->objectives_total = map->objective_count;
  system->objectives_completed = 0u;
  system->door_count = 0u;
  system->portcullis_count = 0u;
  for (u32 i=0;i<map->passage_count;i++) {
    system->door_count += map->passages[i].kind == NACRE_DUNGEON_PASSAGE_DOOR;
    system->portcullis_count +=
        map->passages[i].kind == NACRE_DUNGEON_PASSAGE_PORTCULLIS;
  }
  for (u32 i=0;i<map->objective_count;i++)
    system->objectives_completed += map->objectives[i].completed;
  system->shortest_path_length = map->shortest_path_length;
  system->entry_tile = map->entry;
  system->exit_tile = map->exit;
}

static void nacre_dungeon_system_generate(NacreDungeonSystem *system)
{
  NacreDungeonConfig config;
  NacreDungeonMap next = {0};
  NacreDungeonPlacementList placements = {0};
  NacreDungeonSceneVisibility next_visibility = {0};
  NacreDungeonResult result;
  LDKEntity next_root = x_handle_null();
  u32 created = 0u, missing = 0u, fallback_count = 0u;

  if (!nacre_dungeon_system_get_config(system, &config))
  {
    system->last_result = NACRE_DUNGEON_INVALID_CONFIG;
    nacre_dungeon_system_set_status(system, "Invalid seed (empty or too long)");
    ldk_log_error("NacreDungeonSystem: invalid seed.\n");
    return;
  }

  /* Invalid settings are not a random generation failure. Explain precisely
   * which inspector value needs correction before attempting to allocate. */
  const char *config_error = nacre_dungeon_generator_config_error(&config);
  if (config_error)
  {
    system->last_result = NACRE_DUNGEON_INVALID_CONFIG;
    nacre_dungeon_system_set_status(system, config_error);
    ldk_log_error("NacreDungeonSystem: invalid config: %s; "
        "seed='%s' size=%ux%u rooms=%u..%u room_size=%u corridor=%u "
        "loops=%g irregular=%g adjacent=%g alternative=%g cracked=%g "
        "secrets=%u caves=%u pillars=%u broken=%g exit=%u goals=%u.\n",
        config_error, config.seed, config.width, config.height,
        config.rooms_min, config.rooms_max, config.max_room_size,
        config.max_corridor_tiles, config.extra_connections,
        config.irregular_rooms, config.adjacent_rooms,
        config.alternative_walls, config.cracked_fraction,
        config.secret_rooms_max, config.cave_tunnels_max,
        config.pillar_count, config.pillar_broken_fraction,
        config.exit_requirement, config.objective_count);
    return;
  }

  result = nacre_dungeon_generator_generate(&config, &next);
  if (result == NACRE_DUNGEON_OK &&
      !nacre_dungeon_generator_validate(&next))
  {
    result = NACRE_DUNGEON_INVALID_LAYOUT;
  }
  system->last_result = (u32)result;
  if (result != NACRE_DUNGEON_OK)
  {
    nacre_dungeon_generator_release(&next);
    nacre_dungeon_system_set_status(system,
        result == NACRE_DUNGEON_NO_VALID_LAYOUT ?
        "No valid layout for this seed" :
        result == NACRE_DUNGEON_INVALID_CONFIG ?
        "Invalid generation parameters" :
        result == NACRE_DUNGEON_OUT_OF_MEMORY ?
        "Out of memory" : "Invalid generated layout");
    ldk_log_error("NacreDungeonSystem: generation failed (%u), seed=%s.\n",
        (u32)result, config.seed);
    return;
  }

  LDKAssetManager *assets = (LDKAssetManager *)
      ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  const bool combine_walls = nacre_dungeon_system_prefab_live(
      assets, system->wall_2m_prefab);
  /* A long wall cannot cross a visibility ownership boundary. Establish
   * the region map first, then let placement pack only equivalent 1 m edges. */
  if (!nacre_dungeon_visibility_build(&next, &next_visibility.region_map))
  {
    nacre_dungeon_generator_release(&next);
    system->last_result = NACRE_DUNGEON_OUT_OF_MEMORY;
    nacre_dungeon_system_set_status(system, "Failed to construct visibility regions");
    return;
  }
  if (!nacre_dungeon_placements_build(
      &next, &next_visibility.region_map, combine_walls, &placements))
  {
    nacre_dungeon_visibility_scene_release(&next_visibility);
    nacre_dungeon_generator_release(&next);
    system->last_result = NACRE_DUNGEON_OUT_OF_MEMORY;
    nacre_dungeon_system_set_status(system, "Failed to construct placements");
    return;
  }
  if (!nacre_dungeon_system_instantiate(
      system, &next, &placements, &next_visibility,
      &next_root, &created, &missing, &fallback_count))
  {
    nacre_dungeon_visibility_scene_release(&next_visibility);
    nacre_dungeon_placements_release(&placements);
    nacre_dungeon_generator_release(&next);
    nacre_dungeon_system_set_status(system, "Prefab instantiation failed");
    ldk_log_error("NacreDungeonSystem: failed to instantiate generated prefabs.\n");
    return;
  }

  /* Commit: delete the old visual subtree only after the new one exists. */
  nacre_dungeon_system_destroy_subtree(nacre_dungeon_runtime.generated_root);
  nacre_dungeon_visibility_scene_release(&nacre_dungeon_runtime.visibility);
  nacre_dungeon_generator_release(&nacre_dungeon_runtime.map);
  nacre_dungeon_runtime.map = next;
  nacre_dungeon_runtime.generated_layout_valid = true;
  nacre_dungeon_runtime.visibility = next_visibility;
  /* Commit the same origin used when creating prefab transforms. Do not
   * follow uncommitted editor changes until the scene is regenerated. */
  nacre_dungeon_runtime.generated_origin = system->world_origin;
  nacre_dungeon_runtime.generated_root = next_root;
  nacre_dungeon_runtime.generated_entity_count = created;
  nacre_dungeon_runtime.missing_prefab_pieces = missing;
  nacre_dungeon_runtime.fallback_instance_count = fallback_count;
  nacre_dungeon_runtime.planned_piece_count = placements.count;
  nacre_dungeon_placements_release(&placements);

  /* The entry room is the default view even without the player controller.
   * The player will update it on spawn and every cell transition. */
  (void)nacre_dungeon_system_visible_tiles_set(next.entry, next.entry);
  ++system->generation_revision;
  nacre_dungeon_system_publish_map(system);
  nacre_dungeon_system_set_status(system, created == 0u ?
      "Assign dungeon prefabs, then Regenerate" :
      missing > 0u ? "Generated with missing prefab slots" :
      fallback_count > 0u ? "Dungeon instantiated with fallback prefabs" :
      "Dungeon instantiated");

  ldk_log_info("NacreDungeonSystem: seed=%s grid=%ux%u rooms=%u "
        "secrets=%u caves=%u pillars=%u treasure=%u gates=%u "
        "totems=%u goals=%u pieces=%u instances=%u missing=%u fallback=%u.\n",
      config.seed, config.width, config.height,
      system->room_count, system->secret_room_count,
      system->cave_count, system->pillar_instances,
      system->treasure_room_count, system->portcullis_count,
      system->totem_count, system->objectives_total,
      system->planned_piece_count, created, missing, fallback_count);
}

int nacre_dungeon_system_initialize(void *data)
{
  NacreDungeonSystem *system = (NacreDungeonSystem *)data;
  if (!system || nacre_dungeon_runtime.owner)
  {
    return -1;
  }
  memset(&nacre_dungeon_runtime, 0, sizeof(nacre_dungeon_runtime));
  nacre_dungeon_runtime.generated_root = x_handle_null();
  nacre_dungeon_runtime.owner = system;
  nacre_dungeon_system_normalize_prefabs(system);
  nacre_dungeon_system_apply_defaults(system);
  system->regenerate = false;
  system->generation_revision = 0u;
  system->visible_instance_count = 0u;
  system->active_visibility_region = 0u;
  system->last_result = NACRE_DUNGEON_OK;
  nacre_dungeon_system_publish_map(system);
  nacre_dungeon_system_set_status(system, "Ready");
  if (system->generate_on_start)
  {
    nacre_dungeon_system_generate(system);
  }
  return 0;
}

void nacre_dungeon_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  NacreDungeonSystem *system = (NacreDungeonSystem *)data;
  (void)group;
  (void)dt;
  if (!system || nacre_dungeon_runtime.owner != system) return;
  if (system->regenerate)
  {
    system->regenerate = false;
    nacre_dungeon_system_generate(system);
  }
  /* Inspector changes to Show Entire Dungeon work even while standing still. */
  if (nacre_dungeon_runtime.visibility.last_show_entire_dungeon !=
      system->show_entire_dungeon)
    nacre_dungeon_system_apply_visibility(system);
}

void nacre_dungeon_system_terminate(void *data)
{
  NacreDungeonSystem *system = (NacreDungeonSystem *)data;
  if (!system || nacre_dungeon_runtime.owner != system)
  {
    return;
  }
  nacre_dungeon_system_destroy_subtree(nacre_dungeon_runtime.generated_root);
  nacre_dungeon_visibility_scene_release(&nacre_dungeon_runtime.visibility);
  nacre_dungeon_runtime.generated_root = x_handle_null();
  nacre_dungeon_runtime.generated_entity_count = 0u;
  nacre_dungeon_runtime.missing_prefab_pieces = 0u;
  nacre_dungeon_runtime.fallback_instance_count = 0u;
  nacre_dungeon_runtime.planned_piece_count = 0u;
  nacre_dungeon_generator_release(&nacre_dungeon_runtime.map);
  nacre_dungeon_runtime.generated_layout_valid = false;
  nacre_dungeon_runtime.owner = NULL;
  system->visible_instance_count = 0u;
  system->active_visibility_region = 0u;
  nacre_dungeon_system_publish_map(system);
  nacre_dungeon_system_set_status(system, "Stopped");
}

const NacreDungeonMap *nacre_dungeon_system_map_get(void)
{
  if (!nacre_dungeon_runtime.owner || !nacre_dungeon_runtime.map.cells)
  {
    return NULL;
  }
  return &nacre_dungeon_runtime.map;
}

/* Expose the authored placement of the currently active dungeon to gameplay.
 * Navigation coordinates must match prefab placement exactly. */
bool nacre_dungeon_system_world_origin_get(Vec3 *out)
{
  if (!out || !nacre_dungeon_runtime.owner ||
      !nacre_dungeon_runtime.map.cells) return false;
  *out = nacre_dungeon_runtime.generated_origin;
  return true;
}

/* Invoked by the player controller only at the start/end of a logical move.
 * No per-frame scanning or world-space tests against thousands of entities. */
bool nacre_dungeon_system_visible_tiles_set(u32 source, u32 destination)
{
  NacreDungeonRuntime *runtime = &nacre_dungeon_runtime;
  NacreDungeonSceneVisibility *scene = &runtime->visibility;
  if (!runtime->owner || !runtime->map.cells ||
      !scene->region_map.region_by_tile ||
      source >= scene->region_map.tile_count ||
      destination >= scene->region_map.tile_count)
    return false;

  const u16 current = scene->region_map.region_by_tile[source];
  const u16 target = scene->region_map.region_by_tile[destination];
  if (!current || !target) return false;
  const u16 secondary = current == target ? 0u : target;
  if (!scene->applied || current != scene->active_region ||
      secondary != scene->crossing_region)
  {
    scene->active_region = current;
    scene->crossing_region = secondary;
    nacre_dungeon_system_apply_visibility(runtime->owner);
  }
  return true;
}

/* Replace a single boundary's visual instance. The logical map is NOT changed
 * until this function succeeds; a prefab instantiate failure cannot leave
 * collision and visual state out of sync. The old prefab is deleted only
 * after the replacement has been created and all its drawables collected. */
static bool nacre_dungeon_system_boundary_visual_replace(u32 from, u32 to,
    NacreDungeonPartKind old_x, NacreDungeonPartKind old_z,
    NacreDungeonPartKind next_x, NacreDungeonPartKind next_z)
{
  NacreDungeonRuntime *runtime=&nacre_dungeon_runtime;
  NacreDungeonMap *map=&runtime->map;
  NacreDungeonSystem *system=runtime->owner;
  if(!system||!map->cells||from>=map->width*map->height||
     to>=map->width*map->height)return false;
  const u32 fx=from%map->width,tx=to%map->width;
  const u32 fz=from/map->width,tz=to/map->width;
  const bool along_x=fz!=tz&&fx==tx&&
      (fz>tz?fz-tz:tz-fz)==1u;
  const bool along_z=fx!=tx&&fz==tz&&
      (fx>tx?fx-tx:tx-fx)==1u;
  if(!along_x&&!along_z)return false;
  const NacreDungeonPartKind old_kind=along_x?old_x:old_z;
  const NacreDungeonPartKind new_kind=along_x?next_x:next_z;
  const float x=along_x?(float)fx+.5f:(float)(fx>tx?fx:tx);
  const float z=along_z?(float)fz+.5f:(float)(fz>tz?fz:tz);
  NacreDungeonSceneVisibility *scene=&runtime->visibility;
  NacreDungeonVisualPiece *piece=NULL;
  for(u32 i=0;i<scene->piece_count;i++){
    NacreDungeonVisualPiece *p=&scene->pieces[i];
    if(p->placement.kind==old_kind&&p->placement.x==x&&p->placement.z==z){
      piece=p;break;
    }
  }
  if(!piece)return false;

  LDKAssetManager *assets=(LDKAssetManager *)
      ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  const NacreDungeonPlacement replacement={new_kind,x,z};
  const NacreDungeonSelectedPrefab selected=nacre_dungeon_system_resolve_prefab(
      system,map,&replacement,assets);
  const bool new_present=nacre_dungeon_system_prefab_live(assets,selected.asset);
  const bool old_present=!x_handle_is_null(piece->root);
  LDKEntity new_entity=x_handle_null();
  const u32 first_new_drawable=scene->drawable_count;
  if(new_present){
    if(x_handle_is_null(runtime->generated_root))return false;
    new_entity=ldk_ecs_prefab_instantiate(selected.asset);
    if(x_handle_is_null(new_entity))return false;
    const Vec3 local=vec3_make(x-(float)map->width*.5f,0.0f,
                               z-(float)map->height*.5f);
    if(!nacre_dungeon_system_instance_place(
           new_entity,runtime->generated_root,local,new_kind)||
       (selected.using_fallback &&
        !ldk_ecs_entity_name_set(new_entity,selected.missing_name))||
       !nacre_dungeon_visibility_collect(scene,new_entity)){
      nacre_dungeon_system_destroy_subtree(new_entity);
      scene->drawable_count=first_new_drawable;
      return false;
    }
  }
  if(piece->using_fallback)--runtime->fallback_instance_count;
  if(new_present&&selected.using_fallback)++runtime->fallback_instance_count;
  if(old_present)nacre_dungeon_system_destroy_subtree(piece->root);
  piece->root=new_entity;
  piece->using_fallback=new_present&&selected.using_fallback;
  piece->placement.kind=new_kind;
  piece->first_drawable=first_new_drawable;
  piece->drawable_count=scene->drawable_count-first_new_drawable;
  piece->visible=false;
  if(old_present)--runtime->generated_entity_count;
  if(new_present)++runtime->generated_entity_count;
  if(!old_present)--runtime->missing_prefab_pieces;
  if(!new_present)++runtime->missing_prefab_pieces;
  scene->applied=false;
  nacre_dungeon_system_apply_visibility(system);
  return true;
}

static NacreDungeonEdge *nacre_dungeon_system_edge_find(
    NacreDungeonMap *map,u32 from,u32 to,NacreDungeonEdgeType expected)
{
  if(!map||!map->cells)return NULL;
  for(u32 i=0;i<map->edge_count;i++){
    NacreDungeonEdge *e=&map->edges[i];
    if(((e->a==from&&e->b==to)||(e->b==from&&e->a==to))&&
       e->type==expected)return e;
  }
  return NULL;
}

static NacreDungeonPassage *nacre_dungeon_system_passage_find(
    NacreDungeonMap *map,u32 from,u32 to)
{
  if(!map||!map->passages)return NULL;
  for(u32 i=0;i<map->passage_count;i++){
    NacreDungeonPassage *p=&map->passages[i];
    if((p->a==from&&p->b==to)||(p->b==from&&p->a==to))return p;
  }
  return NULL;
}

static bool nacre_dungeon_system_barrier_open(
    u32 from,u32 to,NacreDungeonEdgeType expected_type)
{
  NacreDungeonMap *map=&nacre_dungeon_runtime.map;
  NacreDungeonEdge *edge=nacre_dungeon_system_edge_find(
      map,from,to,expected_type);
  if(!edge)return false;
  const NacreDungeonPartKind old_x=expected_type==NACRE_DUNGEON_EDGE_CRACKED?
      NACRE_DUNGEON_PART_CRACKED_X:NACRE_DUNGEON_PART_SECRET_X;
  const NacreDungeonPartKind old_z=expected_type==NACRE_DUNGEON_EDGE_CRACKED?
      NACRE_DUNGEON_PART_CRACKED_Z:NACRE_DUNGEON_PART_SECRET_Z;
  if(!nacre_dungeon_system_boundary_visual_replace(from,to,old_x,old_z,
      NACRE_DUNGEON_PART_HOLE_X,NACRE_DUNGEON_PART_HOLE_Z))return false;
  edge->type=NACRE_DUNGEON_EDGE_HOLE;
  nacre_dungeon_system_publish_map(nacre_dungeon_runtime.owner);
  return true;
}

bool nacre_dungeon_system_cracked_wall_break(u32 from,u32 to)
{
  return nacre_dungeon_system_barrier_open(from,to,NACRE_DUNGEON_EDGE_CRACKED);
}

bool nacre_dungeon_system_secret_open(u32 from,u32 to)
{
  return nacre_dungeon_system_barrier_open(from,to,NACRE_DUNGEON_EDGE_SECRET);
}

/* Architectural passage and barrier are separate placements at the
 * same grid edge. Opening only affects the BARRIER instance: never replace
 * either prefab, so any author-created animation keeps its own hierarchy.
 * If the clip is not authored yet, hide only that barrier's visual drawables
 * to avoid retaining a visibly closed door on an open navigation edge.
 * The room-visibility system remains the authority for their enabled state. */
static void nacre_dungeon_system_barrier_visual_open(u32 from, u32 to,
    NacreDungeonPartKind x_kind, NacreDungeonPartKind z_kind)
{
  NacreDungeonRuntime *runtime = &nacre_dungeon_runtime;
  NacreDungeonSceneVisibility *scene = &runtime->visibility;
  const NacreDungeonMap *map = &runtime->map;
  const u32 fx = from % map->width, tx = to % map->width;
  const u32 fz = from / map->width, tz = to / map->width;
  const bool along_x = fx == tx &&
      (fz > tz ? fz - tz : tz - fz) == 1u;
  const bool along_z = fz == tz &&
      (fx > tx ? fx - tx : tx - fx) == 1u;
  if (!along_x && !along_z) return;
  const NacreDungeonPartKind kind = along_x ? x_kind : z_kind;
  const float x = along_x ? (float)fx + .5f :
      (float)(fx > tx ? fx : tx);
  const float z = along_z ? (float)fz + .5f :
      (float)(fz > tz ? fz : tz);
  for (u32 i = 0u; i < scene->piece_count; ++i)
  {
    NacreDungeonVisualPiece *piece = &scene->pieces[i];
    if (piece->placement.kind != kind ||
        piece->placement.x != x || piece->placement.z != z) continue;
    LDKAssetManager *assets = (LDKAssetManager *)
        ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    if (!x_handle_is_null(piece->root) &&
        nacre_dungeon_system_open_animation(piece->root, assets, true))
      return;

    /* Missing animation: make the visual state agree with the now-open
     * logical edge. This is also safe with the generic prefab fallback. */
    for (u32 d = 0u; d < piece->drawable_count; ++d)
      scene->drawables[piece->first_drawable + d].authored_enabled = false;
    scene->applied = false;
    nacre_dungeon_system_apply_visibility(runtime->owner);
    ldk_log_warning("NacreDungeonSystem: %s has no playable open.anim; "
        "hiding barrier visuals.\n", nacre_dungeon_system_part_name(kind));
    return;
  }
}

bool nacre_dungeon_system_door_open(u32 from,u32 to)
{
  NacreDungeonMap *map=&nacre_dungeon_runtime.map;
  NacreDungeonPassage *door=nacre_dungeon_system_passage_find(map,from,to);
  if(!door||door->kind!=NACRE_DUNGEON_PASSAGE_DOOR||door->opened)return false;
  nacre_dungeon_system_barrier_visual_open(from,to,
      NACRE_DUNGEON_PART_DOOR_X,NACRE_DUNGEON_PART_DOOR_Z);
  door->opened=true;
  nacre_dungeon_system_publish_map(nacre_dungeon_runtime.owner);
  return true;
}

bool nacre_dungeon_system_portcullis_open(
    u32 from,u32 to,const NacreDungeonDirection sequence[4])
{
  if(!sequence)return false;
  NacreDungeonMap *map=&nacre_dungeon_runtime.map;
  NacreDungeonPassage *gate=nacre_dungeon_system_passage_find(map,from,to);
  if(!gate||gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS||gate->opened)
    return false;
  bool authorized=false;
  for(u32 t=0;t<map->totem_count;t++){
    const NacreDungeonTotem *totem=&map->totems[t];
    if(totem->color!=gate->color||totem->id!=gate->totem_id||
       !totem->discovered)continue;
    authorized=true;
    for(u32 j=0;j<4;j++){
      if(sequence[j]>NACRE_DUNGEON_DIRECTION_RIGHT||
         sequence[j]!=totem->sequence[j]){authorized=false;break;}
    }
    break;
  }
  if(!authorized)return false;
  nacre_dungeon_system_barrier_visual_open(from,to,
      NACRE_DUNGEON_PART_PORTCULLIS_X,NACRE_DUNGEON_PART_PORTCULLIS_Z);
  gate->opened=true;
  nacre_dungeon_system_publish_map(nacre_dungeon_runtime.owner);
  return true;
}

bool nacre_dungeon_system_totem_discover(u32 tile,NacreDungeonTotem *out)
{
  NacreDungeonMap *map=&nacre_dungeon_runtime.map;
  if(!nacre_dungeon_runtime.owner||!map->cells||
     tile>=map->width*map->height)return false;
  for(u32 t=0;t<map->totem_count;t++){
    NacreDungeonTotem *totem=&map->totems[t];
    if(totem->tile!=tile)continue;
    totem->discovered=true;
    if(out)*out=*totem;
    return true;
  }
  return false;
}

bool nacre_dungeon_system_objective_complete(u32 tile)
{
  NacreDungeonRuntime *runtime=&nacre_dungeon_runtime;
  NacreDungeonMap *map=&runtime->map;
  if(!runtime->owner||!map->cells||tile>=map->width*map->height)return false;
  for(u32 o=0;o<map->objective_count;o++){
    NacreDungeonObjective *objective=&map->objectives[o];
    if(objective->tile!=tile || objective->completed)continue;
    objective->completed=true;
    /* Prevent the renderer from re-enabling this marker when room visibility
     * changes. Keep the entity and its gameplay components untouched. */
    const NacreDungeonPartKind kind=map->exit_requirement==1?
        NACRE_DUNGEON_PART_OBJECTIVE_KEY:NACRE_DUNGEON_PART_OBJECTIVE_LEVER;
    NacreDungeonSceneVisibility *scene=&runtime->visibility;
    const float x=(float)(tile%map->width)+.5f;
    const float z=(float)(tile/map->width)+.5f;
    for(u32 p=0;p<scene->piece_count;p++){
      NacreDungeonVisualPiece *piece=&scene->pieces[p];
      if(piece->placement.kind!=kind||
         piece->placement.x!=x||piece->placement.z!=z)continue;
      for(u32 d=0;d<piece->drawable_count;d++)
        scene->drawables[piece->first_drawable+d].authored_enabled=false;
      scene->applied=false;
      break;
    }
    nacre_dungeon_system_apply_visibility(runtime->owner);
    nacre_dungeon_system_publish_map(runtime->owner);
    return true;
  }
  return false;
}

bool nacre_dungeon_system_exit_can_use(u32 player_tile)
{
  const NacreDungeonMap *map=&nacre_dungeon_runtime.map;
  return nacre_dungeon_runtime.owner&&map->cells&&player_tile==map->exit&&
      nacre_dungeon_progression_can_exit(map);
}
