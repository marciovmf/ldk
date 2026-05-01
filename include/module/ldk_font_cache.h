#ifndef LDK_FONT_CACHE_H
#define LDK_FONT_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_ttf.h>

#include <stdbool.h>
#include <stdint.h>

  typedef uintptr_t LDKFontTextureHandle;

  typedef struct LDKFontCachePage
  {
    u32 page_index;
    u32 width;
    u32 height;
    LDKFontTextureHandle texture;
  } LDKFontCachePage;

  typedef struct LDKFontCacheConfig
  {
    u32 initial_page_capacity;
  } LDKFontCacheConfig;

  typedef struct LDKFontCache
  {
    LDKFontInstance* font;
    LDKFontCachePage* pages;
    u32 page_count;
    u32 page_capacity;
  } LDKFontCache;

  LDK_API bool ldk_font_cache_initialize(LDKFontCache* cache, LDKFontInstance* font, LDKFontCacheConfig const* config);
  LDK_API void ldk_font_cache_terminate(LDKFontCache* cache);

  LDK_API void ldk_font_cache_set_font(LDKFontCache* cache, LDKFontInstance* font);
  LDK_API LDKFontInstance* ldk_font_cache_get_font(LDKFontCache const* cache);

  LDK_API LDKFontTextureHandle ldk_font_cache_get_page_texture(LDKFontCache* cache, u32 page_index);
  LDK_API void ldk_font_cache_invalidate_page(LDKFontCache* cache, u32 page_index);
  LDK_API void ldk_font_cache_invalidate_all(LDKFontCache* cache);

  /**
   * Convenience adapter for systems that expect:
   *   void* user, LDKFontInstance* font, u32 page_index
   * The font argument is accepted so UI callbacks can pass their current font,
   * but the cache remains the texture owner.
   */
  LDK_API LDKFontTextureHandle ldk_font_cache_get_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index);

#ifdef __cplusplus
}
#endif

#endif /* LDK_FONT_CACHE_H */

