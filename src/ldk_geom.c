#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_color.h>
#include <stdx/stdx_math.h>
#include <stdarg.h>

LDKSize ldk_size(i32 width, i32 height)
{
  LDKSize size = {.w = width, .h = height}; 
  return size;
}

LDKSize ldk_size_zero()
{
  LDKSize size = {0};
  return size;
}

LDKSize ldk_size_one()
{
  LDKSize size = {.w = 1, .h = 1}; 
  return size;
}

LDKSizef ldk_sizef(float width, float height)
{
  LDKSizef size = {.w = width, .h = height}; 
  return size;
}

LDKSizef ldk_sizef_zero()
{
  LDKSizef size = {0};
  return size;
}

LDKSizef ldk_sizef_one()
{
  LDKSizef size = {.w = 1.0f, .h = 1.0f}; 
  return size;
}

LDKRect ldk_rect(i32 x, i32 y, i32 width, i32 height)
{
  LDKRect rect = {.x = x, .y = y, .w = width, .h = height};
  return rect;
}

bool ldk_rect_contains(const LDKRect* rect, i32 x, i32 y)
{
  if (x < rect->x) { return false; }
  if (y < rect->y) { return false; }
  if (x >= rect->x + rect->w) { return false; }
  if (y >= rect->y + rect->h) { return false; }
  return true;
}

LDKRect ldk_rect_intersect(const LDKRect* a, const LDKRect* b)
{
  LDKRect rect;
  i32 x0 = (i32) float_max((float)a->x, (float)b->x);
  i32 y0 = (i32) float_max((float)a->y, (float)b->y);
  i32 x1 = (i32) float_min((float)(a->x + a->w), (float)(b->x + b->w));
  i32 y1 = (i32) float_min((float)(a->y + a->h), (float)(b->y + b->h));

  rect.x = x0;
  rect.y = y0;
  rect.w = (i32)float_max(0.0f, (float)(x1 - x0));
  rect.h = (i32)float_max(0.0f, (float)(y1 - y0));

  return rect;
}

LDKRectf ldk_rectf(float x, float y, float width, float height)
{
  LDKRectf rectf = {.x = x, .y = y, .w = width, .h = height};
  return rectf;
}

bool ldk_rectf_contains(const LDKRectf* rect, float x, float y)
{
  if (x < rect->x) { return false; }
  if (y < rect->y) { return false; }
  if (x >= rect->x + rect->w) { return false; }
  if (y >= rect->y + rect->h) { return false; }
  return true;
}

LDKRectf ldk_rectf_intersect(const LDKRectf* a, const LDKRectf* b)
{
  LDKRectf rect;
  float x0 = float_max(a->x, b->x);
  float y0 = float_max(a->y, b->y);
  float x1 = float_min(a->x + a->w, b->x + b->w);
  float y1 = float_min(a->y + a->h, b->y + b->h);

  rect.x = x0;
  rect.y = y0;
  rect.w = float_max(0.0f, x1 - x0);
  rect.h = float_max(0.0f, y1 - y0);

  return rect;
}

LDKPoint ldk_point(i32 x, i32 y)
{
  LDKPoint point = {.x = x, .y = y};
  return point;
}

LDKPointf ldk_pointf(float x, float y)
{
  LDKPointf point = {.x = x, .y = y};
  return point;
}

LDKRGB ldk_rgb(u8 r, u8 g, u8 b)
{
  LDKRGB rgb = {.r = r, .g = g, .b = b};
  return rgb;
}

LDKRGBA ldk_rgba(u8 r, u8 g, u8 b, u8 a)
{
  LDKRGBA rgba = {.r = r, .g = g, .b = b, .a = a};
  return rgba;
}

