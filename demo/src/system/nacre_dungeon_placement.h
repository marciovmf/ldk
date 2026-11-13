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
 * and stand in a ZY plane. Prefabs already provide the mesh orientation and
 * dimensions; the placement layer never rotates or resizes individual parts.
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
 * Passages, holes and cracked walls are never merged. Prefab placement never
 * changes the generated topology, and every physical grid edge remains once.
 */
bool nacre_dungeon_placements_build(const NacreDungeonMap *map,
    const struct NacreDungeonVisibilityMap *visibility,
    bool use_wall_x_2m, bool use_wall_z_2m,
    NacreDungeonPlacementList *out);
void nacre_dungeon_placements_release(NacreDungeonPlacementList *placements);

#endif /* NACRE_DUNGEON_PLACEMENT_H */
