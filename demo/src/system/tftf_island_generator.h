#ifndef TFTF_ISLAND_GENERATOR_H
#define TFTF_ISLAND_GENERATOR_H

#include <stdbool.h>
#include <stdint.h>

/* Standalone C11 island generation API. No LDK or STDX headers. */
#define TFTF_ISLAND_GENERATOR_VERSION 15u

typedef enum TFTFIslandGeneratorSurface
{
  TFTF_ISLAND_GENERATOR_SURFACE_DEEP_WATER = 0,
  TFTF_ISLAND_GENERATOR_SURFACE_SHALLOW_WATER,
  TFTF_ISLAND_GENERATOR_SURFACE_SAND,
  TFTF_ISLAND_GENERATOR_SURFACE_GRASS,
  TFTF_ISLAND_GENERATOR_SURFACE_DRY,
  TFTF_ISLAND_GENERATOR_SURFACE_FOREST,
  TFTF_ISLAND_GENERATOR_SURFACE_TALL_GRASS,
  TFTF_ISLAND_GENERATOR_SURFACE_THORNS,
  TFTF_ISLAND_GENERATOR_SURFACE_ROCK,
  TFTF_ISLAND_GENERATOR_SURFACE_COUNT
} TFTFIslandGeneratorSurface;

typedef struct TFTFIslandGeneratorConfig
{
  uint32_t seed;
  uint32_t map_size; /* World map width and height in cells */
  float cell_size_meters;
  float land_percent; /* Fraction of the original 2x2 world area, 1..80 */
  float min_satellite_gap_tiles;
  float max_satellite_gap_tiles;
} TFTFIslandGeneratorConfig;

typedef struct TFTFIslandGeneratorCell
{
  float height; /* Signed elevation, in generator world units: sea=0 */
  float density; /* Primary vegetation density 0..1, generated from noise */
  uint8_t surface; /* TFTFIslandGeneratorSurface */
  uint8_t coast; /* 0 none, 1 beach, 2 cliff, 3 overgrown */
  uint8_t land_id; /* 0 sea, 1 main island, 2-3 satellites */
  uint8_t reserved;
} TFTFIslandGeneratorCell;

typedef struct TFTFIslandGeneratorMap
{
  TFTFIslandGeneratorCell *cells; /* Caller releases with release(). */
  uint32_t width;
  uint32_t height;
  uint32_t seed;
  uint32_t satellite_count;
  uint32_t formation_count;
  uint32_t unreachable_land_cells; /* Inaccessible on foot, at internal 600x600 */
  float meters_per_world_unit;
  float sampled_world_span;
} TFTFIslandGeneratorMap;

void tftf_island_generator_config_defaults(TFTFIslandGeneratorConfig *config);
bool tftf_island_generator_generate(const TFTFIslandGeneratorConfig *config,
    TFTFIslandGeneratorMap *out);
void tftf_island_generator_release(TFTFIslandGeneratorMap *map);

#endif /* TFTF_ISLAND_GENERATOR_H */
