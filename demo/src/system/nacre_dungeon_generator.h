#ifndef NACRE_DUNGEON_GENERATOR_H
#define NACRE_DUNGEON_GENERATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Nacre dungeon generator - geometry phase (LDK Layout Lab v21).
 * Self-contained C11. No dependency on the LDK renderer, editor or ECS.
 *
 * Layout geometry, hidden rooms, optional cave shortcuts and blocking pillars.
 * Peripheral treasure rooms, doors, gates, totems and exit objectives are
 * generated in a separate progression phase; physics/rendering remain outside.
 *
 * Coordinates: X increases right, Y increases down on the logical grid.
 * A cell's 3D center is (x + .5f, 0, y + .5f) * cell_size, relative to the
 * grid origin. Walls sit at the midpoint of the corresponding cell edge.
 */
#define NACRE_DUNGEON_GENERATOR_VERSION 1u
#define NACRE_DUNGEON_MAX_DIMENSION 100u
#define NACRE_DUNGEON_MAX_ROOMS 32u
#define NACRE_DUNGEON_MAX_SEED_LENGTH 127u

typedef enum NacreDungeonCellType {
  NACRE_DUNGEON_EMPTY = 0,
  NACRE_DUNGEON_ROOM = 1,
  NACRE_DUNGEON_CORRIDOR = 2,
  NACRE_DUNGEON_SECRET = 3,
  NACRE_DUNGEON_CAVE = 4
} NacreDungeonCellType;

typedef enum NacreDungeonEdgeType {
  NACRE_DUNGEON_EDGE_OPEN = 0,
  NACRE_DUNGEON_EDGE_HOLE,
  NACRE_DUNGEON_EDGE_CRACKED,
  NACRE_DUNGEON_EDGE_SECRET /* Closed: normal-looking hidden entrance. */
} NacreDungeonEdgeType;

typedef struct NacreDungeonConfig {
  char seed[NACRE_DUNGEON_MAX_SEED_LENGTH + 1]; /* UTF-8, NUL-terminated. */
  uint32_t width;
  uint32_t height;
  uint32_t rooms_min;
  uint32_t rooms_max;
  uint32_t max_room_size;
  uint32_t max_corridor_tiles;
  double extra_connections;       /* 0..0.75 */
  double irregular_rooms;         /* 0..0.60 */
  double adjacent_rooms;         /* 0..1 */
  double alternative_walls;      /* 0..1 of extra budget */
  double cracked_fraction;       /* 0..1 of special walls */
  uint32_t secret_rooms_max;     /* 0..3, weighted v21 count */
  uint32_t cave_tunnels_max;     /* 0..3, weighted v21 count */
  uint32_t pillar_count;         /* 0..18 desired, not guaranteed */
  double pillar_broken_fraction; /* 0..1 */
  /* 0: free exit; 1: keys; 2: levers. N objectives, 1..6. */
  uint32_t exit_requirement;
  uint32_t objective_count;
} NacreDungeonConfig;

typedef struct NacreDungeonRoom {
  uint32_t id;
  int32_t x, y, width, height;
  int32_t center_x, center_y;
  /* Cell indices of this room. Ownership belongs to NacreDungeonMap. */
  uint32_t *cells;
  uint32_t cell_count;
  /* Internal: geometry stage keeps original exits to reproduce scores. */
  bool peripheral_treasure;
  uint32_t *exits;
  uint32_t exit_count;
} NacreDungeonRoom;

typedef struct NacreDungeonEdge {
  uint32_t a, b;  /* Adjacent cells; map.edges canonical (a < b). */
  NacreDungeonEdgeType type;
} NacreDungeonEdge;

typedef struct NacreDungeonLink {
  uint32_t room_a, room_b;
  bool extra;
  bool direct;
  bool hole;
  bool normalized_micro_corridor;
  bool treasure;
  /* An ordinary adjacent link can contain two passages. */
  uint32_t passage_count;
  NacreDungeonEdge passages[2];
  uint32_t path_count;
  uint32_t path[5]; /* max_corridor_tiles <= 5 in v21. */
} NacreDungeonLink;

typedef struct NacreDungeonSecretRoom {
  uint32_t id;
  int32_t x, y, width, height;
  uint32_t *cells;
  uint32_t cell_count;
  uint32_t entrance_outside, entrance_inside;
} NacreDungeonSecretRoom;

typedef struct NacreDungeonCave {
  uint32_t room_a, room_b;
  uint32_t *path;
  uint32_t path_count;
  NacreDungeonEdge entrances[2];
  bool cross_branch;
} NacreDungeonCave;

typedef struct NacreDungeonPillar {
  uint32_t tile, room_id;
  bool broken;
} NacreDungeonPillar;

/* Gameplay state is deliberately stored on the map, not on a prefab. */
typedef enum NacreDungeonPassageKind {
  NACRE_DUNGEON_PASSAGE_OPEN = 0,
  NACRE_DUNGEON_PASSAGE_DOOR,
  NACRE_DUNGEON_PASSAGE_PORTCULLIS
} NacreDungeonPassageKind;

typedef enum NacreDungeonTotemColor {
  NACRE_DUNGEON_COLOR_RED = 0,
  NACRE_DUNGEON_COLOR_GREEN = 1,
  NACRE_DUNGEON_COLOR_BLUE = 2,
  NACRE_DUNGEON_COLOR_NONE = 255
} NacreDungeonTotemColor;

typedef enum NacreDungeonDirection {
  NACRE_DUNGEON_DIRECTION_UP = 0,
  NACRE_DUNGEON_DIRECTION_DOWN,
  NACRE_DUNGEON_DIRECTION_LEFT,
  NACRE_DUNGEON_DIRECTION_RIGHT
} NacreDungeonDirection;

typedef struct NacreDungeonPassage {
  uint32_t id, a, b; /* 1-based stable ID; a < b. */
  NacreDungeonPassageKind kind;
  bool opened;
  NacreDungeonTotemColor color;
  uint32_t totem_id; /* 0 for ordinary openings and doors. */
  int32_t treasure_room_id; /* -1 for non-treasure passages. */
} NacreDungeonPassage;

typedef struct NacreDungeonTreasure {
  uint32_t room_id, parent_room_id;
  uint32_t entrance_a, entrance_b, gate_id;
  NacreDungeonTotemColor color;
} NacreDungeonTreasure;

typedef struct NacreDungeonTotem {
  uint32_t id, room_id, tile;
  NacreDungeonTotemColor color;
  NacreDungeonDirection sequence[4];
  bool discovered;
} NacreDungeonTotem;

typedef struct NacreDungeonObjective {
  uint32_t id, room_id, tile;
  bool completed;
} NacreDungeonObjective;

typedef struct NacreDungeonMap {
  uint32_t width, height, seed_hash;
  uint32_t requested_room_count, requested_totem_count;
  uint32_t normalized_micro_corridors;
  uint32_t entry, exit, shortest_path_length;
  uint8_t *cells;       /* NacreDungeonCellType, width * height */
  int16_t *room_ids;    /* -1 outside rooms */
  NacreDungeonRoom *rooms;
  uint32_t room_count;
  NacreDungeonLink *links;
  uint32_t link_count;
  NacreDungeonEdge *edges;
  uint32_t edge_count;
  uint32_t *shortest_route;
  uint32_t shortest_route_count;
  NacreDungeonSecretRoom *secret_rooms;
  uint32_t secret_room_count, requested_secret_count;
  NacreDungeonCave *caves;
  uint32_t cave_count, requested_cave_count;
  NacreDungeonPillar *pillars;
  uint32_t pillar_count;
  uint8_t *pillar_mask;  /* width*height, blocking both intact and broken. */
  NacreDungeonPassage *passages;
  uint32_t passage_count;
  NacreDungeonTreasure *treasures;
  uint32_t treasure_count;
  NacreDungeonTotem *totems;
  uint32_t totem_count;
  NacreDungeonObjective *objectives;
  uint32_t objective_count;
  uint32_t exit_requirement;
  bool progression_valid;
} NacreDungeonMap;

typedef enum NacreDungeonResult {
  NACRE_DUNGEON_OK = 0,
  NACRE_DUNGEON_INVALID_CONFIG,
  NACRE_DUNGEON_OUT_OF_MEMORY,
  NACRE_DUNGEON_NO_VALID_LAYOUT,
  NACRE_DUNGEON_INVALID_LAYOUT
} NacreDungeonResult;

void nacre_dungeon_generator_config_defaults(NacreDungeonConfig *config);
/* Returns NULL for valid settings, otherwise a constant diagnostic message.
 * Uses the same rules as nacre_dungeon_generator_generate(). */
const char *nacre_dungeon_generator_config_error(const NacreDungeonConfig *config);
/* One generation attempt, exactly as v21's generateOnce() does.
 * On a NO_VALID_LAYOUT, caller can choose another seed or retry at runtime.
 * Does NOT silently change the seed, attempt count, or selected room count.
 * On failure, out_map is empty and may be passed to release().
 */
NacreDungeonResult nacre_dungeon_generator_generate(
    const NacreDungeonConfig *config, NacreDungeonMap *out_map);
void nacre_dungeon_generator_release(NacreDungeonMap *map);
/* Independent integrity checks for the implemented geometry subset. */
bool nacre_dungeon_generator_validate(const NacreDungeonMap *map);

#endif
