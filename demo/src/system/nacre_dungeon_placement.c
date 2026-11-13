#include "nacre_dungeon_placement.h"
#include "nacre_dungeon_visibility.h"

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
  NACRE_DUNGEON_BOUNDARY_CRACKED,
  NACRE_DUNGEON_BOUNDARY_SECRET,
  NACRE_DUNGEON_BOUNDARY_CAVE_WALL,
  NACRE_DUNGEON_BOUNDARY_DOOR,
  NACRE_DUNGEON_BOUNDARY_PORTCULLIS
} NacreDungeonBoundary;

static NacreDungeonBoundary nacre_dungeon_boundary_between(
    const NacreDungeonMap *map, uint32_t a, uint32_t b)
{
  if (!map->cells[a] || !map->cells[b])
  {
    return (map->cells[a] == NACRE_DUNGEON_CAVE ||
            map->cells[b] == NACRE_DUNGEON_CAVE) ?
        NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL;
  }

  /* Cave cells and ordinary corridor cells can both carry room ID -1.
   * Check their authored edge BEFORE the equal-room shortcut: cave entrance
   * barriers must be rendered, never silently treated as open floor. */
  const bool cave_transition =
      (map->cells[a] == NACRE_DUNGEON_CAVE) !=
      (map->cells[b] == NACRE_DUNGEON_CAVE);
  if (!cave_transition && map->room_ids[a] == map->room_ids[b])
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
      if (edge->type == NACRE_DUNGEON_EDGE_OPEN)
      {
        for (uint32_t p = 0; p < map->passage_count; ++p)
        {
          const NacreDungeonPassage *passage = &map->passages[p];
          if (passage->a == edge->a && passage->b == edge->b)
          {
            if (passage->kind == NACRE_DUNGEON_PASSAGE_DOOR)
              return NACRE_DUNGEON_BOUNDARY_DOOR;
            if (passage->kind == NACRE_DUNGEON_PASSAGE_PORTCULLIS)
              return NACRE_DUNGEON_BOUNDARY_PORTCULLIS;
            break;
          }
        }
      }
      switch (edge->type)
      {
        case NACRE_DUNGEON_EDGE_OPEN:
          return NACRE_DUNGEON_BOUNDARY_PASSAGE;
        case NACRE_DUNGEON_EDGE_HOLE:
          return NACRE_DUNGEON_BOUNDARY_HOLE;
        case NACRE_DUNGEON_EDGE_CRACKED:
          return NACRE_DUNGEON_BOUNDARY_CRACKED;
        case NACRE_DUNGEON_EDGE_SECRET:
          return NACRE_DUNGEON_BOUNDARY_SECRET;
        default:
          return NACRE_DUNGEON_BOUNDARY_WALL;
      }
    }
  }
  return (map->cells[a] == NACRE_DUNGEON_CAVE ||
          map->cells[b] == NACRE_DUNGEON_CAVE) ?
      NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL;
}

static bool nacre_dungeon_placement_add(NacreDungeonPlacementList *list,
    NacreDungeonPartKind kind, float x, float z)
{
  if (list->count == list->capacity)
  {
    uint32_t next = list->capacity ? list->capacity * 2u : 256u;
    if (next <= list->capacity ||
        SIZE_MAX / (size_t)next < sizeof(*list->items))
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
    case NACRE_DUNGEON_BOUNDARY_SECRET:
      return along_x ? NACRE_DUNGEON_PART_SECRET_X : NACRE_DUNGEON_PART_SECRET_Z;
    case NACRE_DUNGEON_BOUNDARY_DOOR:
      return along_x ? NACRE_DUNGEON_PART_DOOR_X : NACRE_DUNGEON_PART_DOOR_Z;
    case NACRE_DUNGEON_BOUNDARY_PORTCULLIS:
      return along_x ? NACRE_DUNGEON_PART_PORTCULLIS_X : NACRE_DUNGEON_PART_PORTCULLIS_Z;
    case NACRE_DUNGEON_BOUNDARY_CAVE_WALL:
      return along_x ? NACRE_DUNGEON_PART_CAVE_WALL_X : NACRE_DUNGEON_PART_CAVE_WALL_Z;
    default:
      return along_x ? NACRE_DUNGEON_PART_WALL_X_1M : NACRE_DUNGEON_PART_WALL_Z_1M;
  }
}

/* Compare the exact regions that would own each separate 1 m wall.
 * The visibility module is the sole authority for ownership; do not infer
 * room IDs here (corridors can have their own visibility regions). Region
 * lists are unique but not part of an ordered API, so compare as sets.
 *
 * This is evaluated only when both adjacent edges are ordinary walls.
 * Same orientation/type alone is NOT sufficient for a safe 2 m merge. */
static bool nacre_dungeon_walls_share_visibility(
    const NacreDungeonMap *map,
    const NacreDungeonVisibilityMap *visibility,
    bool along_x, uint32_t x, uint32_t z)
{
  const NacreDungeonPartKind kind = along_x ?
      NACRE_DUNGEON_PART_WALL_X_1M : NACRE_DUNGEON_PART_WALL_Z_1M;
  const NacreDungeonPlacement first = {
    kind, along_x ? (float)x + .5f : (float)x,
    along_x ? (float)z : (float)z + .5f
  };
  const NacreDungeonPlacement second = {
    kind, along_x ? (float)x + 1.5f : (float)x,
    along_x ? (float)z : (float)z + 1.5f
  };
  uint16_t first_regions[4] = {0};
  uint16_t second_regions[4] = {0};
  const uint32_t first_count = nacre_dungeon_visibility_piece_regions(
      map, visibility, &first, first_regions);
  const uint32_t second_count = nacre_dungeon_visibility_piece_regions(
      map, visibility, &second, second_regions);

  if (!first_count || first_count != second_count) return false;
  for (uint32_t i = 0; i < first_count; ++i)
  {
    bool found = false;
    for (uint32_t j = 0; j < second_count; ++j)
    {
      if (first_regions[i] == second_regions[j])
      {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

bool nacre_dungeon_placements_build(const NacreDungeonMap *map,
    const NacreDungeonVisibilityMap *visibility,
    bool use_wall_2m,
    NacreDungeonPlacementList *out)
{
  if (!map || !visibility || !visibility->region_by_tile ||
      !out || !map->cells || !map->room_ids ||
      !map->width || !map->height ||
      map->width > NACRE_DUNGEON_MAX_DIMENSION ||
      map->height > NACRE_DUNGEON_MAX_DIMENSION ||
      visibility->tile_count != map->width * map->height)
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
          map->cells[a] == NACRE_DUNGEON_CAVE ?
              NACRE_DUNGEON_PART_CAVE_FLOOR : NACRE_DUNGEON_PART_FLOOR,
          (float)x + .5f, (float)z + .5f))
      {
        goto failure;
      }

      /* Left/top boundaries are processed only for outside/empty neighbors.
       * Occupied/occupied boundaries were already evaluated from the other
       * cell's right/bottom edge. */
      if (x == 0u || !map->cells[a - 1u])
      {
        vertical[(size_t)z * (w + 1u) + x] =
            map->cells[a] == NACRE_DUNGEON_CAVE ?
                NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL;
      }
      if (z == 0u || !map->cells[a - w])
      {
        horizontal[(size_t)z * w + x] =
            map->cells[a] == NACRE_DUNGEON_CAVE ?
                NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL;
      }

      vertical[(size_t)z * (w + 1u) + (x + 1u)] =
          x + 1u == w ?
              (map->cells[a] == NACRE_DUNGEON_CAVE ?
                  NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL) :
          nacre_dungeon_boundary_between(map, a, a + 1u);
      horizontal[(size_t)(z + 1u) * w + x] =
          z + 1u == h ?
              (map->cells[a] == NACRE_DUNGEON_CAVE ?
                  NACRE_DUNGEON_BOUNDARY_CAVE_WALL : NACRE_DUNGEON_BOUNDARY_WALL) :
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
      if (boundary == NACRE_DUNGEON_BOUNDARY_WALL && use_wall_2m &&
          x + 1u < w &&
          horizontal[(size_t)z * w + (x + 1u)] == NACRE_DUNGEON_BOUNDARY_WALL &&
          nacre_dungeon_walls_share_visibility(map, visibility, true, x, z))
      {
        if (!nacre_dungeon_placement_add(&result,
            NACRE_DUNGEON_PART_WALL_X_2M, (float)x + 1.0f, (float)z))
        {
          goto failure;
        }
        ++x;
        continue;
      }
      /* A door/portcullis is a blocking object placed IN an architectural
       * passage, not a replacement for that passage. Both are independent
       * prefabs, with identical orientation and bottom-center pivot. */
      if ((boundary == NACRE_DUNGEON_BOUNDARY_DOOR ||
           boundary == NACRE_DUNGEON_BOUNDARY_PORTCULLIS) &&
          !nacre_dungeon_placement_add(&result,
              NACRE_DUNGEON_PART_PASSAGE_X, (float)x + .5f, (float)z))
      {
        goto failure;
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
      if (boundary == NACRE_DUNGEON_BOUNDARY_WALL && use_wall_2m &&
          z + 1u < h &&
          vertical[(size_t)(z + 1u) * (w + 1u) + x] == NACRE_DUNGEON_BOUNDARY_WALL &&
          nacre_dungeon_walls_share_visibility(map, visibility, false, x, z))
      {
        if (!nacre_dungeon_placement_add(&result,
            NACRE_DUNGEON_PART_WALL_Z_2M, (float)x, (float)z + 1.0f))
        {
          goto failure;
        }
        ++z;
        continue;
      }
      if ((boundary == NACRE_DUNGEON_BOUNDARY_DOOR ||
           boundary == NACRE_DUNGEON_BOUNDARY_PORTCULLIS) &&
          !nacre_dungeon_placement_add(&result,
              NACRE_DUNGEON_PART_PASSAGE_Z, (float)x, (float)z + .5f))
      {
        goto failure;
      }
      if (!nacre_dungeon_placement_add(&result,
          nacre_dungeon_boundary_kind(boundary, false),
          (float)x, (float)z + .5f))
      {
        goto failure;
      }
    }
  }

  /* Pillar prefabs share the same centered-at-base pivot convention as
   * floor tiles. Their cells stay walkable in the map but are forbidden by
   * the navigation mask, so placement and collision cannot diverge. */
  for (uint32_t i = 0; i < map->pillar_count; ++i)
  {
    const NacreDungeonPillar *pillar = &map->pillars[i];
    if (!nacre_dungeon_placement_add(&result,
        pillar->broken ? NACRE_DUNGEON_PART_PILLAR_BROKEN :
                         NACRE_DUNGEON_PART_PILLAR_INTACT,
        (float)(pillar->tile % w) + .5f,
        (float)(pillar->tile / w) + .5f)) goto failure;
  }

  /* Start/end markers are optional prefabs, but always part of the logical
   * placement list. They are centered on their respective tile bases. */
  if(!nacre_dungeon_placement_add(&result,NACRE_DUNGEON_PART_ENTRY,
      (float)(map->entry%w)+.5f,(float)(map->entry/w)+.5f) ||
     !nacre_dungeon_placement_add(&result,NACRE_DUNGEON_PART_EXIT,
      (float)(map->exit%w)+.5f,(float)(map->exit/w)+.5f))goto failure;
  for(uint32_t t=0;t<map->totem_count;t++){
    const uint32_t tile=map->totems[t].tile;
    if(!nacre_dungeon_placement_add(&result,NACRE_DUNGEON_PART_TOTEM,
        (float)(tile%w)+.5f,(float)(tile/w)+.5f))goto failure;
  }
  for(uint32_t o=0;o<map->objective_count;o++){
    const uint32_t tile=map->objectives[o].tile;
    if(!nacre_dungeon_placement_add(&result,
        map->exit_requirement==1 ? NACRE_DUNGEON_PART_OBJECTIVE_KEY :
        NACRE_DUNGEON_PART_OBJECTIVE_LEVER,
        (float)(tile%w)+.5f,(float)(tile/w)+.5f))goto failure;
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
