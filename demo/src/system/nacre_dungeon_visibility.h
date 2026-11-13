#ifndef NACRE_DUNGEON_VISIBILITY_H
#define NACRE_DUNGEON_VISIBILITY_H

#include "nacre_dungeon_generator.h"
#include "nacre_dungeon_placement.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Logical visibility regions, independent of the renderer and the LDK.
 *
 * Every authored room owns one region. Contiguous non-room floor tiles form
 * separate corridor regions (cardinal adjacency only). Region 0 is void.
 * This is VISIBILITY grouping, not a collision/navigation graph: corridor
 * cells touching a room are still a distinct region even across an arch.
 *
 * Boundary pieces are owned by every region on either side. In particular,
 * a 2 m wall may touch up to four distinct regions. Never cull a shared wall
 * just because it belongs to another room too.
 */
typedef struct NacreDungeonVisibilityMap
{
  uint16_t *region_by_tile;   /* width * height, indexed like map->cells */
  uint32_t tile_count;
  uint16_t region_count;      /* Excludes region 0. */
} NacreDungeonVisibilityMap;

bool nacre_dungeon_visibility_build(
    const NacreDungeonMap *map, NacreDungeonVisibilityMap *out);
void nacre_dungeon_visibility_release(NacreDungeonVisibilityMap *visibility);

/* Return up to four UNIQUE nonzero region IDs touching the placement. */
uint32_t nacre_dungeon_visibility_piece_regions(
    const NacreDungeonMap *map, const NacreDungeonVisibilityMap *visibility,
    const NacreDungeonPlacement *piece, uint16_t out_regions[4]);

#endif /* NACRE_DUNGEON_VISIBILITY_H */
