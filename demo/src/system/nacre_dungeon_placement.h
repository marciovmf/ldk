#ifndef NACRE_DUNGEON_PLACEMENT_H
#define NACRE_DUNGEON_PLACEMENT_H

#include "nacre_dungeon_generator.h"

#include <stdbool.h>
#include <stdint.h>

struct NacreDungeonVisibilityMap;

/*
 * Deterministic conversion from NacreDungeonMap topology to structural pieces.
 *
 * No LDK dependency. The resulting placements describe LOCAL positions in
 * logical tile units. (0,0) is the grid's top-left corner; (x+.5,z+.5) is
 * the center of cell (x,z). The scene system offsets these positions by
 * (-width/2, -height/2) so the map is centered around its configured origin.
 * Every referenced prefab MUST have its pivot at the center of its base.
 *
 * X-aligned walls span X and stand in an XY plane. Z-aligned walls span Z
 * and stand in a ZY plane. All authored directional prefabs face X; the
 * scene system rotates Z placements +90 degrees about world Y. The placement
 * layer remains engine-independent and never rotates or scales parts.
 */
typedef enum NacreDungeonPartKind
{
  NACRE_DUNGEON_PART_FLOOR = 0,
  NACRE_DUNGEON_PART_WALL_X_1M,
  NACRE_DUNGEON_PART_WALL_Z_1M,
  NACRE_DUNGEON_PART_WALL_X_2M,
  NACRE_DUNGEON_PART_WALL_Z_2M,
  NACRE_DUNGEON_PART_PASSAGE_X,
  NACRE_DUNGEON_PART_PASSAGE_Z,
  NACRE_DUNGEON_PART_HOLE_X,
  NACRE_DUNGEON_PART_HOLE_Z,
  NACRE_DUNGEON_PART_CRACKED_X,
  NACRE_DUNGEON_PART_CRACKED_Z,
  NACRE_DUNGEON_PART_SECRET_X, /* Visually identical to ordinary 1 m wall. */
  NACRE_DUNGEON_PART_SECRET_Z,
  NACRE_DUNGEON_PART_PILLAR_INTACT,
  NACRE_DUNGEON_PART_PILLAR_BROKEN,
  NACRE_DUNGEON_PART_CAVE_FLOOR,
  NACRE_DUNGEON_PART_CAVE_WALL_X,
  NACRE_DUNGEON_PART_CAVE_WALL_Z,
  NACRE_DUNGEON_PART_DOOR_X,
  NACRE_DUNGEON_PART_DOOR_Z,
  NACRE_DUNGEON_PART_PORTCULLIS_X,
  NACRE_DUNGEON_PART_PORTCULLIS_Z,
  NACRE_DUNGEON_PART_TOTEM,
  NACRE_DUNGEON_PART_ENTRY,
  NACRE_DUNGEON_PART_EXIT,
  NACRE_DUNGEON_PART_OBJECTIVE_KEY,
  NACRE_DUNGEON_PART_OBJECTIVE_LEVER,
  NACRE_DUNGEON_PART_KIND_COUNT
} NacreDungeonPartKind;

typedef struct NacreDungeonPlacement
{
  NacreDungeonPartKind kind;
  float x;
  float z;
} NacreDungeonPlacement;

typedef struct NacreDungeonPlacementList
{
  NacreDungeonPlacement *items;
  uint32_t count;
  uint32_t capacity;
} NacreDungeonPlacementList;

/* The visibility map must be built BEFORE placements. Two consecutive 1 m
 * walls may share a 2 m prefab only when their visibility-region sets are
 * identical. This prevents a long prefab from extending beyond the current
 * room when room culling is enabled. Different ownership -> two 1 m pieces.
 *
 * Passages, holes and cracked walls are never merged. Each DOOR or
 * PORTCULLIS edge produces exactly TWO placements: Passage X/Z plus the
 * appropriate barrier X/Z at the same pivot. Both have independent visibility
 * and prefab fallback. The underlying logical grid edge remains unique.
 */
bool nacre_dungeon_placements_build(const NacreDungeonMap *map,
    const struct NacreDungeonVisibilityMap *visibility,
    bool use_wall_2m,
    NacreDungeonPlacementList *out);
void nacre_dungeon_placements_release(NacreDungeonPlacementList *placements);

#endif /* NACRE_DUNGEON_PLACEMENT_H */
