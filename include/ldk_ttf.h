#ifndef LDK_FONT_H
#define LDK_FONT_H
#include <ldk_geom.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>

#include <stdbool.h>
#include <stdint.h>

  typedef LDKSizef LDKTextSize;
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

  /**
   * @brief Consumes one UTF-8 codepoint and advances the input cursor.
   * @param cursor Pointer to the current UTF-8 string position. Updated to the next byte after the decoded sequence.
   * @param out_codepoint Destination for the decoded Unicode codepoint.
   * @return true if a codepoint was consumed, false if input is invalid or the cursor is at the string terminator.
   */
  LDK_API bool ldk_ttf_utf8_consume_codepoint(char const** cursor, u32* out_codepoint);

  /**
   * @brief Consumes one UTF-8 codepoint within a bounded byte range and advances the input cursor.
   * @param cursor Pointer to the current UTF-8 string position. Updated to the next byte after the decoded sequence.
   * @param end Exclusive end pointer for the UTF-8 byte range.
   * @param out_codepoint Destination for the decoded Unicode codepoint.
   * @return true if a codepoint was consumed, false if input is invalid, the cursor is at the string terminator, or the cursor reached end.
   */
  LDK_API bool ldk_ttf_utf8_consume_codepoint_range(char const** cursor, char const* end, u32* out_codepoint);

  /**
   * @brief Creates a font face from TrueType font data.
   * @param data Source font file data.
   * @param data_size Size of the source font data in bytes.
   * @return New font face, or NULL on failure.
   */
  LDK_API LDKFontFace* ldk_ttf_face_create(void const* data, u32 data_size);

  /**
   * @brief Destroys a font face and all font instances created from it.
   * @param face Font face to destroy. May be NULL.
   */
  LDK_API void ldk_ttf_face_destroy(LDKFontFace* face);

  /**
   * @brief Gets or creates a font instance for a specific pixel height.
   * @param face Source font face.
   * @param pixel_height Requested font pixel height.
   * @param atlas_desc Optional atlas configuration. NULL uses default atlas settings.
   * @return Font instance, or NULL on failure.
   */
  LDK_API LDKFontInstance* ldk_ttf_get_instance(LDKFontFace* face, float pixel_height, LDKFontAtlasDesc const* atlas_desc);

  /**
   * @brief Gets the pixel height used by a font instance.
   * @param instance Font instance.
   * @return Pixel height, or 0 if instance is NULL.
   */
  LDK_API float ldk_ttf_get_pixel_height(LDKFontInstance const* instance);

  /**
   * @brief Gets scaled font metrics for a font instance.
   * @param instance Font instance.
   * @return Font metrics. Returns zeroed metrics if instance is NULL.
   */
  LDK_API LDKFontMetrics ldk_ttf_get_metrics(LDKFontInstance const* instance);

  /**
   * @brief Gets a glyph, rasterizing and caching it if necessary.
   * @param instance Font instance.
   * @param codepoint Unicode codepoint.
   * @return Glyph data, or NULL on failure.
   */
  LDK_API LDKGlyph const* ldk_ttf_get_glyph(LDKFontInstance* instance, u32 codepoint);

  /**
   * @brief Gets the horizontal kerning adjustment between two codepoints.
   * @param instance Font instance.
   * @param left_codepoint Previous Unicode codepoint.
   * @param right_codepoint Current Unicode codepoint.
   * @return Kerning adjustment in pixels, or 0 if unavailable.
   */
  LDK_API float ldk_ttf_get_kerning(LDKFontInstance const* instance, u32 left_codepoint, u32 right_codepoint);

  /**
   * @brief Preloads and caches all glyphs in an inclusive codepoint range.
   * @param instance Font instance.
   * @param first_codepoint First Unicode codepoint to preload.
   * @param last_codepoint Last Unicode codepoint to preload.
   * @return true if all glyphs were processed, false on failure.
   */
  LDK_API bool ldk_ttf_preload_range(LDKFontInstance* instance, u32 first_codepoint, u32 last_codepoint);

  /**
   * @brief Preloads printable basic ASCII glyphs.
   * @param instance Font instance.
   * @return true if all glyphs were processed, false on failure.
   */
  LDK_API bool ldk_ttf_preload_basic_ascii(LDKFontInstance* instance);

  /**
   * @brief Gets the number of atlas pages owned by a font instance.
   * @param instance Font instance.
   * @return Number of atlas pages, or 0 if instance is NULL.
   */
  LDK_API u32 ldk_ttf_get_page_count(LDKFontInstance const* instance);

  /**
   * @brief Gets information for a font atlas page.
   * @param instance Font instance.
   * @param page_index Atlas page index.
   * @param out_page Destination page information.
   * @return true if page information was written, false on failure.
   */
  LDK_API bool ldk_ttf_get_page_info(LDKFontInstance const* instance, u32 page_index, LDKFontPageInfo* out_page);

  /**
   * @brief Clears the dirty flag of an atlas page.
   * @param instance Font instance.
   * @param page_index Atlas page index.
   */
  LDK_API void ldk_ttf_clear_page_dirty(LDKFontInstance* instance, u32 page_index);

  /**
   * @brief Gets the scaled line height for a font instance.
   * @param instance Font instance.
   * @return Line height in pixels, or 0 if instance is NULL.
   */
  LDK_API float ldk_ttf_get_line_height(LDKFontInstance* instance);

  /**
   * @brief Measures a null-terminated UTF-8 string.
   * @param instance Font instance.
   * @param text Null-terminated UTF-8 string.
   * @return Text size in pixels.
   */
  LDK_API LDKTextSize ldk_ttf_measure_text_cstr(LDKFontInstance* instance, char const* text);

  /**
   * @brief Measures a UTF-8 string with a byte limit.
   * @param instance Font instance.
   * @param text UTF-8 string.
   * @param byte_count Maximum number of bytes to read. If 0, measures an empty string.
   * @return Text size in pixels.
   */
  LDK_API LDKTextSize ldk_ttf_measure_text_cstrn(LDKFontInstance* instance, char const* text, u32 byte_count);

  /**
   * @brief Measures a null-terminated UTF-8 string with word wrapping.
   * @param instance Font instance.
   * @param text Null-terminated UTF-8 string.
   * @param max_width Maximum line width in pixels. If less than or equal to 0, wrapping is disabled.
   * @return Wrapped text size in pixels.
   */
  LDK_API LDKSizef ldk_ttf_measure_text_cstr_wrapped(LDKFontInstance* instance, char const* text, float max_width);

#ifdef __cplusplus
}
#endif

#endif // LDK_FONT_H
