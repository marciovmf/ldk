#include "nacre_dungeon_placement.h"

#include <stdlib.h>
#include <string.h>

/* Per-grid-edge representation. All edges (including the outside boundary)
 * are evaluated exactly once, so no duplicate wall entity can be generated. */
typedef enum NacreDungeonBoundary
{
  NACRE_DUNGEON_BOUNDARY_NONE = 0,
  NACRE_DUNGEON_BOUNDARY_WALL,
  NACRE_DUNGEON_BOUNDARY_PASSAGE,
  NACRE_DUNGEON_BOUNDARY_HOLE,
  NACRE_DUNGEON_BOUNDARY_CRACKED
} NacreDungeonBoundary;

static NacreDungeonBoundary nacre_dungeon_boundary_between(
    const NacreDungeonMap *map, uint32_t a, uint32_t b)
{
  if (!map->cells[a] || !map->cells[b])
  {
    return NACRE_DUNGEON_BOUNDARY_WALL;
  }

  /* Same room, or two corridor cells: no internal wall. */
  if (map->room_ids[a] == map->room_ids[b])
  {
    return NACRE_DUNGEON_BOUNDARY_NONE;
  }

  /* A connection between different regions must be explicitly authorized.
   * The ordinary OPEN edge means an arch/pass-through, NOT an empty wall.
   * Special connections replace the entire wall with their own prefab. */
  for (uint32_t i = 0; i < map->edge_count; ++i)
  {
    const NacreDungeonEdge *edge = &map->edges[i];
    if ((edge->a == a && edge->b == b) ||
        (edge->a == b && edge->b == a))
    {
      switch (edge->type)
      {
        case NACRE_DUNGEON_EDGE_OPEN:
          return NACRE_DUNGEON_BOUNDARY_PASSAGE;
        case NACRE_DUNGEON_EDGE_HOLE:
          return NACRE_DUNGEON_BOUNDARY_HOLE;
        case NACRE_DUNGEON_EDGE_CRACKED:
          return NACRE_DUNGEON_BOUNDARY_CRACKED;
        default:
          return NACRE_DUNGEON_BOUNDARY_WALL;
      }
    }
  }
  return NACRE_DUNGEON_BOUNDARY_WALL;
}

static bool nacre_dungeon_placement_add(NacreDungeonPlacementList *list,
    NacreDungeonPartKind kind, float x, float z)
{
  if (list->count == list->capacity)
  {
    uint32_t next = list->capacity ? list->capacity * 2u : 256u;
    if (next <= list->capacity ||
        (size_t)next > SIZE_MAX / sizeof(*list->items))
    {
      return false;
    }
    void *items = realloc(list->items, (size_t)next * sizeof(*list->items));
    if (!items)
    {
      return false;
    }
    list->items = items;
    list->capacity = next;
  }
  list->items[list->count++] = (NacreDungeonPlacement){kind, x, z};
  return true;
}

static NacreDungeonPartKind nacre_dungeon_boundary_kind(
    NacreDungeonBoundary boundary, bool along_x)
{
  switch (boundary)
  {
    case NACRE_DUNGEON_BOUNDARY_PASSAGE:
      return along_x ? NACRE_DUNGEON_PART_PASSAGE_X : NACRE_DUNGEON_PART_PASSAGE_Z;
    case NACRE_DUNGEON_BOUNDARY_HOLE:
      return along_x ? NACRE_DUNGEON_PART_HOLE_X : NACRE_DUNGEON_PART_HOLE_Z;
    case NACRE_DUNGEON_BOUNDARY_CRACKED:
      return along_x ? NACRE_DUNGEON_PART_CRACKED_X : NACRE_DUNGEON_PART_CRACKED_Z;
    default:
      return along_x ? NACRE_DUNGEON_PART_WALL_X_1M : NACRE_DUNGEON_PART_WALL_Z_1M;
  }
}

bool nacre_dungeon_placements_build(const NacreDungeonMap *map,
    bool use_wall_x_2m, bool use_wall_z_2m,
    NacreDungeonPlacementList *out)
{
  if (!map || !out || !map->cells || !map->room_ids ||
      !map->width || !map->height ||
      map->width > NACRE_DUNGEON_MAX_DIMENSION ||
      map->height > NACRE_DUNGEON_MAX_DIMENSION)
  {
    return false;
  }

  /* Keep out empty on failure; the caller can safely leave the previous
   * generated scene untouched until this list has been built successfully. */
  NacreDungeonPlacementList result = {0};
  const uint32_t w = map->width, h = map->height;
  const size_t horizontal_count = (size_t)w * (h + 1u);
  const size_t vertical_count = (size_t)(w + 1u) * h;
  uint8_t *horizontal = calloc(horizontal_count, sizeof(uint8_t));
  uint8_t *vertical = calloc(vertical_count, sizeof(uint8_t));
  if (!horizontal || !vertical)
  {
    free(horizontal);
    free(vertical);
    return false;
  }

  for (uint32_t z = 0; z < h; ++z)
  {
    for (uint32_t x = 0; x < w; ++x)
    {
      const uint32_t a = z * w + x;
      if (!map->cells[a])
      {
        continue;
      }
      if (!nacre_dungeon_placement_add(&result,
          NACRE_DUNGEON_PART_FLOOR, (float)x + .5f, (float)z + .5f))
      {
        goto failure;
      }

      /* Left/top boundaries are processed only for outside/empty neighbors.
       * Occupied/occupied boundaries were already evaluated from the other
       * cell's right/bottom edge. */
      if (x == 0u || !map->cells[a - 1u])
      {
        vertical[(size_t)z * (w + 1u) + x] = NACRE_DUNGEON_BOUNDARY_WALL;
      }
      if (z == 0u || !map->cells[a - w])
      {
        horizontal[(size_t)z * w + x] = NACRE_DUNGEON_BOUNDARY_WALL;
      }

      vertical[(size_t)z * (w + 1u) + (x + 1u)] =
          x + 1u == w ? NACRE_DUNGEON_BOUNDARY_WALL :
          nacre_dungeon_boundary_between(map, a, a + 1u);
      horizontal[(size_t)(z + 1u) * w + x] =
          z + 1u == h ? NACRE_DUNGEON_BOUNDARY_WALL :
          nacre_dungeon_boundary_between(map, a, a + w);
    }
  }

  /* A wall is a grid boundary; the prefab pivot is at its bottom center.
   * A merged 2 m segment is centered between the two original 1 m edges. */
  for (uint32_t z = 0; z <= h; ++z)
  {
    for (uint32_t x = 0; x < w; ++x)
    {
      const NacreDungeonBoundary boundary =
          (NacreDungeonBoundary)horizontal[(size_t)z * w + x];
      if (boundary == NACRE_DUNGEON_BOUNDARY_NONE)
      {
        continue;
      }
      if (boundary == NACRE_DUNGEON_BOUNDARY_WALL && use_wall_x_2m &&
          x + 1u < w &&
          horizontal[(size_t)z * w + (x + 1u)] == NACRE_DUNGEON_BOUNDARY_WALL)
      {
        if (!nacre_dungeon_placement_add(&result,
            NACRE_DUNGEON_PART_WALL_X_2M, (float)x + 1.0f, (float)z))
        {
          goto failure;
        }
        ++x;
        continue;
      }
      if (!nacre_dungeon_placement_add(&result,
          nacre_dungeon_boundary_kind(boundary, true),
          (float)x + .5f, (float)z))
      {
        goto failure;
      }
    }
  }

  /* Vertical segments must be scanned column-first, so a 2 m segment
   * consumes only the next row in the SAME column. */
  for (uint32_t x = 0; x <= w; ++x)
  {
    for (uint32_t z = 0; z < h; ++z)
    {
      const NacreDungeonBoundary boundary =
          (NacreDungeonBoundary)vertical[(size_t)z * (w + 1u) + x];
      if (boundary == NACRE_DUNGEON_BOUNDARY_NONE)
      {
        continue;
      }
      if (boundary == NACRE_DUNGEON_BOUNDARY_WALL && use_wall_z_2m &&
          z + 1u < h &&
          vertical[(size_t)(z + 1u) * (w + 1u) + x] == NACRE_DUNGEON_BOUNDARY_WALL)
      {
        if (!nacre_dungeon_placement_add(&result,
            NACRE_DUNGEON_PART_WALL_Z_2M, (float)x, (float)z + 1.0f))
        {
          goto failure;
        }
        ++z;
        continue;
      }
      if (!nacre_dungeon_placement_add(&result,
          nacre_dungeon_boundary_kind(boundary, false),
          (float)x, (float)z + .5f))
      {
        goto failure;
      }
    }
  }

  free(horizontal);
  free(vertical);
  *out = result;
  return true;

failure:
  free(horizontal);
  free(vertical);
  nacre_dungeon_placements_release(&result);
  return false;
}

void nacre_dungeon_placements_release(NacreDungeonPlacementList *placements)
{
  if (!placements)
  {
    return;
  }
  free(placements->items);
  memset(placements, 0, sizeof(*placements));
}
