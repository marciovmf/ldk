#ifndef NACRE_DUNGEON_FEATURES_H
#define NACRE_DUNGEON_FEATURES_H
#include "nacre_dungeon_generator.h"
/* Post-geometry features. The caller passes the main generator PRNG state
 * so that secret-room sampling follows the same stream as the v21 lab.
 * Cave and pillar distributions use independent, suffix-derived streams. */
bool nacre_dungeon_features_generate(const NacreDungeonConfig *config,
                                    NacreDungeonMap *map, uint32_t *rng_state);
bool nacre_dungeon_features_place_pillars(const NacreDungeonConfig *config,
                                          NacreDungeonMap *map);
void nacre_dungeon_features_release(NacreDungeonMap *map);
#endif
