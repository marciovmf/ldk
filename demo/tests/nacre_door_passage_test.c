/* Regressions for the architectural rule: each authored door/portcullis is
 * two *independent* pieces on the same edge: Passage plus blocker.
 * Built using only the C11 generator, without LDK/ECS dependencies. */
#include "../src/system/nacre_dungeon_generator.h"
#include "../src/system/nacre_dungeon_placement.h"
#include "../src/system/nacre_dungeon_visibility.h"
#include <assert.h>
#include <stdio.h>

static unsigned count_at(const NacreDungeonPlacementList *items,
    NacreDungeonPartKind kind, float x, float z)
{
  unsigned count = 0;
  for (uint32_t i = 0; i < items->count; ++i)
    if (items->items[i].kind == kind &&
        items->items[i].x == x && items->items[i].z == z) ++count;
  return count;
}

int main(void)
{
  unsigned valid = 0, gates = 0, doors = 0;
  for (unsigned trial = 0; trial < 500; ++trial)
  {
    NacreDungeonConfig cfg;
    nacre_dungeon_generator_config_defaults(&cfg);
    (void)snprintf(cfg.seed, sizeof(cfg.seed), "nacre-double-passage-%04u", trial);
    NacreDungeonMap map = {0};
    const NacreDungeonResult result = nacre_dungeon_generator_generate(&cfg, &map);
    if (result == NACRE_DUNGEON_NO_VALID_LAYOUT) continue;
    assert(result == NACRE_DUNGEON_OK);
    NacreDungeonVisibilityMap regions = {0};
    NacreDungeonPlacementList list = {0};
    assert(nacre_dungeon_visibility_build(&map, &regions));
    assert(nacre_dungeon_placements_build(&map, &regions, true, &list));
    for (uint32_t i = 0; i < map.passage_count; ++i)
    {
      const NacreDungeonPassage *p = &map.passages[i];
      if (p->kind != NACRE_DUNGEON_PASSAGE_DOOR &&
          p->kind != NACRE_DUNGEON_PASSAGE_PORTCULLIS) continue;
      const uint32_t ax = p->a % map.width, az = p->a / map.width;
      const uint32_t bx = p->b % map.width, bz = p->b / map.width;
      const bool x_aligned = ax == bx;
      assert(x_aligned ? (az + 1 == bz || bz + 1 == az) :
          (az == bz && (ax + 1 == bx || bx + 1 == ax)));
      const float x = x_aligned ? (float)ax + 0.5f :
          (float)(ax > bx ? ax : bx);
      const float z = x_aligned ? (float)(az > bz ? az : bz) :
          (float)az + 0.5f;
      const NacreDungeonPartKind passage_kind = x_aligned ?
          NACRE_DUNGEON_PART_PASSAGE_X : NACRE_DUNGEON_PART_PASSAGE_Z;
      const NacreDungeonPartKind barrier_kind =
          p->kind == NACRE_DUNGEON_PASSAGE_DOOR ?
          (x_aligned ? NACRE_DUNGEON_PART_DOOR_X : NACRE_DUNGEON_PART_DOOR_Z) :
          (x_aligned ? NACRE_DUNGEON_PART_PORTCULLIS_X :
              NACRE_DUNGEON_PART_PORTCULLIS_Z);
      assert(count_at(&list, passage_kind, x, z) == 1);
      assert(count_at(&list, barrier_kind, x, z) == 1);
      gates += p->kind == NACRE_DUNGEON_PASSAGE_PORTCULLIS;
      doors += p->kind == NACRE_DUNGEON_PASSAGE_DOOR;
    }
    /* Authoring an opened state must not collapse the two assets into one.
     * Open state belongs to the barrier animation, not to the architecture. */
    for (uint32_t i = 0; i < map.passage_count; ++i)
      map.passages[i].opened = true;
    NacreDungeonPlacementList open_list = {0};
    assert(nacre_dungeon_placements_build(
        &map, &regions, true, &open_list));
    for (uint32_t i = 0; i < map.passage_count; ++i)
    {
      const NacreDungeonPassage *p = &map.passages[i];
      if (p->kind != NACRE_DUNGEON_PASSAGE_DOOR &&
          p->kind != NACRE_DUNGEON_PASSAGE_PORTCULLIS) continue;
      const uint32_t ax = p->a % map.width, az = p->a / map.width;
      const uint32_t bx = p->b % map.width, bz = p->b / map.width;
      const bool x_aligned = ax == bx;
      const float x = x_aligned ? (float)ax + .5f :
          (float)(ax > bx ? ax : bx);
      const float z = x_aligned ? (float)(az > bz ? az : bz) :
          (float)az + .5f;
      assert(count_at(&open_list, x_aligned ?
          NACRE_DUNGEON_PART_PASSAGE_X : NACRE_DUNGEON_PART_PASSAGE_Z,
          x, z) == 1u);
      assert(count_at(&open_list,
          p->kind == NACRE_DUNGEON_PASSAGE_DOOR ?
              (x_aligned ? NACRE_DUNGEON_PART_DOOR_X : NACRE_DUNGEON_PART_DOOR_Z) :
              (x_aligned ? NACRE_DUNGEON_PART_PORTCULLIS_X :
                  NACRE_DUNGEON_PART_PORTCULLIS_Z), x, z) == 1u);
    }
    nacre_dungeon_placements_release(&open_list);
    ++valid;
    nacre_dungeon_placements_release(&list);
    nacre_dungeon_visibility_release(&regions);
    nacre_dungeon_generator_release(&map);
  }
  assert(valid > 450 && doors && gates);
  printf("PASS: valid=%u doors=%u gates=%u, each with separate Passage\n",
      valid, doors, gates);
  return 0;
}
