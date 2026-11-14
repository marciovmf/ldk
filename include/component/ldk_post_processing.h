/**
 * # Post-processing component
 *
 * Camera post-processing configuration.
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

  /** @group ldk.component.post_processing */
  /* Ordered threshold matrices. Bayer 8x8 matches dispersed-dot
   * halftoning; this does not imply monochrome output. */
  //@enum
  typedef enum LDKRetroDitherPattern
  {
    LDK_RETRO_DITHER_PATTERN_CLUSTERED_4X4 = 0,
    LDK_RETRO_DITHER_PATTERN_BAYER_4X4,
    LDK_RETRO_DITHER_PATTERN_BAYER_8X8
  } LDKRetroDitherPattern;

  /** @group ldk.component.post_processing */
  /* The default preserves legacy blur-before-color behavior. The alternate
   * mode applies the same blur kernel to the fully processed image, including
   * dithering, and composites with the existing focus mask. */
  //@enum
  typedef enum LDKBlurOrder
  {
    LDK_BLUR_ORDER_BEFORE_DITHER = 0,
    LDK_BLUR_ORDER_AFTER_DITHER
  } LDKBlurOrder;

  /** @group ldk.component.post_processing */
  //@component
  typedef struct LDKPostProcessing
  {
    bool enabled;

    //@begin_group "Tonemapping"
    bool tonemapping_enabled;
    //@inspect slider min=-10.0 max=10.0
    float exposure;
    //@end_group

    //@begin_group "Bloom"
    bool bloom_enabled;
    //@inspect slider min=0.0 max=10.0
    float bloom_threshold;
    //@inspect slider min=0.0 max=3.0
    float bloom_intensity;
    //@inspect slider min=0.0 max=1.0
    float bloom_radius;
    //@inspect slider min=0.0 max=1.0
    float bloom_soft_knee;
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
    // After Dither blurs the finished post-process image (also when Retro is off).
    LDKBlurOrder blur_order;
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
    // Additional ordered-dot contrast, independently of color levels.
    //@inspect slider min=0.0 max=1.0
    float retro_dither_contrast;
    //@inspect slider min=1.0 max=8.0
    float retro_pattern_scale;
    LDKRetroDitherPattern retro_dither_pattern;
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

    //@begin_group "Dither Region"
    // Optional screen-space region for Retro dithering. With both modes off,
    // dithering covers the whole screen as before. The horizontal band wins
    // when both toggles are on, preserving existing radial-enabled scenes.
    // These masks only affect dither strength, not pixelation or blur.
    bool retro_dither_radial_enabled;
    // Radial radius uses the normalized screen half-diagonal, accounting for
    // aspect ratio to remain circular in pixels. Ignored by horizontal band.
    //@inspect slider min=0.0 max=1.0
    float retro_dither_radius;
    // The radial mode measures feather in half-diagonal units; the band mode
    // measures it in viewport height. In either case this extends outwards
    // from the fully dithered area, never cutting it with a hard edge.
    //@inspect slider min=0.0 max=1.0
    float retro_dither_feather;
    // UV-space offsets from the screen center: +0.1 shifts by 10% of the
    // viewport width/height. Horizontal band uses only offset_y (the X offset
    // does not alter a band that crosses the entire viewport).
    //@inspect slider min=-1.0 max=1.0
    float retro_dither_offset_x;
    //@inspect slider min=-1.0 max=1.0
    float retro_dither_offset_y;
    // New horizontal band controls follow the original radial fields so
    // existing component member offsets remain unchanged.
    bool retro_dither_horizontal_band_enabled;
    // Half the vertical height of the fully dithered horizontal band, in
    // normalized viewport height (0.25 = middle 50% of the screen).
    //@inspect slider min=0.0 max=0.5
    float retro_dither_band_half_height;
    //@end_group
  } LDKPostProcessing;

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the post processing component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group ldk.component.post_processing
   */
  LDK_API LDKComponentDesc ldk_post_processing_component_desc(
      u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_POST_PROCESSING_H
