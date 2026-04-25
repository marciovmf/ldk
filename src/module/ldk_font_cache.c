#include <ldk_font_cache.h>

#include <ldk_gl.h>

#include <stdlib.h>
#include <string.h>

static LDKFontCachePage* s_ldk_font_cache_find_page(LDKFontCache* cache, u32 page_index)
{
  if (cache == NULL)
  {
    return NULL;
  }

  for (u32 i = 0; i < cache->page_count; ++i)
  {
    LDKFontCachePage* page = cache->pages + i;

    if (page->page_index == page_index)
    {
      return page;
    }
  }

  return NULL;
}

static bool s_ldk_font_cache_reserve(LDKFontCache* cache, u32 capacity)
{
  if (cache == NULL)
  {
    return false;
  }

  if (capacity <= cache->page_capacity)
  {
    return true;
  }

  u32 new_capacity = cache->page_capacity;

  if (new_capacity == 0)
  {
    new_capacity = 4;
  }

  while (new_capacity < capacity)
  {
    new_capacity *= 2;
  }

  LDKFontCachePage* new_pages = (LDKFontCachePage*)realloc(cache->pages, sizeof(LDKFontCachePage) * new_capacity);

  if (new_pages == NULL)
  {
    return false;
  }

  cache->pages = new_pages;
  cache->page_capacity = new_capacity;

  return true;
}

static void s_ldk_font_cache_delete_page_texture(LDKFontCachePage* page)
{
  if (page == NULL)
  {
    return;
  }

  if (page->texture == 0)
  {
    return;
  }

  GLuint texture = (GLuint)page->texture;
  glDeleteTextures(1, &texture);

  page->texture = 0;
}

static bool s_ldk_font_cache_create_page_texture(LDKFontCachePage* cache_page, LDKFontPageInfo const* font_page)
{
  GLuint texture = 0;

  if (cache_page == NULL || font_page == NULL)
  {
    return false;
  }

  if (font_page->pixels == NULL || font_page->width == 0 || font_page->height == 0)
  {
    return false;
  }

  glGenTextures(1, &texture);

  if (texture == 0)
  {
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_R8,
      (GLsizei)font_page->width,
      (GLsizei)font_page->height,
      0,
      GL_RED,
      GL_UNSIGNED_BYTE,
      font_page->pixels
      );

  cache_page->page_index = font_page->page_index;
  cache_page->width = font_page->width;
  cache_page->height = font_page->height;
  cache_page->texture = (LDKFontTextureHandle)texture;

  return true;
}

static bool s_ldk_font_cache_update_page_texture(LDKFontCachePage* cache_page, LDKFontPageInfo const* font_page)
{
  if (cache_page == NULL || font_page == NULL)
  {
    return false;
  }

  if (cache_page->texture == 0)
  {
    return s_ldk_font_cache_create_page_texture(cache_page, font_page);
  }

  if (cache_page->width != font_page->width || cache_page->height != font_page->height)
  {
    s_ldk_font_cache_delete_page_texture(cache_page);
    return s_ldk_font_cache_create_page_texture(cache_page, font_page);
  }

  GLuint texture = (GLuint)cache_page->texture;

  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexSubImage2D(
      GL_TEXTURE_2D,
      0,
      0,
      0,
      (GLsizei)font_page->width,
      (GLsizei)font_page->height,
      GL_RED,
      GL_UNSIGNED_BYTE,
      font_page->pixels
      );

  return true;
}

bool ldk_font_cache_initialize(LDKFontCache* cache, LDKFontInstance* font, LDKFontCacheConfig const* config)
{
  if (cache == NULL)
  {
    return false;
  }

  memset(cache, 0, sizeof(*cache));

  cache->font = font;

  u32 capacity = 4;

  if (config != NULL && config->initial_page_capacity > 0)
  {
    capacity = config->initial_page_capacity;
  }

  cache->pages = (LDKFontCachePage*)calloc(capacity, sizeof(LDKFontCachePage));

  if (cache->pages == NULL)
  {
    memset(cache, 0, sizeof(*cache));
    return false;
  }

  cache->page_capacity = capacity;

  return true;
}

void ldk_font_cache_terminate(LDKFontCache* cache)
{
  if (cache == NULL)
  {
    return;
  }

  ldk_font_cache_invalidate_all(cache);

  free(cache->pages);

  memset(cache, 0, sizeof(*cache));
}

void ldk_font_cache_set_font(LDKFontCache* cache, LDKFontInstance* font)
{
  if (cache == NULL)
  {
    return;
  }

  if (cache->font == font)
  {
    return;
  }

  ldk_font_cache_invalidate_all(cache);
  cache->font = font;
}

LDKFontInstance* ldk_font_cache_get_font(LDKFontCache const* cache)
{
  if (cache == NULL)
  {
    return NULL;
  }

  return cache->font;
}

LDKFontTextureHandle ldk_font_cache_get_page_texture(LDKFontCache* cache, u32 page_index)
{
  if (cache == NULL || cache->font == NULL)
  {
    return 0;
  }

  LDKFontPageInfo font_page = {0};

  if (!ldk_font_get_page_info(cache->font, page_index, &font_page))
  {
    return 0;
  }

  LDKFontCachePage* cache_page = s_ldk_font_cache_find_page(cache, page_index);

  if (cache_page == NULL)
  {
    if (!s_ldk_font_cache_reserve(cache, cache->page_count + 1))
    {
      return 0;
    }

    cache_page = cache->pages + cache->page_count;
    memset(cache_page, 0, sizeof(*cache_page));
    cache->page_count += 1;

    if (!s_ldk_font_cache_create_page_texture(cache_page, &font_page))
    {
      cache->page_count -= 1;
      return 0;
    }

    ldk_font_clear_page_dirty(cache->font, page_index);

    return cache_page->texture;
  }

  if (font_page.dirty)
  {
    if (!s_ldk_font_cache_update_page_texture(cache_page, &font_page))
    {
      return cache_page->texture;
    }

    ldk_font_clear_page_dirty(cache->font, page_index);
  }

  return cache_page->texture;
}

void ldk_font_cache_invalidate_page(LDKFontCache* cache, u32 page_index)
{
  if (cache == NULL)
  {
    return;
  }

  for (u32 i = 0; i < cache->page_count; ++i)
  {
    LDKFontCachePage* page = cache->pages + i;

    if (page->page_index != page_index)
    {
      continue;
    }

    s_ldk_font_cache_delete_page_texture(page);

    if (i + 1 < cache->page_count)
    {
      memmove(
          cache->pages + i,
          cache->pages + i + 1,
          sizeof(LDKFontCachePage) * (cache->page_count - i - 1)
          );
    }

    cache->page_count -= 1;
    return;
  }
}

void ldk_font_cache_invalidate_all(LDKFontCache* cache)
{
  if (cache == NULL)
  {
    return;
  }

  for (u32 i = 0; i < cache->page_count; ++i)
  {
    LDKFontCachePage* page = cache->pages + i;
    s_ldk_font_cache_delete_page_texture(page);
  }

  cache->page_count = 0;
}

LDKFontTextureHandle ldk_font_cache_get_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index)
{
  LDKFontCache* cache = (LDKFontCache*)user;

  if (cache == NULL)
  {
    return 0;
  }

  if (font != NULL && cache->font != font)
  {
    ldk_font_cache_set_font(cache, font);
  }

  return ldk_font_cache_get_page_texture(cache, page_index);
}
