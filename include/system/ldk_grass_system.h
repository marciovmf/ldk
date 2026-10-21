/**
 * # Grass system
 *
 * Persistent procedural grass patches.
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

/** @group ldk.system.grass_system */
#define LDK_GRASS_PROFILE_POINT_COUNT 4u
/** @group ldk.system.grass_system */
#define LDK_GRASS_MIN_SIDE_COUNT 3u
/** @group ldk.system.grass_system */
#define LDK_GRASS_MAX_SIDE_COUNT 8u
/** @group ldk.system.grass_system */
#define LDK_GRASS_TYPE_COUNT 16u
/** @group ldk.system.grass_system */
#define LDK_GRASS_TYPE_ID_INVALID UINT32_MAX

/** @group ldk.system.grass_system */
typedef u32 LDKGrassTypeId;

/** @group ldk.system.grass_system */
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

/** @group ldk.system.grass_system */
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
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassProfilePoint
{
  float height;
  float width;
} LDKGrassProfilePoint;

/** Shape shared by every blade in one visually compatible render batch.
 * @group ldk.system.grass_system
 */
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
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPlane
{
  Vec3 origin;
  Vec3 axis_u;
  Vec3 axis_v;
} LDKGrassPlane;

/** Runtime description used when registering an additional grass type.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassTypeDesc
{
  XSmallstr name;
  LDKGrassBladePreset blade_preset;
  float density;
  /** Tendency to grow near other blades: 0 = uniform, 1 = clustered. */
  float clumping;
  /** Maximum distance from a clump root, in world units. */
  float clump_radius;
  float min_height;
  float max_height;
  float blade_width;
  float curvature;
  /** Per-type maximum wind displacement before the system wind force. */
  float wind_strength;
  /** Seconds until interaction deformation fully returns to rest. Zero uses the default. */
  float interaction_recovery_time;
  rgba32 bottom_color;
  rgba32 top_color;
  bool use_noise_color;
} LDKGrassTypeDesc;

/** Per-region state. Visual configuration comes from LDKGrassSystem.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPatchDesc
{
  LDKGrassPlane plane;
  float density_scale;
  u32 seed;
} LDKGrassPatchDesc;

/** Opaque identity of one patch. Individual blades are intentionally hidden.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPatch
{
  u32 index;
  u32 version;
} LDKGrassPatch;

/**
 * @brief Initialize the grass system.
 * @param data Source data or system-owned context for this operation.
 * @return System initialization status code.
 * @group ldk.system.grass_system
 */
int ldk_grass_system_initialize(void *data);
/**
 * @brief Update the grass system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group ldk.system.grass_system
 */
void ldk_grass_system_update(
    void *data, const LDKEntityGroup *group, float dt);
/**
 * @brief Shut down the grass system.
 * @param data Source data or system-owned context for this operation.
 * @group ldk.system.grass_system
 */
void ldk_grass_system_terminate(void *data);

/** @group ldk.system.grass_system */
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

  //@begin_group "Color noise"
  /** Grayscale noise image file used for world-space color sampling. */
  XSmallstr color_noise_path;
  /** Noise repetitions per world unit. Must be positive. */
  //@inspect min=0.0001
  float color_noise_scale;
  /** Color stops, mapped to the noise [0, 1] interval. */
  //@inspect slider min=0 max=1
  float color_stop_1_position;
  //@inspect slider min=0 max=1
  float color_stop_2_position;
  //@inspect widget=COLOR
  u32 color_stop_0_color;
  //@inspect widget=COLOR
  u32 color_stop_1_color;
  //@inspect widget=COLOR
  u32 color_stop_2_color;
  //@inspect widget=COLOR
  u32 color_stop_3_color;
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
  //@inspect slider min=0 max=1
  float type_0_clumping;
  //@inspect slider min=0 max=3
  float type_0_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_0_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_0_bottom_color;
  //@inspect widget=COLOR
  u32 type_0_top_color;
  bool type_0_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 1"
  XSmallstr type_1_name;
  LDKGrassBladePreset type_1_blade_preset;
  //@inspect min=0
  float type_1_density;
  //@inspect slider min=0 max=1
  float type_1_clumping;
  //@inspect slider min=0 max=3
  float type_1_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_1_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_1_bottom_color;
  //@inspect widget=COLOR
  u32 type_1_top_color;
  bool type_1_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 2"
  XSmallstr type_2_name;
  LDKGrassBladePreset type_2_blade_preset;
  //@inspect min=0
  float type_2_density;
  //@inspect slider min=0 max=1
  float type_2_clumping;
  //@inspect slider min=0 max=3
  float type_2_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_2_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_2_bottom_color;
  //@inspect widget=COLOR
  u32 type_2_top_color;
  bool type_2_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 3"
  XSmallstr type_3_name;
  LDKGrassBladePreset type_3_blade_preset;
  //@inspect min=0
  float type_3_density;
  //@inspect slider min=0 max=1
  float type_3_clumping;
  //@inspect slider min=0 max=3
  float type_3_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_3_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_3_bottom_color;
  //@inspect widget=COLOR
  u32 type_3_top_color;
  bool type_3_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 4"
  XSmallstr type_4_name;
  LDKGrassBladePreset type_4_blade_preset;
  //@inspect min=0
  float type_4_density;
  //@inspect slider min=0 max=1
  float type_4_clumping;
  //@inspect slider min=0 max=3
  float type_4_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_4_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_4_bottom_color;
  //@inspect widget=COLOR
  u32 type_4_top_color;
  bool type_4_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 5"
  XSmallstr type_5_name;
  LDKGrassBladePreset type_5_blade_preset;
  //@inspect min=0
  float type_5_density;
  //@inspect slider min=0 max=1
  float type_5_clumping;
  //@inspect slider min=0 max=3
  float type_5_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_5_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_5_bottom_color;
  //@inspect widget=COLOR
  u32 type_5_top_color;
  bool type_5_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 6"
  XSmallstr type_6_name;
  LDKGrassBladePreset type_6_blade_preset;
  //@inspect min=0
  float type_6_density;
  //@inspect slider min=0 max=1
  float type_6_clumping;
  //@inspect slider min=0 max=3
  float type_6_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_6_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_6_bottom_color;
  //@inspect widget=COLOR
  u32 type_6_top_color;
  bool type_6_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 7"
  XSmallstr type_7_name;
  LDKGrassBladePreset type_7_blade_preset;
  //@inspect min=0
  float type_7_density;
  //@inspect slider min=0 max=1
  float type_7_clumping;
  //@inspect slider min=0 max=3
  float type_7_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_7_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_7_bottom_color;
  //@inspect widget=COLOR
  u32 type_7_top_color;
  bool type_7_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 8"
  XSmallstr type_8_name;
  LDKGrassBladePreset type_8_blade_preset;
  //@inspect min=0
  float type_8_density;
  //@inspect slider min=0 max=1
  float type_8_clumping;
  //@inspect slider min=0 max=3
  float type_8_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_8_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_8_bottom_color;
  //@inspect widget=COLOR
  u32 type_8_top_color;
  bool type_8_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 9"
  XSmallstr type_9_name;
  LDKGrassBladePreset type_9_blade_preset;
  //@inspect min=0
  float type_9_density;
  //@inspect slider min=0 max=1
  float type_9_clumping;
  //@inspect slider min=0 max=3
  float type_9_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_9_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_9_bottom_color;
  //@inspect widget=COLOR
  u32 type_9_top_color;
  bool type_9_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 10"
  XSmallstr type_10_name;
  LDKGrassBladePreset type_10_blade_preset;
  //@inspect min=0
  float type_10_density;
  //@inspect slider min=0 max=1
  float type_10_clumping;
  //@inspect slider min=0 max=3
  float type_10_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_10_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_10_bottom_color;
  //@inspect widget=COLOR
  u32 type_10_top_color;
  bool type_10_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 11"
  XSmallstr type_11_name;
  LDKGrassBladePreset type_11_blade_preset;
  //@inspect min=0
  float type_11_density;
  //@inspect slider min=0 max=1
  float type_11_clumping;
  //@inspect slider min=0 max=3
  float type_11_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_11_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_11_bottom_color;
  //@inspect widget=COLOR
  u32 type_11_top_color;
  bool type_11_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 12"
  XSmallstr type_12_name;
  LDKGrassBladePreset type_12_blade_preset;
  //@inspect min=0
  float type_12_density;
  //@inspect slider min=0 max=1
  float type_12_clumping;
  //@inspect slider min=0 max=3
  float type_12_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_12_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_12_bottom_color;
  //@inspect widget=COLOR
  u32 type_12_top_color;
  bool type_12_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 13"
  XSmallstr type_13_name;
  LDKGrassBladePreset type_13_blade_preset;
  //@inspect min=0
  float type_13_density;
  //@inspect slider min=0 max=1
  float type_13_clumping;
  //@inspect slider min=0 max=3
  float type_13_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_13_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_13_bottom_color;
  //@inspect widget=COLOR
  u32 type_13_top_color;
  bool type_13_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 14"
  XSmallstr type_14_name;
  LDKGrassBladePreset type_14_blade_preset;
  //@inspect min=0
  float type_14_density;
  //@inspect slider min=0 max=1
  float type_14_clumping;
  //@inspect slider min=0 max=3
  float type_14_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_14_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_14_bottom_color;
  //@inspect widget=COLOR
  u32 type_14_top_color;
  bool type_14_use_noise_color;
  //@end_group

  //@begin_group "Grass Type 15"
  XSmallstr type_15_name;
  LDKGrassBladePreset type_15_blade_preset;
  //@inspect min=0
  float type_15_density;
  //@inspect slider min=0 max=1
  float type_15_clumping;
  //@inspect slider min=0 max=3
  float type_15_clump_radius;
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
  //@inspect slider min=0 max=16
  float type_15_interaction_recovery_time;
  //@inspect widget=COLOR
  u32 type_15_bottom_color;
  //@inspect widget=COLOR
  u32 type_15_top_color;
  bool type_15_use_noise_color;
  //@end_group

} LDKGrassSystem;

/** Initialize a useful default grass type description.
 * @param out_desc Receives the descriptor.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_type_desc_defaults(LDKGrassTypeDesc *out_desc);

/** Return the active configuration index for name, or INVALID when absent.
 * @param name Name identifying the object or setting.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassTypeId ldk_grass_get_id_by_name(const char *name);

/** True when type_id names a configured slot in the active GrassSystem.
 * @param type_id Identifier of the type.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_type_is_valid(LDKGrassTypeId type_id);

/** Copy one configured type from the active GrassSystem.
 * @param type_id Identifier of the type.
 * @param out_desc Receives the descriptor.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_type_get(
    LDKGrassTypeId type_id, LDKGrassTypeDesc *out_desc);

/**
 * Register an additional type in the first free runtime slot. Names must be
 * unique. Returns INVALID when the system is inactive, the description is
 * invalid, the name already exists, or all 16 slots are occupied.
 * @param desc Description or configuration for this operation.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassTypeId ldk_grass_type_register(
    const LDKGrassTypeDesc *desc);

/** Initialize an empty one-square-unit patch with density scale 1.
 * @param out_desc Receives the descriptor.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc);

/** Return an invalid grass patch handle.
 * @return An invalid or empty handle.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassPatch ldk_grass_patch_null(void);

/** True while patch identifies a live entry in the active GrassSystem.
 * @param patch Patch used by this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_patch_is_valid(LDKGrassPatch patch);

/** Generate and retain one patch using a configured grass type.
 * @param type_id Identifier of the type.
 * @param desc Description or configuration for this operation.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassPatch ldk_grass_add(
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Replace a patch recipe and regenerate its persistent blade transforms.
 * @param patch Patch used by this operation.
 * @param type_id Identifier of the type.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_update(LDKGrassPatch patch,
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Remove a patch and every opaque blade belonging to it.
 * @param patch Patch used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_remove(LDKGrassPatch patch);

/** Remove every live patch. Renderer-side style resources remain cached.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_clear(void);

/**
 * Queue one world-space grass interaction capsule for the active GrassSystem.
 * Passing the same point for from and to produces a circular interaction.
 * The queued interaction is consumed by the GrassSystem render update.
 * @param from World-space start of the interaction segment.
 * @param to World-space end of the interaction segment.
 * @param radius Interaction or geometry radius.
 * @param strength Strength of the interaction.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_interact(
    Vec3 from, Vec3 to, float radius, float strength);

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_H
