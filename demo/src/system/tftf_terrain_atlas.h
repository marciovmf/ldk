/*
 * This file is auto generated. Do not edit manually.
 * Icon rects use pixel coordinates with origin at the top left.
 */
#ifndef TFTF_TERRAIN_ATLAS_H
#define TFTF_TERRAIN_ATLAS_H

#include <stdint.h>

#include <ldk_geom.h>

typedef LDKRectf LDKEditorIconRect;

#define TFTF_ICON_ATLAS_WIDTH 128u
#define TFTF_ICON_ATLAS_HEIGHT 1024u

typedef enum LDKEditorIcon
{
  TFTF_ICON_DEEP_WATER,
  TFTF_ICON_GRASS,
  TFTF_ICON_GRASS_DARK,
  TFTF_ICON_ROCK,
  TFTF_ICON_SAND,
  TFTF_ICON_SHALLOW_WATER,

  TFTF_ICON_COUNT
} LDKEditorIcon;

static const LDKEditorIconRect tftf_icon_rects[TFTF_ICON_COUNT] =
{
  { 0.0, 0.0, 1.0, 0.125 }, /* deep_water.png */
  { 0.0, 0.126953125, 1.0, 0.125 }, /* grass.png */
  { 0.0, 0.25390625, 1.0, 0.125 }, /* grass_dark.png */
  { 0.0, 0.380859375, 1.0, 0.125 }, /* rock.png */
  { 0.0, 0.5078125, 1.0, 0.125 }, /* sand.png */
  { 0.0, 0.634765625, 1.0, 0.125 }, /* shallow_water.png */
};

#endif /* TFTF_TERRAIN_ATLAS_H */
