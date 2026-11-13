/* Native C regression for generated progression and visual placement.
 * Build independently of LDK with the six pure-C nacre modules.
 */
#include "../src/system/nacre_dungeon_generator.h"
#include "../src/system/nacre_dungeon_progression.h"
#include "../src/system/nacre_dungeon_navigation.h"
#include "../src/system/nacre_dungeon_placement.h"
#include "../src/system/nacre_dungeon_visibility.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static int move_delta(const NacreDungeonMap *m, uint32_t a, uint32_t b,
                      int32_t *dx, int32_t *dz)
{
  *dx = (int32_t)(b % m->width) - (int32_t)(a % m->width);
  *dz = (int32_t)(b / m->width) - (int32_t)(a / m->width);
  return ((*dx == 0 && (*dz == 1 || *dz == -1)) ||
          (*dz == 0 && (*dx == 1 || *dx == -1)));
}

int main(void)
{
  unsigned valid = 0u, rejected = 0u, gates = 0u, doors = 0u;
  unsigned treasures = 0u, totems = 0u, objectives = 0u;
  for (unsigned trial = 0u; trial < 1500u; ++trial)
  {
    NacreDungeonConfig cfg;
    nacre_dungeon_generator_config_defaults(&cfg);
    (void)snprintf(cfg.seed, sizeof(cfg.seed), "nacre-progression-%05u", trial);
    cfg.exit_requirement = trial % 3u; /* Free, keys, levers. */
    cfg.objective_count = 1u + trial % 6u;
    NacreDungeonMap map = {0};
    NacreDungeonResult result = nacre_dungeon_generator_generate(&cfg, &map);
    if (result == NACRE_DUNGEON_NO_VALID_LAYOUT)
    {
      ++rejected;
      continue;
    }
    if (result != NACRE_DUNGEON_OK ||
        !nacre_dungeon_generator_validate(&map) ||
        !nacre_dungeon_progression_validate(&map))
    {
      fprintf(stderr, "Invalid layout for seed %s: %d\n", cfg.seed, result);
      return 1;
    }
    assert(map.treasure_count >= 1u && map.treasure_count <= 3u);
    assert(map.totem_count == map.treasure_count);
    assert(map.objective_count == (cfg.exit_requirement ? cfg.objective_count : 0u));
    assert(map.progression_valid);
    assert(nacre_dungeon_progression_can_exit(&map) ==
           (cfg.exit_requirement == 0u));

    NacreDungeonVisibilityMap regions = {0};
    NacreDungeonPlacementList pieces = {0};
    assert(nacre_dungeon_visibility_build(&map, &regions));
    assert(nacre_dungeon_placements_build(&map, &regions, true, true, &pieces));

    unsigned gate_parts = 0, door_parts = 0, totem_parts = 0;
    unsigned goal_parts = 0, entry_parts = 0, exit_parts = 0;
    for (uint32_t i = 0u; i < pieces.count; ++i)
    {
      const NacreDungeonPartKind type = pieces.items[i].kind;
      gate_parts += type == NACRE_DUNGEON_PART_PORTCULLIS_X ||
                    type == NACRE_DUNGEON_PART_PORTCULLIS_Z;
      door_parts += type == NACRE_DUNGEON_PART_DOOR_X ||
                    type == NACRE_DUNGEON_PART_DOOR_Z;
      totem_parts += type == NACRE_DUNGEON_PART_TOTEM;
      goal_parts += type == NACRE_DUNGEON_PART_OBJECTIVE_KEY ||
                    type == NACRE_DUNGEON_PART_OBJECTIVE_LEVER;
      entry_parts += type == NACRE_DUNGEON_PART_ENTRY;
      exit_parts += type == NACRE_DUNGEON_PART_EXIT;
    }
    assert(entry_parts == 1u && exit_parts == 1u);
    assert(totem_parts == map.totem_count);
    assert(goal_parts == map.objective_count);
    unsigned logical_gates = 0, logical_doors = 0;
    for (uint32_t i = 0u; i < map.passage_count; ++i)
    {
      NacreDungeonPassage *passage = &map.passages[i];
      int32_t dx, dz;
      assert(move_delta(&map, passage->a, passage->b, &dx, &dz));
      if (passage->kind == NACRE_DUNGEON_PASSAGE_DOOR)
      {
        ++logical_doors;
        assert(!passage->opened);
        assert(nacre_dungeon_navigation_step(&map, passage->a, dx, dz, NULL) ==
               NACRE_DUNGEON_MOVE_DOOR);
        passage->opened = true;
        assert(nacre_dungeon_navigation_step(&map, passage->a, dx, dz, NULL) ==
               NACRE_DUNGEON_MOVE_ALLOWED);
        passage->opened = false;
      }
      if (passage->kind == NACRE_DUNGEON_PASSAGE_PORTCULLIS)
      {
        ++logical_gates;
        assert(!passage->opened);
        assert(passage->color != NACRE_DUNGEON_COLOR_NONE);
        assert(passage->totem_id > 0u && passage->totem_id <= map.totem_count);
        assert(map.totems[passage->totem_id - 1u].color == passage->color);
        assert(nacre_dungeon_navigation_step(&map, passage->a, dx, dz, NULL) ==
               NACRE_DUNGEON_MOVE_PORTCULLIS);
        passage->opened = true;
        assert(nacre_dungeon_navigation_step(&map, passage->a, dx, dz, NULL) ==
               NACRE_DUNGEON_MOVE_ALLOWED);
        passage->opened = false;
      }
    }
    assert(gate_parts == logical_gates);
    assert(door_parts == logical_doors);
    assert(logical_gates >= map.treasure_count);
    for (uint32_t i = 0; i < map.objective_count; ++i)
      map.objectives[i].completed = true;
    assert(nacre_dungeon_progression_can_exit(&map));

    ++valid;
    gates += logical_gates;
    doors += logical_doors;
    treasures += map.treasure_count;
    totems += map.totem_count;
    objectives += map.objective_count;
    nacre_dungeon_placements_release(&pieces);
    nacre_dungeon_visibility_release(&regions);
    nacre_dungeon_generator_release(&map);
  }
  printf("valid=%u rejected=%u treasures=%u totems=%u gates=%u "
         "doors=%u objectives=%u\n", valid, rejected, treasures, totems,
         gates, doors, objectives);
  assert(valid > 1000u && gates > treasures && doors > 0u);
  return 0;
}
