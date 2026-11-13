#ifndef NACRE_DUNGEON_NAVIGATION_H
#define NACRE_DUNGEON_NAVIGATION_H

#include "nacre_dungeon_generator.h"

/* A conservative, topology-driven movement test.  Quads are visuals, never
 * physics colliders: the generated cell/edge graph is the source of truth.
 * Only one orthogonal cell may be crossed per request.  Future runtime
 * door/gate/destruction states belong here, not in the scene mesh. */
typedef enum NacreDungeonMoveResult
{
  NACRE_DUNGEON_MOVE_ALLOWED = 0,
  NACRE_DUNGEON_MOVE_OUTSIDE,
  NACRE_DUNGEON_MOVE_VOID,
  NACRE_DUNGEON_MOVE_WALL,
  NACRE_DUNGEON_MOVE_CRACKED,
  NACRE_DUNGEON_MOVE_DOOR,
  NACRE_DUNGEON_MOVE_PORTCULLIS
} NacreDungeonMoveResult;

NacreDungeonMoveResult nacre_dungeon_navigation_step(
    const NacreDungeonMap *map, uint32_t from,
    int32_t dx, int32_t dz, uint32_t *destination);

#endif // NACRE_DUNGEON_NAVIGATION_H
