/* Standalone diagnostic client; the algorithm lives exclusively in the library. */
#include "tftf_island_generator.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int write_ppm(const char *path, const TFTFIslandGeneratorMap *map,
    int mode)
{
  static const uint8_t colors[9][3] = {
    {29,79,121}, {83,169,173}, {224,201,141},
    {137,165,101}, {184,158,113}, {58,113,75},
    {92,145,73}, {154,90,86}, {120,115,112}
  };
  FILE *f = fopen(path, "wb");
  if (!f) return 0;
  int ok = fprintf(f, "P6\n%u %u\n255\n", map->width, map->height) > 0;
  for (uint32_t y = 0; y < map->height && ok; ++y)
  {
    for (uint32_t x = 0; x < map->width && ok; ++x)
    {
      const TFTFIslandGeneratorCell *c = &map->cells[(size_t)y * map->width + x];
      uint8_t rgb[3];
      if (mode == 0)
        memcpy(rgb, colors[c->surface < 9u ? c->surface : 0], 3);
      else if (mode == 1)
      {
        const float grey = fmaxf(0.f, fminf(255.f, 128.f + c->height * 600.f));
        rgb[0] = rgb[1] = rgb[2] = (uint8_t)grey;
      }
      else
      {
        const uint8_t d = (uint8_t)(255.f * c->density);
        rgb[0] = rgb[1] = rgb[2] = d;
      }
      if (fwrite(rgb, 1, 3, f) != 3u) ok = 0;
    }
  }
  if (fclose(f)) ok = 0;
  if (!ok) remove(path);
  return ok;
}

int main(int argc, char **argv)
{
  TFTFIslandGeneratorConfig cfg;
  tftf_island_generator_config_defaults(&cfg);
  const char *name = argc > 2 ? argv[2] : "tftf_island";
  if (argc > 1) cfg.seed = (uint32_t)strtoul(argv[1], NULL, 10);
  if (argc > 3) cfg.map_size = (uint32_t)strtoul(argv[3], NULL, 10);
  TFTFIslandGeneratorMap map = {0};
  if (!tftf_island_generator_generate(&cfg, &map))
  {
    fprintf(stderr, "Generation failed (seed %u).\n", cfg.seed);
    return 1;
  }
  const char *suffix[] = {"biomes", "height", "density"};
  int ok = 1;
  for (int i = 0; i < 3; ++i)
  {
    char path[1024];
    int n = snprintf(path, sizeof(path), "%s_%s.ppm", name, suffix[i]);
    if (n <= 0 || (size_t)n >= sizeof(path) || !write_ppm(path, &map, i)) ok = 0;
  }
  printf("seed=%u size=%u satellites=%u escarpments=%u unreachable=%u "
      "meters_per_world_unit=%.2f\n", cfg.seed, cfg.map_size,
      map.satellite_count, map.formation_count, map.unreachable_land_cells,
      map.meters_per_world_unit);
  tftf_island_generator_release(&map);
  return ok ? 0 : 1;
}
