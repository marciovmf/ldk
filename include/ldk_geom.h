/**
 * @file   ldk_geom.h
 * @brief  Geometry and point/rect/size helper types
 *
 * Defines basic geometry types and constructors used across LDK.
 */

#ifndef LDK_GEOM_H
#define LDK_GEOM_H

#include <ldk_common.h>

// LDKSize
typedef struct
{
  i32 w;
  i32 h;
} LDKSize;

LDK_API LDKSize ldk_size(i32 width, i32 height);
LDK_API LDKSize ldk_size_zero();
LDK_API LDKSize ldk_size_one();

typedef struct
{
  float w;
  float h;
} LDKSizef;

LDK_API LDKSizef ldk_sizef(float width, float height);
LDK_API LDKSizef ldk_sizef_zero();
LDK_API LDKSizef ldk_sizef_one();


// LDKRect
typedef struct 
{
  i32 x;
  i32 y;
  i32 w;
  i32 h;
} LDKRect;

LDK_API LDKRect ldk_rect(i32 x, i32 y, i32 width, i32 height);
LDK_API bool ldk_rect_contains(const LDKRect* rect, i32 x, i32 y);
LDK_API LDKRect ldk_rect_intersect(const LDKRect* a, const LDKRect* b);

// LDKRectf
typedef struct 
{
  float x;
  float y;
  float w;
  float h;
} LDKRectf;


LDK_API LDKRectf ldk_rectf(float x, float y, float width, float height);
LDK_API bool ldk_rectf_contains(const LDKRectf* rect, float x, float y);
LDK_API LDKRectf ldk_rectf_intersect(const LDKRectf* a, const LDKRectf* b);

// LDKPoint
typedef struct
{
  i32 x;
  i32 y;
} LDKPoint;

LDK_API LDKPoint ldk_point(i32 x, i32 y);

// LDKPointf
typedef struct
{
  float x;
  float y;
} LDKPointf;

LDK_API LDKPointf ldk_pointf(float x, float y);
#endif //LDK_GEOM_H

