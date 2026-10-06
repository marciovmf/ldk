/**
 * @file ldk_water_system.h
 * @brief Configurable horizontal water with shader-driven waves and foam.
 */
#ifndef LDK_WATER_SYSTEM_H
#define LDK_WATER_SYSTEM_H

#include <ldk_asset.h>
#include <ldk_color.h>
#include <ldk_common.h>
#include <module/ldk_system.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // clang-format off
  //@system name=WaterSystem initialize=ldk_water_system_initialize update=ldk_water_system_update terminate=ldk_water_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
  typedef struct LDKWaterSystem
  {
    // clang-format on
    //@begin_group "Surface"
    float water_level;
    float center_x;
    float center_z;
    //@inspect min=0.01
    float width;
    //@inspect min=0.01
    float depth;
    //@inspect min=0.01
    float cell_size;
    //@inspect min=1 max=128
    u32 chunk_size;
    //@end_group

    //@begin_group "Color and Transparency"
    //@inspect widget=COLOR
    u32 shallow_color;
    //@inspect widget=COLOR
    u32 deep_color;
    //@inspect min=0.01
    float depth_color_distance;
    //@inspect min=0
    float edge_fade_distance;
    //@end_group

    //@begin_group "Wave 0"
    //@inspect min=0
    float wave_0_height;
    //@inspect min=0.01
    float wave_0_length;
    float wave_0_speed;
    float wave_0_direction_degrees;
    //@end_group

    //@begin_group "Wave 1"
    //@inspect min=0
    float wave_1_height;
    //@inspect min=0.01
    float wave_1_length;
    float wave_1_speed;
    float wave_1_direction_degrees;
    //@end_group

    //@begin_group "Wave 2"
    //@inspect min=0
    float wave_2_height;
    //@inspect min=0.01
    float wave_2_length;
    float wave_2_speed;
    float wave_2_direction_degrees;
    //@end_group

    //@begin_group "Surface Detail and Lighting"
    LDKAssetImage normal_texture;
    //@inspect min=0.01
    float detail_scale;
    //@inspect min=0
    float detail_strength;
    float detail_speed;
    //@inspect min=0
    float specular;
    //@inspect min=1
    float shininess;
    //@end_group

    //@begin_group "Noise and Distortion"
    LDKAssetImage noise_texture;
    //@inspect min=0.01
    float noise_scale;
    float noise_speed;
    //@inspect slider min=0 max=1
    float distortion_strength;
    //@end_group

    //@begin_group "Contact Foam"
    LDKAssetImage foam_texture;
    //@inspect widget=COLOR
    u32 foam_color;
    //@inspect min=0
    float foam_width; // Horizontal surface distance in world units.
    //@inspect slider min=0 max=1
    float foam_strength;
    //@inspect min=0.01
    float foam_scale;
    float foam_speed;
    //@inspect slider min=0 max=1
    float foam_cutoff;
    //@inspect slider min=0 max=1
    float surface_foam_strength;
    //@end_group

    //@begin_group "Shore Waves"
    //@inspect min=0
    float shore_range;
    //@inspect min=0.01
    float shore_wave_length;
    float shore_wave_speed;
    //@inspect slider min=0 max=1
    float shore_foam_strength;
    //@end_group

    //@begin_group "Caustics"
    LDKAssetImage caustics_texture;
    //@inspect min=0.01
    float caustics_scale;
    //@inspect min=0
    float caustics_strength;
    float caustics_speed;
    //@inspect min=0.01
    float caustics_depth;
    //@end_group

    //@begin_group "Runtime"
    //@inspect runtime
    u32 chunk_count;
    //@inspect runtime
    u32 visible_chunk_count;
    //@inspect runtime
    u32 mesh_update_count;
    //@inspect runtime
    bool has_geometry;
    //@end_group
  } LDKWaterSystem;

  LDK_API void ldk_water_system_defaults(LDKWaterSystem *system);
  LDK_API bool ldk_water_system_is_active(void);
  /** Query the animated surface inside the configured rectangle. */
  LDK_API bool ldk_water_height_at_world(
      float world_x, float world_z, float *out_height);

  LDK_API int ldk_water_system_initialize(void *data);
  LDK_API void ldk_water_system_update(
      void *data, const LDKEntityGroup *group, float dt);
  LDK_API void ldk_water_system_terminate(void *data);

#ifdef __cplusplus
}
#endif

#endif // LDK_WATER_SYSTEM_H
