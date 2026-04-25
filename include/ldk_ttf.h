#ifndef LDK_FONT_H
#define LDK_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>

#include <stdbool.h>
#include <stdint.h>

  typedef struct LDKFontFace LDKFontFace;
  typedef struct LDKFontInstance LDKFontInstance;

  typedef struct LDKFontAtlasDesc
  {
    u16 page_width;
    u16 page_height;
    u16 padding;
  } LDKFontAtlasDesc;

  typedef struct LDKFontMetrics
  {
    float ascent;
    float descent;
    float line_gap;
    float line_height;
  } LDKFontMetrics;

  typedef struct LDKGlyph
  {
    u32 codepoint;
    u32 glyph_index;
    u16 page_index;

    u16 atlas_x0;
    u16 atlas_y0;
    u16 atlas_x1;
    u16 atlas_y1;

    i16 offset_x;
    i16 offset_y;
    i16 advance_x;

    bool valid;
  } LDKGlyph;

  typedef struct LDKFontPageInfo
  {
    u32 page_index;
    u32 width;
    u32 height;
    u8 const* pixels;
    bool dirty;
  } LDKFontPageInfo;

  LDK_API bool ldk_font_utf8_decode(char const** cursor, u32* out_codepoint);
  LDK_API LDKFontFace* ldk_font_face_create(void const* data, u32 data_size);
  LDK_API void ldk_font_face_destroy(LDKFontFace* face);
  LDK_API LDKFontInstance* ldk_font_get_instance(LDKFontFace* face, float pixel_height, LDKFontAtlasDesc const* atlas_desc);
  LDK_API float ldk_font_get_pixel_height(LDKFontInstance const* instance);
  LDK_API LDKFontMetrics ldk_font_get_metrics(LDKFontInstance const* instance);
  LDK_API LDKGlyph const* ldk_font_get_glyph(LDKFontInstance* instance, u32 codepoint);
  LDK_API float ldk_font_get_kerning(LDKFontInstance const* instance, u32 left_codepoint, u32 right_codepoint);
  LDK_API bool ldk_font_preload_range(LDKFontInstance* instance, u32 first_codepoint, u32 last_codepoint);
  LDK_API bool ldk_font_preload_basic_ascii(LDKFontInstance* instance);
  LDK_API u32 ldk_font_get_page_count(LDKFontInstance const* instance);
  LDK_API bool ldk_font_get_page_info(LDKFontInstance const* instance, u32 page_index, LDKFontPageInfo* out_page);
  LDK_API void ldk_font_clear_page_dirty(LDKFontInstance* instance, u32 page_index);
  LDK_API float ldk_font_measure_text_cstr(LDKFontInstance* instance, char const* text);
  LDK_API float ldk_font_get_line_height(LDKFontInstance* instance);

#ifdef __cplusplus
}
#endif

#endif // LDK_FONT_H
