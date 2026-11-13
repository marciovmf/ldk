#include "nacre_dungeon_visibility.h"

#include <stdlib.h>
#include <string.h>

void nacre_dungeon_visibility_release(NacreDungeonVisibilityMap *visibility)
{
  if (!visibility) return;
  free(visibility->region_by_tile);
  memset(visibility, 0, sizeof(*visibility));
}

bool nacre_dungeon_visibility_build(
    const NacreDungeonMap *map, NacreDungeonVisibilityMap *out)
{
  if (!map || !out || !map->cells || !map->room_ids ||
      !map->width || !map->height ||
      map->width > NACRE_DUNGEON_MAX_DIMENSION ||
      map->height > NACRE_DUNGEON_MAX_DIMENSION ||
      map->room_count + map->secret_room_count >= UINT16_MAX) return false;

  const uint32_t size = map->width * map->height;
  uint16_t *regions = (uint16_t *)calloc(size, sizeof(*regions));
  uint32_t *queue = (uint32_t *)malloc((size_t)size * sizeof(*queue));
  if (!regions || !queue)
  {
    free(regions);
    free(queue);
    return false;
  }

  /* Room IDs are stable across all the cells of one room. */
  for (uint32_t i = 0; i < size; ++i)
  {
    if (!map->cells[i]) continue;
    const int32_t room_id = map->room_ids[i];
    if (room_id >= 0)
    {
      if ((uint32_t)room_id >= map->room_count + map->secret_room_count)
      {
        free(regions);
        free(queue);
        return false;
      }
      regions[i] = (uint16_t)(room_id + 1);
    }
  }

  /* A corridor is visible as one connected section. Never flood-fill into
   * rooms: a passage/door is a visibility boundary, not a corridor merger. */
  uint32_t region_count = map->room_count + map->secret_room_count;
  for (uint32_t start = 0; start < size; ++start)
  {
    if (!map->cells[start] || regions[start]) continue;
    if (region_count >= UINT16_MAX)
    {
      free(regions);
      free(queue);
      return false;
    }
    const uint16_t region = (uint16_t)++region_count;
    uint32_t head = 0u, tail = 0u;
    queue[tail++] = start;
    regions[start] = region;
    while (head < tail)
    {
      const uint32_t tile = queue[head++];
      const uint32_t x = tile % map->width;
      const uint32_t z = tile / map->width;
      uint32_t neighbor[4];
      uint32_t count = 0u;
      if (x > 0u) neighbor[count++] = tile - 1u;
      if (x + 1u < map->width) neighbor[count++] = tile + 1u;
      if (z > 0u) neighbor[count++] = tile - map->width;
      if (z + 1u < map->height) neighbor[count++] = tile + map->width;
      for (uint32_t j = 0; j < count; ++j)
      {
        const uint32_t other = neighbor[j];
        if (map->cells[other] == map->cells[start] &&
            map->room_ids[other] < 0 && !regions[other])
        {
          regions[other] = region;
          queue[tail++] = other;
        }
      }
    }
  }
  free(queue);
  out->region_by_tile = regions;
  out->tile_count = size;
  out->region_count = (uint16_t)region_count;
  return true;
}

static void nacre_dungeon_visibility_add_region(
    const NacreDungeonMap *map, const NacreDungeonVisibilityMap *visibility,
    int32_t x, int32_t z, uint16_t regions[4], uint32_t *count)
{
  if (x < 0 || z < 0 || x >= (int32_t)map->width ||
      z >= (int32_t)map->height) return;
  const uint32_t index = (uint32_t)z * map->width + (uint32_t)x;
  if (index >= visibility->tile_count) return;
  const uint16_t region = visibility->region_by_tile[index];
  if (!region) return;
  for (uint32_t i = 0u; i < *count; ++i)
    if (regions[i] == region) return;
  if (*count < 4u) regions[(*count)++] = region;
}

uint32_t nacre_dungeon_visibility_piece_regions(
    const NacreDungeonMap *map, const NacreDungeonVisibilityMap *visibility,
    const NacreDungeonPlacement *piece, uint16_t out_regions[4])
{
  if (!map || !visibility || !visibility->region_by_tile ||
      !piece || !out_regions) return 0u;
  uint32_t count = 0u;
  if (piece->kind == NACRE_DUNGEON_PART_FLOOR ||
      piece->kind == NACRE_DUNGEON_PART_PILLAR_INTACT ||
      piece->kind == NACRE_DUNGEON_PART_PILLAR_BROKEN ||
      piece->kind == NACRE_DUNGEON_PART_CAVE_FLOOR ||
      piece->kind == NACRE_DUNGEON_PART_TOTEM ||
      piece->kind == NACRE_DUNGEON_PART_ENTRY ||
      piece->kind == NACRE_DUNGEON_PART_EXIT ||
      piece->kind == NACRE_DUNGEON_PART_OBJECTIVE_KEY ||
      piece->kind == NACRE_DUNGEON_PART_OBJECTIVE_LEVER)
  {
    /* Floor pivots are at exact tile centers (x+.5, z+.5). */
    nacre_dungeon_visibility_add_region(map, visibility,
        (int32_t)piece->x, (int32_t)piece->z, out_regions, &count);
  }
  else if (piece->kind == NACRE_DUNGEON_PART_WALL_X_1M ||
      piece->kind == NACRE_DUNGEON_PART_WALL_X_2M ||
      piece->kind == NACRE_DUNGEON_PART_PASSAGE_X ||
      piece->kind == NACRE_DUNGEON_PART_HOLE_X ||
      piece->kind == NACRE_DUNGEON_PART_CRACKED_X ||
      piece->kind == NACRE_DUNGEON_PART_SECRET_X ||
      piece->kind == NACRE_DUNGEON_PART_CAVE_WALL_X ||
      piece->kind == NACRE_DUNGEON_PART_DOOR_X ||
      piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_X)
  {
    /* Horizontal wall at a whole Z coordinate: cells above and below it.
     * A 2 m wall can touch two cells on EACH side. */
    const uint32_t length =
        piece->kind == NACRE_DUNGEON_PART_WALL_X_2M ? 2u : 1u;
    const int32_t x0 = (int32_t)(piece->x - (float)length * .5f);
    const int32_t z = (int32_t)piece->z;
    for (uint32_t i = 0u; i < length; ++i)
    {
      nacre_dungeon_visibility_add_region(map, visibility,
          x0 + (int32_t)i, z - 1, out_regions, &count);
      nacre_dungeon_visibility_add_region(map, visibility,
          x0 + (int32_t)i, z, out_regions, &count);
    }
  }
  else if (piece->kind == NACRE_DUNGEON_PART_WALL_Z_1M ||
      piece->kind == NACRE_DUNGEON_PART_WALL_Z_2M ||
      piece->kind == NACRE_DUNGEON_PART_PASSAGE_Z ||
      piece->kind == NACRE_DUNGEON_PART_HOLE_Z ||
      piece->kind == NACRE_DUNGEON_PART_CRACKED_Z ||
      piece->kind == NACRE_DUNGEON_PART_SECRET_Z ||
      piece->kind == NACRE_DUNGEON_PART_CAVE_WALL_Z ||
      piece->kind == NACRE_DUNGEON_PART_DOOR_Z ||
      piece->kind == NACRE_DUNGEON_PART_PORTCULLIS_Z)
  {
    const uint32_t length =
        piece->kind == NACRE_DUNGEON_PART_WALL_Z_2M ? 2u : 1u;
    const int32_t x = (int32_t)piece->x;
    const int32_t z0 = (int32_t)(piece->z - (float)length * .5f);
    for (uint32_t i = 0u; i < length; ++i)
    {
      nacre_dungeon_visibility_add_region(map, visibility,
          x - 1, z0 + (int32_t)i, out_regions, &count);
      nacre_dungeon_visibility_add_region(map, visibility,
          x, z0 + (int32_t)i, out_regions, &count);
    }
  }
  return count;
}
