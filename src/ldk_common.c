#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_color.h>

#include <stdarg.h>

LDKSize ldkSize(i32 width, i32 height)
{
  LDKSize size = {.width = width, .height = height}; 
  return size;
}

LDKSize ldkSizeZero()
{
  LDKSize size = {0};
  return size;
}

LDKSize ldkSizeOne()
{
  LDKSize size = {.width = 1, .height = 1}; 
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
