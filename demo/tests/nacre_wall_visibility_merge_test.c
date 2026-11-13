/*
 * Visibility-safe wall packing regressions.
 * Build against the same C-only generator/placement/visibility modules.
 * No rendering, ECS, Windows SDK or LDK runtime required.
 */
#include "../src/system/nacre_dungeon_generator.h"
#include "../src/system/nacre_dungeon_placement.h"
#include "../src/system/nacre_dungeon_visibility.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool same_regions(const uint16_t a[4], uint32_t na,
                         const uint16_t b[4], uint32_t nb)
{
  if (na != nb || na == 0u) return false;
  for (uint32_t i = 0; i < na; ++i)
  {
    bool found = false;
    for (uint32_t j = 0; j < nb; ++j)
      if (a[i] == b[j]) found = true;
    if (!found) return false;
  }
  return true;
}

static uint32_t count_kind_at(const NacreDungeonPlacementList *list,
    NacreDungeonPartKind kind, float x, float z)
{
  uint32_t count = 0u;
  for (uint32_t i = 0; i < list->count; ++i)
  {
    const NacreDungeonPlacement *p = &list->items[i];
    if (p->kind == kind && p->x == x && p->z == z) ++count;
  }
  return count;
}

/* The exact case that used to leak a wall into a different visible room. */
static void horizontal_different_rooms_must_not_merge(void)
{
  uint8_t cells[4] = {1, 1, 0, 0};
  int16_t room_ids[4] = {0, 1, -1, -1};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 2u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  /* These neighbors are collinear ordinary walls, but are owned by different
   * rooms, so both the top and bottom boundaries remain separate 1 m pieces. */
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_2M,
      1.0f, 1.0f) == 0u);
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_1M,
      .5f, 1.0f) == 1u);
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_1M,
      1.5f, 1.0f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

static void horizontal_same_room_must_merge(void)
{
  uint8_t cells[4] = {1, 1, 0, 0};
  int16_t room_ids[4] = {0, 0, -1, -1};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 1u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_2M,
      1.0f, 1.0f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

static void horizontal_shared_boundary_must_merge(void)
{
  uint8_t cells[4] = {1, 1, 1, 1};
  int16_t room_ids[4] = {0, 0, 1, 1};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 2u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  /* Both edges are owned by the SAME set {room A, room B}. */
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_2M,
      1.0f, 1.0f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

static void vertical_different_rooms_must_not_merge(void)
{
  uint8_t cells[4] = {1, 0, 1, 0};
  int16_t room_ids[4] = {0, -1, 1, -1};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 2u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_Z_2M,
      1.0f, 1.0f) == 0u);
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_Z_1M,
      1.0f, .5f) == 1u);
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_Z_1M,
      1.0f, 1.5f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

static void vertical_same_room_must_merge(void)
{
  uint8_t cells[4] = {1, 0, 1, 0};
  int16_t room_ids[4] = {0, -1, 0, -1};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 1u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_Z_2M,
      1.0f, 1.0f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

static void same_region_set_in_reversed_order_must_merge(void)
{
  uint8_t cells[4] = {1, 1, 1, 1};
  int16_t room_ids[4] = {0, 1, 1, 0};
  NacreDungeonMap map = {0};
  map.width = map.height = 2u;
  map.cells = cells;
  map.room_ids = room_ids;
  map.room_count = 2u;
  NacreDungeonVisibilityMap visibility = {0};
  NacreDungeonPlacementList pieces = {0};
  assert(nacre_dungeon_visibility_build(&map, &visibility));
  assert(nacre_dungeon_placements_build(
      &map, &visibility, true, true, &pieces));
  /* Region ordering must not affect merge decisions. */
  assert(count_kind_at(&pieces, NACRE_DUNGEON_PART_WALL_X_2M,
      1.0f, 1.0f) == 1u);
  nacre_dungeon_placements_release(&pieces);
  nacre_dungeon_visibility_release(&visibility);
}

typedef struct CanonicalPiece
{
  int32_t kind;
  int32_t x2;
  int32_t z2;
} CanonicalPiece;

static int canonical_compare(const void *lhs, const void *rhs)
{
  const CanonicalPiece *a = lhs, *b = rhs;
  if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
  if (a->x2 != b->x2) return a->x2 < b->x2 ? -1 : 1;
  if (a->z2 != b->z2) return a->z2 < b->z2 ? -1 : 1;
  return 0;
}

static uint32_t flatten(const NacreDungeonPlacementList *source,
    CanonicalPiece *dst)
{
  uint32_t count = 0u;
  for (uint32_t i = 0; i < source->count; ++i)
  {
    const NacreDungeonPlacement *p = &source->items[i];
    const int32_t x2 = (int32_t)(p->x * 2.f);
    const int32_t z2 = (int32_t)(p->z * 2.f);
    if (p->kind == NACRE_DUNGEON_PART_WALL_X_2M)
    {
      dst[count++] = (CanonicalPiece){NACRE_DUNGEON_PART_WALL_X_1M,
          x2 - 1, z2};
      dst[count++] = (CanonicalPiece){NACRE_DUNGEON_PART_WALL_X_1M,
          x2 + 1, z2};
    }
    else if (p->kind == NACRE_DUNGEON_PART_WALL_Z_2M)
    {
      dst[count++] = (CanonicalPiece){NACRE_DUNGEON_PART_WALL_Z_1M,
          x2, z2 - 1};
      dst[count++] = (CanonicalPiece){NACRE_DUNGEON_PART_WALL_Z_1M,
          x2, z2 + 1};
    }
    else dst[count++] = (CanonicalPiece){p->kind, x2, z2};
  }
  return count;
}

static void validate_merged_wall_owners(
    const NacreDungeonMap *map,
    const NacreDungeonVisibilityMap *visibility,
    const NacreDungeonPlacementList *packed,
    uint32_t *out_merged)
{
  for (uint32_t i = 0u; i < packed->count; ++i)
  {
    const NacreDungeonPlacement *p = &packed->items[i];
    const bool along_x = p->kind == NACRE_DUNGEON_PART_WALL_X_2M;
    if (!along_x && p->kind != NACRE_DUNGEON_PART_WALL_Z_2M)
      continue;
    NacreDungeonPlacement a = {along_x ? NACRE_DUNGEON_PART_WALL_X_1M :
        NACRE_DUNGEON_PART_WALL_Z_1M,
        p->x - (along_x ? .5f : 0.f),
        p->z - (along_x ? 0.f : .5f)};
    NacreDungeonPlacement b = {a.kind,
        p->x + (along_x ? .5f : 0.f),
        p->z + (along_x ? 0.f : .5f)};
    uint16_t ar[4] = {0}, br[4] = {0};
    uint32_t na = nacre_dungeon_visibility_piece_regions(
        map, visibility, &a, ar);
    uint32_t nb = nacre_dungeon_visibility_piece_regions(
        map, visibility, &b, br);
    assert(same_regions(ar, na, br, nb));
    ++*out_merged;
  }
}

static void generated_maps_must_preserve_edges_and_owner_sets(void)
{
  NacreDungeonConfig cfg;
  nacre_dungeon_generator_config_defaults(&cfg);
  uint32_t valid = 0u, merged = 0u;
  for (uint32_t i = 0u; i < 500u; ++i)
  {
    snprintf(cfg.seed, sizeof(cfg.seed), "nacre-wall-regions-%u", i);
    NacreDungeonMap map = {0};
    NacreDungeonResult result = nacre_dungeon_generator_generate(&cfg, &map);
    if (result == NACRE_DUNGEON_NO_VALID_LAYOUT) continue;
    assert(result == NACRE_DUNGEON_OK);
    assert(nacre_dungeon_generator_validate(&map));
    NacreDungeonVisibilityMap visibility = {0};
    NacreDungeonPlacementList single = {0}, packed = {0};
    assert(nacre_dungeon_visibility_build(&map, &visibility));
    assert(nacre_dungeon_placements_build(
        &map, &visibility, false, false, &single));
    assert(nacre_dungeon_placements_build(
        &map, &visibility, true, true, &packed));
    validate_merged_wall_owners(&map, &visibility, &packed, &merged);

    /* Compacting walls must neither remove nor duplicate physical edges,
     * floors, or special passages. Compare after expanding every 2 m mesh. */
    const uint32_t max_a = single.count * 2u;
    const uint32_t max_b = packed.count * 2u;
    CanonicalPiece *a = (CanonicalPiece *)malloc(max_a * sizeof(*a));
    CanonicalPiece *b = (CanonicalPiece *)malloc(max_b * sizeof(*b));
    assert(a && b);
    uint32_t na = flatten(&single, a), nb = flatten(&packed, b);
    assert(na == nb);
    qsort(a, na, sizeof(*a), canonical_compare);
    qsort(b, nb, sizeof(*b), canonical_compare);
    for (uint32_t j = 0; j < na; ++j)
      assert(canonical_compare(&a[j], &b[j]) == 0);
    free(a);
    free(b);
    nacre_dungeon_placements_release(&packed);
    nacre_dungeon_placements_release(&single);
    nacre_dungeon_visibility_release(&visibility);
    nacre_dungeon_generator_release(&map);
    ++valid;
  }
  assert(valid >= 450u);
  assert(merged > 0u); /* 2 m optimization is still being used. */
  printf("Wall visibility packing: %u real layouts, %u valid 2 m walls.\n",
      valid, merged);
}

int main(void)
{
  horizontal_different_rooms_must_not_merge();
  horizontal_same_room_must_merge();
  horizontal_shared_boundary_must_merge();
  vertical_different_rooms_must_not_merge();
  vertical_same_room_must_merge();
  same_region_set_in_reversed_order_must_merge();
  generated_maps_must_preserve_edges_and_owner_sets();
  puts("Nacre wall region merge tests passed.");
  return 0;
}
