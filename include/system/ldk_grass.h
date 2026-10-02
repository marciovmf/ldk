/**
 * @file ldk_grass.h
 * @brief Persistent procedural grass patches.
 */
#ifndef LDK_GRASS_H
#define LDK_GRASS_H

#include <ldk_common.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDK_GRASS_PROFILE_POINT_COUNT 4u
#define LDK_GRASS_MIN_SIDE_COUNT 3u
#define LDK_GRASS_MAX_SIDE_COUNT 8u
#define LDK_GRASS_TYPE_COUNT 16u
#define LDK_GRASS_TYPE_ID_INVALID UINT32_MAX

typedef u32 LDKGrassTypeId;

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

//@enum
typedef enum LDKGrassBladePreset
{
  LDK_GRASS_BLADE_PRESET_THIN = 0,
  LDK_GRASS_BLADE_PRESET_CROSSED,
  LDK_GRASS_BLADE_PRESET_TUFT,
  LDK_GRASS_BLADE_PRESET_FLAT_TOP,
  LDK_GRASS_BLADE_PRESET_THORNY
} LDKGrassBladePreset;

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

/** Runtime description used when registering an additional grass type. */
typedef struct LDKGrassTypeDesc
{
  XSmallstr name;
  LDKGrassBladePreset blade_preset;
  float density;
  float min_height;
  float max_height;
  float blade_width;
  float curvature;
  /** Per-type maximum wind displacement before the system wind force. */
  float wind_strength;
  rgba32 bottom_color;
  rgba32 top_color;
} LDKGrassTypeDesc;

/** Per-region state. Visual configuration comes from LDKGrassSystem. */
typedef struct LDKGrassPatchDesc
{
  LDKGrassPlane plane;
  float density_scale;
  u32 seed;
} LDKGrassPatchDesc;

/** Opaque identity of one patch. Individual blades are intentionally hidden. */
typedef struct LDKGrassPatch
{
  u32 index;
  u32 version;
} LDKGrassPatch;

int ldk_grass_system_initialize(void *data);
void ldk_grass_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void ldk_grass_system_terminate(void *data);

//@system name=GrassSystem initialize=ldk_grass_system_initialize update=ldk_grass_system_update terminate=ldk_grass_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
typedef struct LDKGrassSystem
{
  //@begin_group "Blade"
  //@inspect slider min=0 max=89
  float min_tilt_degrees;
  //@inspect slider min=0 max=89
  float max_tilt_degrees;
  /** Shared lean azimuth: 0 degrees is +X, 90 degrees is +Z. */
  //@inspect slider min=0 max=360
  float tilt_direction_degrees;
  //@end_group

  //@begin_group "Wind"
  /** Wind azimuth: 0 degrees is +X, 90 degrees is +Z. */
  //@inspect slider min=0 max=360
  float wind_direction_degrees;
  //@inspect slider min=0 max=4
  float wind_force;
  /** Animation cycles per second. */
  //@inspect slider min=0 max=5
  float wind_speed;
  //@end_group

  //@begin_group "Material"
  //@inspect slider min=0 max=1
  float specular;
  //@inspect slider min=1 max=256
  float shininess;
  //@inspect slider min=0 max=4
  float emission;
  bool casts_shadows;
  //@end_group

  //@begin_group "Grass Type 0"
  XSmallstr type_0_name;
  LDKGrassBladePreset type_0_blade_preset;
  //@inspect min=0
  float type_0_density;
  //@inspect min=0
  float type_0_min_height;
  //@inspect min=0
  float type_0_max_height;
  //@inspect min=0
  float type_0_blade_width;
  //@inspect slider min=0 max=0.5
  float type_0_curvature;
  //@inspect slider min=0 max=0.5
  float type_0_wind_strength;
  //@inspect widget=COLOR
  u32 type_0_bottom_color;
  //@inspect widget=COLOR
  u32 type_0_top_color;
  //@end_group

  //@begin_group "Grass Type 1"
  XSmallstr type_1_name;
  LDKGrassBladePreset type_1_blade_preset;
  //@inspect min=0
  float type_1_density;
  //@inspect min=0
  float type_1_min_height;
  //@inspect min=0
  float type_1_max_height;
  //@inspect min=0
  float type_1_blade_width;
  //@inspect slider min=0 max=0.5
  float type_1_curvature;
  //@inspect slider min=0 max=0.5
  float type_1_wind_strength;
  //@inspect widget=COLOR
  u32 type_1_bottom_color;
  //@inspect widget=COLOR
  u32 type_1_top_color;
  //@end_group

  //@begin_group "Grass Type 2"
  XSmallstr type_2_name;
  LDKGrassBladePreset type_2_blade_preset;
  //@inspect min=0
  float type_2_density;
  //@inspect min=0
  float type_2_min_height;
  //@inspect min=0
  float type_2_max_height;
  //@inspect min=0
  float type_2_blade_width;
  //@inspect slider min=0 max=0.5
  float type_2_curvature;
  //@inspect slider min=0 max=0.5
  float type_2_wind_strength;
  //@inspect widget=COLOR
  u32 type_2_bottom_color;
  //@inspect widget=COLOR
  u32 type_2_top_color;
  //@end_group

  //@begin_group "Grass Type 3"
  XSmallstr type_3_name;
  LDKGrassBladePreset type_3_blade_preset;
  //@inspect min=0
  float type_3_density;
  //@inspect min=0
  float type_3_min_height;
  //@inspect min=0
  float type_3_max_height;
  //@inspect min=0
  float type_3_blade_width;
  //@inspect slider min=0 max=0.5
  float type_3_curvature;
  //@inspect slider min=0 max=0.5
  float type_3_wind_strength;
  //@inspect widget=COLOR
  u32 type_3_bottom_color;
  //@inspect widget=COLOR
  u32 type_3_top_color;
  //@end_group

  //@begin_group "Grass Type 4"
  XSmallstr type_4_name;
  LDKGrassBladePreset type_4_blade_preset;
  //@inspect min=0
  float type_4_density;
  //@inspect min=0
  float type_4_min_height;
  //@inspect min=0
  float type_4_max_height;
  //@inspect min=0
  float type_4_blade_width;
  //@inspect slider min=0 max=0.5
  float type_4_curvature;
  //@inspect slider min=0 max=0.5
  float type_4_wind_strength;
  //@inspect widget=COLOR
  u32 type_4_bottom_color;
  //@inspect widget=COLOR
  u32 type_4_top_color;
  //@end_group

  //@begin_group "Grass Type 5"
  XSmallstr type_5_name;
  LDKGrassBladePreset type_5_blade_preset;
  //@inspect min=0
  float type_5_density;
  //@inspect min=0
  float type_5_min_height;
  //@inspect min=0
  float type_5_max_height;
  //@inspect min=0
  float type_5_blade_width;
  //@inspect slider min=0 max=0.5
  float type_5_curvature;
  //@inspect slider min=0 max=0.5
  float type_5_wind_strength;
  //@inspect widget=COLOR
  u32 type_5_bottom_color;
  //@inspect widget=COLOR
  u32 type_5_top_color;
  //@end_group

  //@begin_group "Grass Type 6"
  XSmallstr type_6_name;
  LDKGrassBladePreset type_6_blade_preset;
  //@inspect min=0
  float type_6_density;
  //@inspect min=0
  float type_6_min_height;
  //@inspect min=0
  float type_6_max_height;
  //@inspect min=0
  float type_6_blade_width;
  //@inspect slider min=0 max=0.5
  float type_6_curvature;
  //@inspect slider min=0 max=0.5
  float type_6_wind_strength;
  //@inspect widget=COLOR
  u32 type_6_bottom_color;
  //@inspect widget=COLOR
  u32 type_6_top_color;
  //@end_group

  //@begin_group "Grass Type 7"
  XSmallstr type_7_name;
  LDKGrassBladePreset type_7_blade_preset;
  //@inspect min=0
  float type_7_density;
  //@inspect min=0
  float type_7_min_height;
  //@inspect min=0
  float type_7_max_height;
  //@inspect min=0
  float type_7_blade_width;
  //@inspect slider min=0 max=0.5
  float type_7_curvature;
  //@inspect slider min=0 max=0.5
  float type_7_wind_strength;
  //@inspect widget=COLOR
  u32 type_7_bottom_color;
  //@inspect widget=COLOR
  u32 type_7_top_color;
  //@end_group

  //@begin_group "Grass Type 8"
  XSmallstr type_8_name;
  LDKGrassBladePreset type_8_blade_preset;
  //@inspect min=0
  float type_8_density;
  //@inspect min=0
  float type_8_min_height;
  //@inspect min=0
  float type_8_max_height;
  //@inspect min=0
  float type_8_blade_width;
  //@inspect slider min=0 max=0.5
  float type_8_curvature;
  //@inspect slider min=0 max=0.5
  float type_8_wind_strength;
  //@inspect widget=COLOR
  u32 type_8_bottom_color;
  //@inspect widget=COLOR
  u32 type_8_top_color;
  //@end_group

  //@begin_group "Grass Type 9"
  XSmallstr type_9_name;
  LDKGrassBladePreset type_9_blade_preset;
  //@inspect min=0
  float type_9_density;
  //@inspect min=0
  float type_9_min_height;
  //@inspect min=0
  float type_9_max_height;
  //@inspect min=0
  float type_9_blade_width;
  //@inspect slider min=0 max=0.5
  float type_9_curvature;
  //@inspect slider min=0 max=0.5
  float type_9_wind_strength;
  //@inspect widget=COLOR
  u32 type_9_bottom_color;
  //@inspect widget=COLOR
  u32 type_9_top_color;
  //@end_group

  //@begin_group "Grass Type 10"
  XSmallstr type_10_name;
  LDKGrassBladePreset type_10_blade_preset;
  //@inspect min=0
  float type_10_density;
  //@inspect min=0
  float type_10_min_height;
  //@inspect min=0
  float type_10_max_height;
  //@inspect min=0
  float type_10_blade_width;
  //@inspect slider min=0 max=0.5
  float type_10_curvature;
  //@inspect slider min=0 max=0.5
  float type_10_wind_strength;
  //@inspect widget=COLOR
  u32 type_10_bottom_color;
  //@inspect widget=COLOR
  u32 type_10_top_color;
  //@end_group

  //@begin_group "Grass Type 11"
  XSmallstr type_11_name;
  LDKGrassBladePreset type_11_blade_preset;
  //@inspect min=0
  float type_11_density;
  //@inspect min=0
  float type_11_min_height;
  //@inspect min=0
  float type_11_max_height;
  //@inspect min=0
  float type_11_blade_width;
  //@inspect slider min=0 max=0.5
  float type_11_curvature;
  //@inspect slider min=0 max=0.5
  float type_11_wind_strength;
  //@inspect widget=COLOR
  u32 type_11_bottom_color;
  //@inspect widget=COLOR
  u32 type_11_top_color;
  //@end_group

  //@begin_group "Grass Type 12"
  XSmallstr type_12_name;
  LDKGrassBladePreset type_12_blade_preset;
  //@inspect min=0
  float type_12_density;
  //@inspect min=0
  float type_12_min_height;
  //@inspect min=0
  float type_12_max_height;
  //@inspect min=0
  float type_12_blade_width;
  //@inspect slider min=0 max=0.5
  float type_12_curvature;
  //@inspect slider min=0 max=0.5
  float type_12_wind_strength;
  //@inspect widget=COLOR
  u32 type_12_bottom_color;
  //@inspect widget=COLOR
  u32 type_12_top_color;
  //@end_group

  //@begin_group "Grass Type 13"
  XSmallstr type_13_name;
  LDKGrassBladePreset type_13_blade_preset;
  //@inspect min=0
  float type_13_density;
  //@inspect min=0
  float type_13_min_height;
  //@inspect min=0
  float type_13_max_height;
  //@inspect min=0
  float type_13_blade_width;
  //@inspect slider min=0 max=0.5
  float type_13_curvature;
  //@inspect slider min=0 max=0.5
  float type_13_wind_strength;
  //@inspect widget=COLOR
  u32 type_13_bottom_color;
  //@inspect widget=COLOR
  u32 type_13_top_color;
  //@end_group

  //@begin_group "Grass Type 14"
  XSmallstr type_14_name;
  LDKGrassBladePreset type_14_blade_preset;
  //@inspect min=0
  float type_14_density;
  //@inspect min=0
  float type_14_min_height;
  //@inspect min=0
  float type_14_max_height;
  //@inspect min=0
  float type_14_blade_width;
  //@inspect slider min=0 max=0.5
  float type_14_curvature;
  //@inspect slider min=0 max=0.5
  float type_14_wind_strength;
  //@inspect widget=COLOR
  u32 type_14_bottom_color;
  //@inspect widget=COLOR
  u32 type_14_top_color;
  //@end_group

  //@begin_group "Grass Type 15"
  XSmallstr type_15_name;
  LDKGrassBladePreset type_15_blade_preset;
  //@inspect min=0
  float type_15_density;
  //@inspect min=0
  float type_15_min_height;
  //@inspect min=0
  float type_15_max_height;
  //@inspect min=0
  float type_15_blade_width;
  //@inspect slider min=0 max=0.5
  float type_15_curvature;
  //@inspect slider min=0 max=0.5
  float type_15_wind_strength;
  //@inspect widget=COLOR
  u32 type_15_bottom_color;
  //@inspect widget=COLOR
  u32 type_15_top_color;
  //@end_group

} LDKGrassSystem;

/** Initialize a useful default grass type description. */
LDK_API void ldk_grass_type_desc_defaults(LDKGrassTypeDesc *out_desc);

/** Return the active configuration index for name, or INVALID when absent. */
LDK_API LDKGrassTypeId ldk_grass_get_id_by_name(const char *name);

/** True when type_id names a configured slot in the active GrassSystem. */
LDK_API bool ldk_grass_type_is_valid(LDKGrassTypeId type_id);

/** Copy one configured type from the active GrassSystem. */
LDK_API bool ldk_grass_type_get(
    LDKGrassTypeId type_id, LDKGrassTypeDesc *out_desc);

/**
 * Register an additional type in the first free runtime slot. Names must be
 * unique. Returns INVALID when the system is inactive, the description is
 * invalid, the name already exists, or all 16 slots are occupied.
 */
LDK_API LDKGrassTypeId ldk_grass_type_register(
    const LDKGrassTypeDesc *desc);

/** Initialize an empty one-square-unit patch with density scale 1. */
LDK_API void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc);

/** Return an invalid grass patch handle. */
LDK_API LDKGrassPatch ldk_grass_patch_null(void);

/** True while patch identifies a live entry in the active GrassSystem. */
LDK_API bool ldk_grass_patch_is_valid(LDKGrassPatch patch);

/** Generate and retain one patch using a configured grass type. */
LDK_API LDKGrassPatch ldk_grass_add(
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Replace a patch recipe and regenerate its persistent blade transforms. */
LDK_API bool ldk_grass_update(LDKGrassPatch patch,
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Remove a patch and every opaque blade belonging to it. */
LDK_API bool ldk_grass_remove(LDKGrassPatch patch);

/** Remove every live patch. Renderer-side style resources remain cached. */
LDK_API void ldk_grass_clear(void);

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_H
