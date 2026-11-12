/* Placement regression: checks the physical scene plan independently of ECS. */
#include "../src/system/nacre_dungeon_placement.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NACRE_TEST_MAX 100u

static void nacre_test_verify(const NacreDungeonMap *map, bool merge_x, bool merge_z)
{
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_placements_build(map, merge_x, merge_z, &pieces));
  uint32_t floor_count = 0u;
  uint32_t opening_count = 0u;
  uint32_t hole_count = 0u;
  uint32_t cracked_count = 0u;
  uint32_t wall_segments = 0u;
  const uint32_t w = map->width, h = map->height;
  const size_t hc = (size_t)w * (h + 1u);
  const size_t vc = (size_t)(w + 1u) * h;
  uint8_t *horizontal = calloc(hc, 1);
  uint8_t *vertical = calloc(vc, 1);
  assert(horizontal && vertical);

  for (uint32_t k = 0; k < pieces.count; ++k)
  {
    const NacreDungeonPlacement *p = &pieces.items[k];
    const float x = p->x, z = p->z;
    if (p->kind == NACRE_DUNGEON_PART_FLOOR)
    {
      ++floor_count;
      uint32_t gx = (uint32_t)(x - .5f), gz = (uint32_t)(z - .5f);
      assert(gx < w && gz < h && map->cells[gz * w + gx]);
      continue;
    }
    bool axis_x = p->kind == NACRE_DUNGEON_PART_WALL_X_1M ||
        p->kind == NACRE_DUNGEON_PART_WALL_X_2M ||
        p->kind == NACRE_DUNGEON_PART_PASSAGE_X ||
        p->kind == NACRE_DUNGEON_PART_HOLE_X ||
        p->kind == NACRE_DUNGEON_PART_CRACKED_X;
    bool is_2m = p->kind == NACRE_DUNGEON_PART_WALL_X_2M ||
        p->kind == NACRE_DUNGEON_PART_WALL_Z_2M;
    uint32_t width = is_2m ? 2u : 1u;
    if (axis_x)
    {
      int start = (int)roundf(x - (float)width * .5f);
      int row = (int)roundf(z);
      assert(start >= 0 && row >= 0 && start + (int)width <= (int)w && row <= (int)h);
      for (uint32_t u = 0; u < width; ++u)
      {
        size_t i = (size_t)row * w + (uint32_t)start + u;
        assert(horizontal[i] == 0);
        horizontal[i] = 1;
      }
    }
    else
    {
      int col = (int)roundf(x);
      int start = (int)roundf(z - (float)width * .5f);
      assert(start >= 0 && col >= 0 && col <= (int)w && start + (int)width <= (int)h);
      for (uint32_t u = 0; u < width; ++u)
      {
        size_t i = (size_t)(start + (int)u) * (w + 1u) + (uint32_t)col;
        assert(vertical[i] == 0);
        vertical[i] = 1;
      }
    }
    if (p->kind == NACRE_DUNGEON_PART_PASSAGE_X ||
        p->kind == NACRE_DUNGEON_PART_PASSAGE_Z) ++opening_count;
    if (p->kind == NACRE_DUNGEON_PART_HOLE_X ||
        p->kind == NACRE_DUNGEON_PART_HOLE_Z) ++hole_count;
    if (p->kind == NACRE_DUNGEON_PART_CRACKED_X ||
        p->kind == NACRE_DUNGEON_PART_CRACKED_Z) ++cracked_count;
    if (p->kind >= NACRE_DUNGEON_PART_WALL_X_1M &&
        p->kind <= NACRE_DUNGEON_PART_WALL_Z_2M) wall_segments += width;
  }
  uint32_t expected_floor = 0u;
  for (uint32_t i = 0; i < w * h; ++i) if (map->cells[i]) ++expected_floor;
  assert(floor_count == expected_floor);

  uint32_t expected_open = 0u, expected_hole = 0u, expected_cracked = 0u;
  for (uint32_t i = 0; i < map->edge_count; ++i)
  {
    switch (map->edges[i].type)
    {
      case NACRE_DUNGEON_EDGE_OPEN: ++expected_open; break;
      case NACRE_DUNGEON_EDGE_HOLE: ++expected_hole; break;
      case NACRE_DUNGEON_EDGE_CRACKED: ++expected_cracked; break;
    }
  }
  assert(opening_count == expected_open);
  assert(hole_count == expected_hole);
  assert(cracked_count == expected_cracked);

  /* Every boundary between walkable and nonwalkable cells is walled;
   * after decomposing 2 m meshes, each physical boundary appears once. */
  for (uint32_t z = 0; z < h; ++z)
  {
    for (uint32_t x = 0; x < w; ++x)
    {
      uint32_t i = z * w + x;
      if (!map->cells[i]) continue;
      if (z == 0u || !map->cells[i - w])
        assert(horizontal[(size_t)z * w + x] == 1);
      if (z + 1u == h || !map->cells[i + w])
        assert(horizontal[(size_t)(z + 1u) * w + x] == 1);
      if (x == 0u || !map->cells[i - 1u])
        assert(vertical[(size_t)z * (w + 1u) + x] == 1);
      if (x + 1u == w || !map->cells[i + 1u])
        assert(vertical[(size_t)z * (w + 1u) + x + 1u] == 1);
    }
  }
  free(horizontal);
  free(vertical);
  nacre_dungeon_placements_release(&pieces);
  assert(pieces.count == 0u && pieces.items == NULL);
  (void)wall_segments;
}

int main(void)
{
  NacreDungeonConfig config;
  nacre_dungeon_generator_config_defaults(&config);
  uint32_t checked = 0u;
  for (uint32_t i = 0; i < 250u; ++i)
  {
    snprintf(config.seed, sizeof(config.seed), "LDK-%03u", i + 1u);
    NacreDungeonMap map = {0};
    NacreDungeonResult result = nacre_dungeon_generator_generate(&config, &map);
    if (result == NACRE_DUNGEON_NO_VALID_LAYOUT) continue;
    assert(result == NACRE_DUNGEON_OK);
    assert(nacre_dungeon_generator_validate(&map));
    nacre_test_verify(&map, false, false);
    nacre_test_verify(&map, true, true);
    nacre_dungeon_generator_release(&map);
    ++checked;
  }
  printf("Nacre placement plan: %u seeds, both 1 m and 2 m modes OK\n", checked);
  return 0;
}
