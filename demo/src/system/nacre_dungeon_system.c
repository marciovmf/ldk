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
 * - Missing/unassigned prefab slots cause only the corresponding pieces to
 *   be skipped. A structural instantiate/transform error rolls back the new
 *   subtree and keeps the previous dungeon intact.
 *
 * Coordinates:
 * - The generator grid spans [0,width] x [0,height] in XZ, Y up.
 * - Local prefab pivots are at the center of the base and already oriented
 *   along X or Z. No per-piece scale or rotation is required.
 * - The whole dungeon is centered on system->world_origin.
 *
 * Stage 1: floors, straight walls, open passages, holes and cracked walls.
 * No actual doors/portcullises, pillars, totems, caves or secrets exist yet.
 */
#include "nacre_dungeon_system.h"

#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <string.h>

typedef struct NacreDungeonRuntime
{
  NacreDungeonSystem *owner;
  NacreDungeonMap map;
  Vec3 generated_origin;
  LDKEntity generated_root;
  u32 generated_entity_count;
  u32 missing_prefab_pieces;
  u32 planned_piece_count;
} NacreDungeonRuntime;

/* One scene-owned instance, matching the current LDK single active scene. */
static NacreDungeonRuntime nacre_dungeon_runtime;

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
    &system->floor_prefab,
    &system->wall_x_1m_prefab, &system->wall_z_1m_prefab,
    &system->wall_x_2m_prefab, &system->wall_z_2m_prefab,
    &system->passage_x_prefab, &system->passage_z_prefab,
    &system->hole_x_prefab, &system->hole_z_prefab,
    &system->cracked_x_prefab, &system->cracked_z_prefab,
    &system->door_x_prefab, &system->door_z_prefab,
    &system->portcullis_x_prefab, &system->portcullis_z_prefab,
    &system->pillar_intact_prefab, &system->pillar_broken_prefab
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
  if (new_instance)
  {
    system->extra_connections = (float)defaults.extra_connections;
    system->irregular_rooms = (float)defaults.irregular_rooms;
    system->adjacent_rooms = (float)defaults.adjacent_rooms;
    system->alternative_walls = (float)defaults.alternative_walls;
    system->cracked_fraction = (float)defaults.cracked_fraction;
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
  return true;
}

static LDKAssetPrefab nacre_dungeon_system_part_prefab(
    const NacreDungeonSystem *system, NacreDungeonPartKind kind)
{
  switch (kind)
  {
    case NACRE_DUNGEON_PART_FLOOR: return system->floor_prefab;
    case NACRE_DUNGEON_PART_WALL_X_1M: return system->wall_x_1m_prefab;
    case NACRE_DUNGEON_PART_WALL_Z_1M: return system->wall_z_1m_prefab;
    case NACRE_DUNGEON_PART_WALL_X_2M: return system->wall_x_2m_prefab;
    case NACRE_DUNGEON_PART_WALL_Z_2M: return system->wall_z_2m_prefab;
    case NACRE_DUNGEON_PART_PASSAGE_X: return system->passage_x_prefab;
    case NACRE_DUNGEON_PART_PASSAGE_Z: return system->passage_z_prefab;
    case NACRE_DUNGEON_PART_HOLE_X: return system->hole_x_prefab;
    case NACRE_DUNGEON_PART_HOLE_Z: return system->hole_z_prefab;
    case NACRE_DUNGEON_PART_CRACKED_X: return system->cracked_x_prefab;
    case NACRE_DUNGEON_PART_CRACKED_Z: return system->cracked_z_prefab;
    default: return ldk_asset_prefab_null();
  }
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
    LDKEntity *out_root, u32 *out_created, u32 *out_missing)
{
  LDKAssetManager *assets = (LDKAssetManager *)
      ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKEntity root = x_handle_null();
  u32 created = 0u, missing = 0u;
  bool any_live = false;

  /* Missing slots are legal. They let the user start with floors and walls
   * only, then add special passage prefabs later in the Comet inspector. */
  for (u32 i = 0; i < placements->count; ++i)
  {
    const LDKAssetPrefab prefab =
        nacre_dungeon_system_part_prefab(system, placements->items[i].kind);
    if (nacre_dungeon_system_prefab_live(assets, prefab))
    {
      any_live = true;
    }
    else
    {
      ++missing;
    }
  }

  if (!any_live)
  {
    *out_root = root;
    *out_created = 0u;
    *out_missing = missing;
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

  for (u32 i = 0; i < placements->count; ++i)
  {
    const NacreDungeonPlacement *piece = &placements->items[i];
    const LDKAssetPrefab prefab =
        nacre_dungeon_system_part_prefab(system, piece->kind);
    if (!nacre_dungeon_system_prefab_live(assets, prefab))
    {
      continue;
    }

    LDKEntity instance = ldk_ecs_prefab_instantiate(prefab);
    if (x_handle_is_null(instance))
    {
      nacre_dungeon_system_destroy_subtree(root);
      return false;
    }
    const Vec3 local = vec3_make(
        piece->x - (float)map->width * .5f,
        0.0f,
        piece->z - (float)map->height * .5f);
    if (!ldk_transform_set_parent(instance, root) ||
        !ldk_transform_set_local_position(instance, local))
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
    ++created;
  }

  *out_root = root;
  *out_created = created;
  *out_missing = missing;
  return true;
}

static void nacre_dungeon_system_publish_map(NacreDungeonSystem *system)
{
  const NacreDungeonMap *map = &nacre_dungeon_runtime.map;
  system->layout_generated = map->cells != NULL;
  system->layout_valid = system->layout_generated &&
      nacre_dungeon_generator_validate(map);
  system->generated_entity_count = nacre_dungeon_runtime.generated_entity_count;
  system->missing_prefab_pieces = nacre_dungeon_runtime.missing_prefab_pieces;
  system->planned_piece_count = nacre_dungeon_runtime.planned_piece_count;
  if (!system->layout_generated)
  {
    system->room_count = 0u;
    system->link_count = 0u;
    system->edge_count = 0u;
    system->normalized_micro_corridors = 0u;
    system->shortest_path_length = 0u;
    system->entry_tile = 0u;
    system->exit_tile = 0u;
    return;
  }
  system->room_count = map->room_count;
  system->link_count = map->link_count;
  system->edge_count = map->edge_count;
  system->normalized_micro_corridors = map->normalized_micro_corridors;
  system->shortest_path_length = map->shortest_path_length;
  system->entry_tile = map->entry;
  system->exit_tile = map->exit;
}

static void nacre_dungeon_system_generate(NacreDungeonSystem *system)
{
  NacreDungeonConfig config;
  NacreDungeonMap next = {0};
  NacreDungeonPlacementList placements = {0};
  NacreDungeonResult result;
  LDKEntity next_root = x_handle_null();
  u32 created = 0u, missing = 0u;

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
        "loops=%g irregular=%g adjacent=%g alternative=%g cracked=%g.\n",
        config_error, config.seed, config.width, config.height,
        config.rooms_min, config.rooms_max, config.max_room_size,
        config.max_corridor_tiles, config.extra_connections,
        config.irregular_rooms, config.adjacent_rooms,
        config.alternative_walls, config.cracked_fraction);
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
  const bool combine_x = nacre_dungeon_system_prefab_live(
      assets, system->wall_x_2m_prefab);
  const bool combine_z = nacre_dungeon_system_prefab_live(
      assets, system->wall_z_2m_prefab);
  if (!nacre_dungeon_placements_build(
      &next, combine_x, combine_z, &placements))
  {
    nacre_dungeon_generator_release(&next);
    system->last_result = NACRE_DUNGEON_OUT_OF_MEMORY;
    nacre_dungeon_system_set_status(system, "Failed to construct placements");
    return;
  }
  if (!nacre_dungeon_system_instantiate(
      system, &next, &placements, &next_root, &created, &missing))
  {
    nacre_dungeon_placements_release(&placements);
    nacre_dungeon_generator_release(&next);
    nacre_dungeon_system_set_status(system, "Prefab instantiation failed");
    ldk_log_error("NacreDungeonSystem: failed to instantiate generated prefabs.\n");
    return;
  }

  /* Commit: delete the old visual subtree only after the new one exists. */
  nacre_dungeon_system_destroy_subtree(nacre_dungeon_runtime.generated_root);
  nacre_dungeon_generator_release(&nacre_dungeon_runtime.map);
  nacre_dungeon_runtime.map = next;
  /* Commit the same origin used when creating prefab transforms. Do not
   * follow uncommitted editor changes until the scene is regenerated. */
  nacre_dungeon_runtime.generated_origin = system->world_origin;
  nacre_dungeon_runtime.generated_root = next_root;
  nacre_dungeon_runtime.generated_entity_count = created;
  nacre_dungeon_runtime.missing_prefab_pieces = missing;
  nacre_dungeon_runtime.planned_piece_count = placements.count;
  nacre_dungeon_placements_release(&placements);

  ++system->generation_revision;
  nacre_dungeon_system_publish_map(system);
  nacre_dungeon_system_set_status(system, created == 0u ?
      "Assign dungeon prefabs, then Regenerate" :
      missing > 0u ? "Generated with missing prefab slots" :
      "Dungeon instantiated");

  ldk_log_info("NacreDungeonSystem: seed=%s grid=%ux%u rooms=%u "
      "pieces=%u instances=%u missing=%u.\n",
      config.seed, config.width, config.height,
      system->room_count, system->planned_piece_count,
      created, missing);
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
  if (!system || nacre_dungeon_runtime.owner != system || !system->regenerate)
  {
    return;
  }
  system->regenerate = false;
  nacre_dungeon_system_generate(system);
}

void nacre_dungeon_system_terminate(void *data)
{
  NacreDungeonSystem *system = (NacreDungeonSystem *)data;
  if (!system || nacre_dungeon_runtime.owner != system)
  {
    return;
  }
  nacre_dungeon_system_destroy_subtree(nacre_dungeon_runtime.generated_root);
  nacre_dungeon_runtime.generated_root = x_handle_null();
  nacre_dungeon_runtime.generated_entity_count = 0u;
  nacre_dungeon_runtime.missing_prefab_pieces = 0u;
  nacre_dungeon_runtime.planned_piece_count = 0u;
  nacre_dungeon_generator_release(&nacre_dungeon_runtime.map);
  nacre_dungeon_runtime.owner = NULL;
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
