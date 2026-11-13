#ifndef NACRE_DUNGEON_PROGRESSION_H
#define NACRE_DUNGEON_PROGRESSION_H
#include "nacre_dungeon_generator.h"

/* Pure C progression stage. The map is modified only while being generated;
 * in play, the dungeon system owns mutable door/gate/totem/objective state. */
bool nacre_dungeon_progression_generate(const NacreDungeonConfig *cfg,
                                        NacreDungeonMap *map);
bool nacre_dungeon_progression_validate(const NacreDungeonMap *map);
void nacre_dungeon_progression_release(NacreDungeonMap *map);

/* Progression-only reachability. Cracked edges can be broken with one attack,
 * hidden entrances NEVER count, doors are freely openable, and a portcullis
 * only counts when its opened flag is true. */
bool nacre_dungeon_progression_can_step(const NacreDungeonMap *map,
                                       uint32_t from, uint32_t to,
                                       bool assume_cracked_broken,
                                       bool assume_all_gates_open);
bool nacre_dungeon_progression_can_exit(const NacreDungeonMap *map);
#endif
