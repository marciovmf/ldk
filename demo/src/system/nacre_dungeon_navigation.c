/**
 * @file nacre_dungeon_navigation.c
 * @brief First-stage grid collision independent of the renderer and LDK.
 *
 * The collision graph must agree with nacre_dungeon_placement.c:
 * - Empty cells block traversal, even if their floor or wall prefab is missing.
 * - Internal boundaries of a room or corridor are open.
 * - Between different regions, ONLY explicitly generated edges are openings.
 * - Open arches and holes are traversable; cracked walls stay blocked until a
 *   future gameplay system explicitly changes their state.
 * - Unimplemented doors/portcullises are NOT assumed to be open.
 */
#include "nacre_dungeon_navigation.h"

NacreDungeonMoveResult nacre_dungeon_navigation_step(
    const NacreDungeonMap *map, uint32_t from,
    int32_t dx, int32_t dz, uint32_t *destination)
{
  if (!map || !map->cells || !map->room_ids || !map->width || !map->height ||
      from >= map->width * map->height ||
      (dx != 0 && dz != 0) ||
      (dx == 0 && dz == 0) ||
      (dx < -1 || dx > 1 || dz < -1 || dz > 1))
    return NACRE_DUNGEON_MOVE_OUTSIDE;

  const int32_t x = (int32_t)(from % map->width) + dx;
  const int32_t z = (int32_t)(from / map->width) + dz;
  if (x < 0 || z < 0 || x >= (int32_t)map->width ||
      z >= (int32_t)map->height)
    return NACRE_DUNGEON_MOVE_OUTSIDE;

  const uint32_t to = (uint32_t)z * map->width + (uint32_t)x;
  if (!map->cells[from] || !map->cells[to])
    return NACRE_DUNGEON_MOVE_VOID;

  /* All pillar variants block, regardless of their rendered shape. */
  if (map->pillar_mask && map->pillar_mask[to])
    return NACRE_DUNGEON_MOVE_WALL;

  if (map->room_ids[from] != map->room_ids[to] ||
      ((map->cells[from] == NACRE_DUNGEON_CAVE) !=
       (map->cells[to] == NACRE_DUNGEON_CAVE)))
  {
    /* No implicit passage through a wall just because two floor cells touch.
     * Both endpoints are always cardinally adjacent by the check above. */
    for (uint32_t i = 0; i < map->edge_count; ++i)
    {
      const NacreDungeonEdge *e = &map->edges[i];
      if (!((e->a == from && e->b == to) ||
            (e->a == to && e->b == from)))
        continue;
      if (e->type == NACRE_DUNGEON_EDGE_CRACKED)
        return NACRE_DUNGEON_MOVE_CRACKED;
      if (e->type == NACRE_DUNGEON_EDGE_SECRET)
        return NACRE_DUNGEON_MOVE_WALL;
      if (e->type == NACRE_DUNGEON_EDGE_OPEN)
      {
        const uint32_t a=from<to?from:to,b=from<to?to:from;
        for(uint32_t p=0;p<map->passage_count;p++){
          const NacreDungeonPassage *passage=&map->passages[p];
          if(passage->a!=a||passage->b!=b)continue;
          if(!passage->opened && passage->kind==NACRE_DUNGEON_PASSAGE_DOOR)
            return NACRE_DUNGEON_MOVE_DOOR;
          if(!passage->opened && passage->kind==NACRE_DUNGEON_PASSAGE_PORTCULLIS)
            return NACRE_DUNGEON_MOVE_PORTCULLIS;
          break;
        }
      }
      if (e->type == NACRE_DUNGEON_EDGE_OPEN ||
          e->type == NACRE_DUNGEON_EDGE_HOLE)
      {
        if (destination) *destination = to;
        return NACRE_DUNGEON_MOVE_ALLOWED;
      }
      return NACRE_DUNGEON_MOVE_WALL;
    }
    return NACRE_DUNGEON_MOVE_WALL;
  }

  if (destination) *destination = to;
  return NACRE_DUNGEON_MOVE_ALLOWED;
}
