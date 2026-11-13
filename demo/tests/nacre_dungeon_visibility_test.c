/* Portable C11 tests; no LDK or renderer required. */
#include "nacre_dungeon_generator.h"
#include "nacre_dungeon_placement.h"
#include "nacre_dungeon_visibility.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool contains(const uint16_t *ids, uint32_t count, uint16_t id)
{
  for (uint32_t i = 0u; i < count; ++i) if (ids[i] == id) return true;
  return false;
}

static void four_room_shared_wall_test(void)
{
  /* Four rooms around a single 2 m wall: ALL FOUR regions own the wall.
   * A naive "one room per prefab" implementation would incorrectly hide it. */
  uint8_t cells[4] = {1, 1, 1, 1};
  int16_t rooms[4] = {0, 1, 2, 3};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = rooms;
  map.room_count = 4u;
  NacreDungeonVisibilityMap visibility = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  const NacreDungeonPlacement wall = {
    NACRE_DUNGEON_PART_WALL_X_2M, 1.0f, 1.0f};
  uint16_t ids[4];
  const uint32_t count = nacre_dungeon_visibility_piece_regions(
      &map, &visibility, &wall, ids);
  assert(count == 4u);
  for (uint16_t id = 1u; id <= 4u; ++id) assert(contains(ids, count, id));
  const NacreDungeonPlacement floor = {
    NACRE_DUNGEON_PART_FLOOR, 1.5f, .5f};
  assert(nacre_dungeon_visibility_piece_regions(
      &map, &visibility, &floor, ids) == 1u);
  assert(ids[0] == 2u);
  nacre_dungeon_visibility_release(&visibility);
}

static void corridor_and_boundary_test(void)
{
  /* Room on the left, two connected corridor cells on the right, plus
   * another isolated corridor. Corridors must NOT merge with rooms. */
  uint8_t cells[9] = {1, 2, 2, 0, 0, 0, 0, 2, 0};
  int16_t rooms[9] = {0, -1, -1, -1, -1, -1, -1, -1, -1};
  NacreDungeonMap map = {0};
  map.width = map.height = 3u;
  map.cells = cells;
  map.room_ids = rooms;
  map.room_count = 1u;
  NacreDungeonVisibilityMap visibility = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(visibility.region_count == 3u);
  assert(visibility.region_by_tile[0] == 1u);
  assert(visibility.region_by_tile[1] == 2u);
  assert(visibility.region_by_tile[2] == 2u);
  assert(visibility.region_by_tile[7] == 3u);
  assert(visibility.region_by_tile[3] == 0u);
  uint16_t ids[4];
  const NacreDungeonPlacement shared_passage = {
    NACRE_DUNGEON_PART_PASSAGE_Z, 1.0f, .5f};
  assert(nacre_dungeon_visibility_piece_regions(
      &map, &visibility, &shared_passage, ids) == 2u);
  assert(contains(ids, 2u, 1u));
  assert(contains(ids, 2u, 2u));
  const NacreDungeonPlacement outside_wall = {
    NACRE_DUNGEON_PART_WALL_X_1M, .5f, 0.0f};
  assert(nacre_dungeon_visibility_piece_regions(
      &map, &visibility, &outside_wall, ids) == 1u);
  assert(ids[0] == 1u);
  nacre_dungeon_visibility_release(&visibility);
}

static void generated_maps_test(void)
{
  NacreDungeonConfig cfg;
  nacre_dungeon_generator_config_defaults(&cfg);
  uint32_t generated = 0u;
  for (uint32_t seed = 0u; seed < 220u; ++seed)
  {
    snprintf(cfg.seed, sizeof(cfg.seed), "nacre-visibility-%u", seed);
    NacreDungeonMap map = {0};
    const NacreDungeonResult result =
        nacre_dungeon_generator_generate(&cfg, &map);
    if (result == NACRE_DUNGEON_NO_VALID_LAYOUT) continue;
    assert(result == NACRE_DUNGEON_OK);
    assert(nacre_dungeon_generator_validate(&map));
    NacreDungeonVisibilityMap visibility = {0};
    assert(nacre_dungeon_visibility_build(&map, &visibility));
    assert(visibility.region_count >= map.room_count);
    assert(visibility.region_by_tile[map.entry] != 0u);
    for (uint32_t i = 0u; i < map.width * map.height; ++i)
    {
      if (map.cells[i]) assert(visibility.region_by_tile[i] != 0u);
      else assert(visibility.region_by_tile[i] == 0u);
    }
    NacreDungeonPlacementList placements = {0};
    assert(nacre_dungeon_placements_build(
        &map, &visibility, true, true, &placements));
    for (uint32_t i = 0u; i < placements.count; ++i)
    {
      uint16_t ids[4] = {0};
      const uint32_t count = nacre_dungeon_visibility_piece_regions(
          &map, &visibility, &placements.items[i], ids);
      assert(count > 0u && count <= 4u);
      for (uint32_t j = 0u; j < count; ++j)
      {
        assert(ids[j] != 0u && ids[j] <= visibility.region_count);
        for (uint32_t k = j + 1u; k < count; ++k)
          assert(ids[j] != ids[k]);
      }
    }
    nacre_dungeon_placements_release(&placements);
    nacre_dungeon_visibility_release(&visibility);
    nacre_dungeon_generator_release(&map);
    ++generated;
  }
  assert(generated > 150u);
  printf("Visibility: %u real dungeon layouts verified.\n", generated);
}

int main(void)
{
  four_room_shared_wall_test();
  corridor_and_boundary_test();
  generated_maps_test();
  puts("All Nacre visibility tests passed.");
  return 0;
}
