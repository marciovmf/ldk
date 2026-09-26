/**
 * @file ldk_post_processing.h
 * @brief Camera post-processing configuration.
 */

#ifndef LDK_POST_PROCESSING_H
#define LDK_POST_PROCESSING_H

#include <ldk_common.h>
#include <module/ldk_component.h>
#include <module/ldk_entity.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  //@component
  typedef struct LDKPostProcessing
  {
    bool enabled;

    //@begin_group "Tonemapping"
    bool tonemapping_enabled;
    //@inspect slider min=-10.0 max=10.0
    float exposure;
    //@end_group

    //@begin_group "Blur"
    bool blur_enabled;
    //@inspect slider min=0.0 max=1.0
    float blur_strength;
    //@inspect slider min=0.0 max=1.0
    float blur_focus_size;
    //@inspect slider min=0.0 max=1.0
    float blur_focus_feather;
    bool blur_inverted;
    //@inspect slider min=-1.0 max=1.0
    float blur_focus_offset_x;
    //@inspect slider min=-1.0 max=1.0
    float blur_focus_offset_y;
    //@end_group

    //@begin_group "Color"
    bool color_enabled;
    //@inspect slider min=-1.0 max=1.0
    float brightness;
    //@inspect slider min=0.0 max=2.0
    float contrast;
    //@inspect slider min=0.0 max=2.0
    float saturation;
    //@end_group

    //@begin_group "Vignette"
    bool vignette_enabled;
    //@inspect slider min=0.0 max=1.0
    float vignette_intensity;
    //@inspect slider min=0.0 max=1.0
    float vignette_radius;
    //@inspect slider min=0.0 max=1.0
    float vignette_softness;
    //@end_group

    //@begin_group "Screen Distortion"
    bool screen_distortion_enabled;
    //@inspect slider min=-1.0 max=1.0
    float screen_distortion_strength;
    //@end_group

    //@begin_group "Chromatic Aberration"
    bool chromatic_aberration_enabled;
    //@inspect slider min=0.0 max=1.0
    float chromatic_aberration_strength;
    //@end_group

    //@begin_group "Heat"
    bool heat_enabled;
    //@inspect slider min=0.0 max=1.0
    float heat_strength;
    //@inspect slider min=0.0 max=32.0
    float heat_amplitude;
    //@inspect slider min=0.25 max=4.0
    float heat_scale;
    //@inspect slider min=0.0 max=4.0
    float heat_speed;
    //@end_group

    //@begin_group "Image Effects"
    bool inverse_enabled;
    bool black_and_white_enabled;
    //@end_group

    //@begin_group "Retro"
    bool retro_enabled;
    //@inspect slider min=1.0 max=16.0
    float retro_pixel_size;
    //@inspect slider min=2.0 max=16.0
    float retro_color_levels;
    //@inspect slider min=0.0 max=1.0
    float retro_dither_strength;
    //@inspect slider min=1.0 max=8.0
    float retro_pattern_scale;
    //@end_group

    //@begin_group "Drunk"
    bool drunk_enabled;
    //@inspect slider min=0.0 max=1.0
    float drunk_strength;
    //@inspect slider min=0.0 max=4.0
    float drunk_speed;
    //@inspect slider min=0.0 max=1.0
    float drunk_ghosting;
    //@inspect slider min=0.0 max=1.0
    float drunk_chromatic_aberration;
    //@inspect slider min=0.0 max=3.0
    float drunk_movement;
    //@end_group
  } LDKPostProcessing;

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_post_processing_component_desc(
      u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_POST_PROCESSING_H
