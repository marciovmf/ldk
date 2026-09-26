/**
 * @file ldk_grass.h
 * @brief Persistent procedural grass patches.
 */
#ifndef LDK_GRASS_H
#define LDK_GRASS_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDK_GRASS_PROFILE_POINT_COUNT 4u
#define LDK_GRASS_MIN_SIDE_COUNT 3u
#define LDK_GRASS_MAX_SIDE_COUNT 8u

typedef enum LDKGrassBladeTopology
{
  LDK_GRASS_BLADE_TOPOLOGY_RIBBON = 0,
  LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS,
  LDK_GRASS_BLADE_TOPOLOGY_RADIAL,
  LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS,
  LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM,
  /** Deprecated source-compatible name for the original thorny prototype. */
  LDK_GRASS_BLADE_TOPOLOGY_THORNY_RIBBON =
      LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM
} LDKGrassBladeTopology;

/**
 * One sample of the normalized blade profile. Height is in [0, 1]. Width is
 * the full ribbon width, or the radial diameter, at that height.
 */
typedef struct LDKGrassProfilePoint
{
  float height;
  float width;
} LDKGrassProfilePoint;

/** Shape shared by every blade in one visually compatible render batch. */
typedef struct LDKGrassBladeShape
{
  LDKGrassProfilePoint profile[LDK_GRASS_PROFILE_POINT_COUNT];
  LDKGrassBladeTopology topology;
  /** Used by the radial and thorny stem topologies. */
  u32 side_count;
} LDKGrassBladeShape;

/**
 * Flat target geometry. The complete parallelogram, including its edges, is a
 * valid generation area. origin + axis_u + axis_v is the opposite corner.
 */
typedef struct LDKGrassPlane
{
  Vec3 origin;
  Vec3 axis_u;
  Vec3 axis_v;
} LDKGrassPlane;

/**
 * Persistent grass recipe. Density is blades per square world unit. Heights
 * and tilt are sampled once per blade and remain unchanged until the patch is
 * updated. Tilt values are angles in radians relative to the surface normal.
 * tilt_direction is the shared azimuth of every blade: zero leans toward +X.
 */
typedef struct LDKGrassPatchDesc
{
  LDKGrassPlane plane;
  LDKGrassBladeShape shape;
  float density;
  float min_height;
  float max_height;
  float min_tilt;
  float max_tilt;
  float tilt_direction;
  rgba32 bottom_color;
  rgba32 top_color;
  float specular;
  /** Phong exponent; zero selects the renderer default (32). */
  float shininess;
  float emission;
  /** Tip displacement as a fraction of blade height. */
  float curvature;
  /** Maximum animated tip displacement as a fraction of blade height. */
  float wind_strength;
  /** Animation cycles per second. */
  float wind_speed;
  u32 seed;
  bool casts_shadows;
} LDKGrassPatchDesc;

/** Opaque identity of one patch. Individual blades are intentionally hidden. */
typedef struct LDKGrassPatch
{
  u32 index;
  u32 version;
} LDKGrassPatch;

/** Initialize a useful four-level tapered blade and an empty flat patch. */
LDK_API void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc);

/** Return an invalid grass patch handle. */
LDK_API LDKGrassPatch ldk_grass_patch_null(void);

/** True while patch identifies a live entry in the grass system. */
LDK_API bool ldk_grass_patch_is_valid(LDKGrassPatch patch);

/**
 * Generate and retain one patch. Returns null when the description is invalid.
 */
LDK_API LDKGrassPatch ldk_grass_add(const LDKGrassPatchDesc *desc);

/** Replace a patch recipe and regenerate its persistent blade transforms. */
LDK_API bool ldk_grass_update(
    LDKGrassPatch patch, const LDKGrassPatchDesc *desc);

/** Remove a patch and every opaque blade belonging to it. */
LDK_API bool ldk_grass_remove(LDKGrassPatch patch);

/** Remove every live patch. Renderer-side style resources remain cached. */
LDK_API void ldk_grass_clear(void);

#ifdef LDK_ENGINE
typedef struct LDKRenderer LDKRenderer;

bool ldk_grass_system_initialize(LDKRenderer *renderer);
void ldk_grass_system_submit(void);
void ldk_grass_system_terminate(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_H
