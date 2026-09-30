#ifndef LDK_TEXT_H
#define LDK_TEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_ttf.h>

#include <stdbool.h>

  typedef struct LDKTextGlyphQuad
  {
    LDKGlyph const *glyph;
    u32 codepoint;
    u32 page_index;
    float x0;
    float y0;
    float x1;
    float y1;
    float u0;
    float v0;
    float u1;
    float v1;
  } LDKTextGlyphQuad;

  typedef struct LDKTextLayoutIterator
  {
    LDKFontInstance *font;
    char const *cursor;
    char const *end;
    float origin_x;
    float pen_x;
    float pen_y;
    float line_height;
    u32 previous_codepoint;
    bool multiline;
  } LDKTextLayoutIterator;

  /**
   * @brief Initializes a glyph-layout iterator for a null-terminated UTF-8 string.
   * @param iterator Iterator to initialize.
   * @param font Font instance used for glyph metrics and atlas placement.
   * @param text Null-terminated UTF-8 text.
   * @param x Horizontal text origin in layout units.
   * @param y Top text origin in layout units.
   * @param multiline Whether newline characters advance to a new line.
   * @return true on success, false for invalid arguments.
   */
  LDK_API bool ldk_text_layout_begin(LDKTextLayoutIterator *iterator,
      LDKFontInstance *font, char const *text, float x, float y,
      bool multiline);

  /**
   * @brief Initializes a glyph-layout iterator for a half-open UTF-8 byte range.
   * @param iterator Iterator to initialize.
   * @param font Font instance used for glyph metrics and atlas placement.
   * @param text_start First byte included in the range.
   * @param text_end First byte excluded from the range.
   * @param x Horizontal text origin in layout units.
   * @param y Top text origin in layout units.
   * @param multiline Whether newline characters advance to a new line.
   * @return true on success, false for invalid arguments.
   */
  LDK_API bool ldk_text_layout_begin_range(LDKTextLayoutIterator *iterator,
      LDKFontInstance *font, char const *text_start, char const *text_end,
      float x, float y, bool multiline);

  /**
   * @brief Produces the next drawable glyph quad from a text layout iterator.
   *
   * Invalid glyphs and glyphs whose atlas page cannot be resolved are skipped.
   * Returned positions use the same layout coordinate system supplied to begin.
   * UVs are normalized atlas coordinates and page_index identifies the atlas page.
   *
   * @param iterator Active layout iterator.
   * @param out_quad Destination glyph quad.
   * @return true when a glyph quad was produced, false when iteration is complete.
   */
  LDK_API bool ldk_text_layout_next(
      LDKTextLayoutIterator *iterator, LDKTextGlyphQuad *out_quad);

  /**
   * @brief Finds the next wrapped line within a UTF-8 byte range.
   * @param font Font used to measure glyph advances and kerning.
   * @param start Byte from which layout begins.
   * @param text_end Exclusive end of the UTF-8 byte range.
   * @param max_width Maximum line width. Non-positive values disable wrapping.
   * @param out_line_start Receives the first byte included in the line.
   * @param out_line_end Receives the byte immediately after visible line text.
   * @param out_next Receives the byte from which the following line starts.
   * @param out_width Optional measured width of the produced line.
   * @return true when a line was produced, false for invalid or empty input.
   */
  LDK_API bool ldk_text_wrapped_next_line(LDKFontInstance *font,
      char const *start, char const *text_end, float max_width,
      char const **out_line_start, char const **out_line_end,
      char const **out_next, float *out_width);

#ifdef __cplusplus
}
#endif

#endif // LDK_TEXT_H
