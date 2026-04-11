/**
 * @file   ldk_geom.h
 * @brief  Geometry and point/rect helper types
 *
 * Defines basic geometry types and constructors used across LDK.
 */

#ifndef LDK_GEOM_H
#define LDK_GEOM_H

#include <ldk_common.h>

// LDKSize
typedef struct
{
  i32 width;
  i32 height;
} LDKSize;

LDK_API LDKSize ldkSize(i32 width, i32 height);
LDK_API LDKSize ldkSizeZero();
LDK_API LDKSize ldkSizeOne();

// LDKRect
typedef struct 
{
  i32 x;
  i32 y;
  i32 w;
  i32 h;
} LDKRect;

LDK_API LDKRect ldkRect(i32 x, i32 y, i32 width, i32 height);

// LDKRectf
typedef struct 
{
  float x;
  float y;
  float w;
  float h;
} LDKRectf;


LDK_API LDKRectf ldkRectf(float x, float y, float width, float height);

// LDKPoint
typedef struct
{
  i32 x;
  i32 y;
} LDKPoint;

LDK_API LDKPoint ldkPoint(i32 x, i32 y);

// LDKPointf
typedef struct
{
  float x;
  float y;
} LDKPointf;

LDK_API LDKPointf ldkPointf(float x, float y);
#endif //LDK_GEOM_H

