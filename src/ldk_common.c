#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_color.h>

#include <stdarg.h>

LDKSize ldkSize(i32 width, i32 height)
{
  LDKSize size = {.w = width, .h = height}; 
  return size;
}

LDKSize ldkSizeZero()
{
  LDKSize size = {0};
  return size;
}

LDKSize ldkSizeOne()
{
  LDKSize size = {.w = 1, .h = 1}; 
  return size;
}

LDKSizef ldkSizef(float width, float height)
{
  LDKSizef size = {.w = width, .h = height}; 
  return size;
}

LDKSizef ldkSizefZero()
{
  LDKSizef size = {0};
  return size;
}

LDKSizef ldkSizefOne()
{
  LDKSizef size = {.w = 1.0f, .h = 1.0f}; 
  return size;
}


LDKRect ldkRect(i32 x, i32 y, i32 width, i32 height)
{
  LDKRect rect = {.x = x, .y = y, .w = width, .h = height};
  return rect;
}

LDKRectf ldkRectf(float x, float y, float width, float height)
{
  LDKRectf rectf = {.x = x, .y = y, .w = width, .h = height};
  return rectf;
}

LDKPoint ldkPoint(i32 x, i32 y)
{
  LDKPoint point = {.x = x, .y = y};
  return point;
}

LDKPointf ldkPointf(float x, float y)
{
  LDKPointf point = {.x = x, .y = y};
  return point;
}

LDKRGB ldkRGB(u8 r, u8 g, u8 b)
{
  LDKRGB rgb = {.r = r, .g = g, .b = b};
  return rgb;
}

LDKRGBA ldkRGBA(u8 r, u8 g, u8 b, u8 a)
{
  LDKRGBA rgba = {.r = r, .g = g, .b = b, .a = a};
  return rgba;
}
