#include "ldk_ttf.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#ifndef LDK_FONT_MALLOC
#define LDK_FONT_MALLOC(size) malloc(size)
#endif

#ifndef LDK_FONT_FREE
#define LDK_FONT_FREE(ptr) free(ptr)
#endif

typedef struct LDKFontPage
{
  u16 width;
  u16 height;
  u16 pen_x;
  u16 pen_y;
  u16 row_height;
  bool dirty;
  u8* pixels;
} LDKFontPage;

typedef struct LDKFontGlyphEntry
{
  LDKGlyph glyph;
} LDKFontGlyphEntry;

struct LDKFontInstance
{
  LDKFontFace* face;
  float pixel_height;
  float scale;
  LDKFontAtlasDesc atlas_desc;
  LDKFontMetrics metrics;

  LDKFontPage* pages;
  u32 page_count;
  u32 page_capacity;

  LDKFontGlyphEntry* glyphs;
  u32 glyph_count;
  u32 glyph_capacity;
};

struct LDKFontFace
{
  u8* data;
  u32 data_size;
  stbtt_fontinfo info;

  LDKFontInstance** instances;
  u32 instance_count;
  u32 instance_capacity;
};

static bool ldk_font_grow_array(void** ptr, u32* capacity, u32 count, size_t stride)
{
  void* new_ptr = NULL;
  u32 new_capacity = 0;

  if (count < *capacity)
  {
    return true;
  }

  new_capacity = (*capacity == 0) ? 8u : (*capacity * 2u);

  while (new_capacity <= count)
  {
    new_capacity *= 2u;
  }

  new_ptr = LDK_FONT_MALLOC((size_t)new_capacity * stride);

  if (new_ptr == NULL)
  {
    return false;
  }

  if (*ptr != NULL)
  {
    memcpy(new_ptr, *ptr, (size_t)count * stride);
    LDK_FONT_FREE(*ptr);
  }

  *ptr = new_ptr;
  *capacity = new_capacity;
  return true;
}

static u32 ldk_font_round_to_u32(float value)
{
  if (value <= 0.0f)
  {
    return 0;
  }

  return (u32)(value + 0.5f);
}

static i16 ldk_font_round_to_i16(float value)
{
  if (value >= 32767.0f)
  {
    return 32767;
  }

  if (value <= -32768.0f)
  {
    return -32768;
  }

  if (value >= 0.0f)
  {
    return (i16)(value + 0.5f);
  }

  return (i16)(value - 0.5f);
}

static LDKFontGlyphEntry* ldk_font_find_glyph_entry(LDKFontInstance const* instance, u32 codepoint)
{
  u32 i = 0;

  if (instance == NULL)
  {
    return NULL;
  }

  for (i = 0; i < instance->glyph_count; ++i)
  {
    if (instance->glyphs[i].glyph.codepoint == codepoint)
    {
      return &instance->glyphs[i];
    }
  }

  return NULL;
}

static bool ldk_font_append_page(LDKFontInstance* instance)
{
  LDKFontPage* page = NULL;
  size_t pixel_count = 0;

  if (instance == NULL)
  {
    return false;
  }

  if (!ldk_font_grow_array((void**)&instance->pages, &instance->page_capacity, instance->page_count, sizeof(LDKFontPage)))
  {
    return false;
  }

  page = &instance->pages[instance->page_count];
  memset(page, 0, sizeof(*page));

  page->width = instance->atlas_desc.page_width;
  page->height = instance->atlas_desc.page_height;
  pixel_count = (size_t)page->width * (size_t)page->height;
  page->pixels = (u8*)LDK_FONT_MALLOC(pixel_count);

  if (page->pixels == NULL)
  {
    return false;
  }

  memset(page->pixels, 0, pixel_count);
  page->dirty = true;
  instance->page_count += 1;

  return true;
}

static bool ldk_font_page_pack_rect(LDKFontPage* page, u16 padding, u16 w, u16 h, u16* out_x, u16* out_y)
{
  u32 padded_w = (u32)w + (u32)padding;
  u32 padded_h = (u32)h + (u32)padding;

  if (page == NULL || out_x == NULL || out_y == NULL)
  {
    return false;
  }

  if (w == 0 || h == 0)
  {
    *out_x = 0;
    *out_y = 0;
    return true;
  }

  if ((u32)page->pen_x + padded_w > page->width)
  {
    page->pen_x = 0;
    page->pen_y = (u16)((u32)page->pen_y + (u32)page->row_height);
    page->row_height = 0;
  }

  if ((u32)page->pen_y + padded_h > page->height)
  {
    return false;
  }

  *out_x = page->pen_x;
  *out_y = page->pen_y;
  page->pen_x = (u16)((u32)page->pen_x + padded_w);

  if (padded_h > page->row_height)
  {
    page->row_height = (u16)padded_h;
  }

  return true;
}

static bool ldk_font_store_bitmap(LDKFontPage* page, u16 dst_x, u16 dst_y, int w, int h, u8 const* src)
{
  int row = 0;

  if (page == NULL)
  {
    return false;
  }

  if (w <= 0 || h <= 0 || src == NULL)
  {
    return true;
  }

  for (row = 0; row < h; ++row)
  {
    u8* dst = page->pixels + ((size_t)(dst_y + (u16)row) * page->width) + dst_x;
    memcpy(dst, src + ((size_t)row * (size_t)w), (size_t)w);
  }

  page->dirty = true;
  return true;
}

static bool ldk_font_rasterize_glyph(LDKFontInstance* instance, u32 codepoint, LDKGlyph* out_glyph)
{
  int glyph_index = 0;
  int advance_width = 0;
  int left_side_bearing = 0;
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  int width = 0;
  int height = 0;
  unsigned char* bitmap = NULL;
  u16 packed_x = 0;
  u16 packed_y = 0;
  u32 page_index = 0;
  LDKFontPage* page = NULL;
  LDKGlyph glyph;

  if (instance == NULL || out_glyph == NULL)
  {
    return false;
  }

  memset(&glyph, 0, sizeof(glyph));
  glyph.codepoint = codepoint;

  glyph_index = stbtt_FindGlyphIndex(&instance->face->info, (int)codepoint);
  glyph.glyph_index = (u32)glyph_index;

  if (glyph_index == 0)
  {
    glyph.valid = false;
    *out_glyph = glyph;
    return true;
  }

  stbtt_GetGlyphHMetrics(&instance->face->info, glyph_index, &advance_width, &left_side_bearing);
  width = x1 - x0;
  height = y1 - y0;
  bitmap = stbtt_GetGlyphBitmap(&instance->face->info, instance->scale, instance->scale, glyph_index, &width, &height, &x0, &y0);

  if (instance->page_count == 0)
  {
    if (!ldk_font_append_page(instance))
    {
      if (bitmap != NULL)
      {
        stbtt_FreeBitmap(bitmap, NULL);
      }

      return false;
    }
  }

  for (page_index = 0; page_index < instance->page_count; ++page_index)
  {
    page = &instance->pages[page_index];

    if (ldk_font_page_pack_rect(page, instance->atlas_desc.padding, (u16)ldk_font_round_to_u32((float)width), (u16)ldk_font_round_to_u32((float)height), &packed_x, &packed_y))
    {
      break;
    }
  }

  if (page_index == instance->page_count)
  {
    if (!ldk_font_append_page(instance))
    {
      if (bitmap != NULL)
      {
        stbtt_FreeBitmap(bitmap, NULL);
      }

      return false;
    }

    page = &instance->pages[instance->page_count - 1u];

    if (!ldk_font_page_pack_rect(page, instance->atlas_desc.padding, (u16)ldk_font_round_to_u32((float)width), (u16)ldk_font_round_to_u32((float)height), &packed_x, &packed_y))
    {
      if (bitmap != NULL)
      {
        stbtt_FreeBitmap(bitmap, NULL);
      }

      return false;
    }

    page_index = instance->page_count - 1u;
  }

  if (!ldk_font_store_bitmap(page, packed_x, packed_y, width, height, bitmap))
  {
    if (bitmap != NULL)
    {
      stbtt_FreeBitmap(bitmap, NULL);
    }

    return false;
  }

  if (bitmap != NULL)
  {
    stbtt_FreeBitmap(bitmap, NULL);
  }

  glyph.page_index = (u16)page_index;
  glyph.atlas_x0 = packed_x;
  glyph.atlas_y0 = packed_y;
  glyph.atlas_x1 = (u16)((u32)packed_x + (u32)ldk_font_round_to_u32((float)width));
  glyph.atlas_y1 = (u16)((u32)packed_y + (u32)ldk_font_round_to_u32((float)height));
  glyph.offset_x = ldk_font_round_to_i16((float)x0);
  glyph.offset_y = ldk_font_round_to_i16((float)y0);
  glyph.advance_x = ldk_font_round_to_i16((float)advance_width * instance->scale);
  glyph.valid = true;

  *out_glyph = glyph;
  return true;
}

bool ldk_font_utf8_decode(char const** cursor, u32* out_codepoint)
{
  u8 const* text = NULL;
  u32 codepoint = 0;

  if (cursor == NULL || *cursor == NULL || out_codepoint == NULL)
  {
    return false;
  }

  text = (u8 const*)*cursor;

  if (*text == 0)
  {
    return false;
  }

  if ((*text & 0x80u) == 0)
  {
    codepoint = *text;
    text += 1;
  }
  else if ((*text & 0xE0u) == 0xC0u)
  {
    if (text[1] == 0)
    {
      return false;
    }

    codepoint = ((u32)(text[0] & 0x1Fu) << 6) |
                ((u32)(text[1] & 0x3Fu));
    text += 2;
  }
  else if ((*text & 0xF0u) == 0xE0u)
  {
    if (text[1] == 0 || text[2] == 0)
    {
      return false;
    }

    codepoint = ((u32)(text[0] & 0x0Fu) << 12) |
                ((u32)(text[1] & 0x3Fu) << 6) |
                ((u32)(text[2] & 0x3Fu));
    text += 3;
  }
  else if ((*text & 0xF8u) == 0xF0u)
  {
    if (text[1] == 0 || text[2] == 0 || text[3] == 0)
    {
      return false;
    }

    codepoint = ((u32)(text[0] & 0x07u) << 18) |
                ((u32)(text[1] & 0x3Fu) << 12) |
                ((u32)(text[2] & 0x3Fu) << 6) |
                ((u32)(text[3] & 0x3Fu));
    text += 4;
  }
  else
  {
    codepoint = 0xFFFDu;
    text += 1;
  }

  *cursor = (char const*)text;
  *out_codepoint = codepoint;
  return true;
}

LDKFontFace* ldk_font_face_create(void const* data, u32 data_size)
{
  LDKFontFace* face = NULL;

  if (data == NULL || data_size == 0)
  {
    return NULL;
  }

  face = (LDKFontFace*)LDK_FONT_MALLOC(sizeof(*face));

  if (face == NULL)
  {
    return NULL;
  }

  memset(face, 0, sizeof(*face));
  face->data = (u8*)LDK_FONT_MALLOC(data_size);

  if (face->data == NULL)
  {
    LDK_FONT_FREE(face);
    return NULL;
  }

  memcpy(face->data, data, data_size);
  face->data_size = data_size;

  if (!stbtt_InitFont(&face->info, face->data, 0))
  {
    LDK_FONT_FREE(face->data);
    LDK_FONT_FREE(face);
    return NULL;
  }

  return face;
}

void ldk_font_face_destroy(LDKFontFace* face)
{
  u32 i = 0;

  if (face == NULL)
  {
    return;
  }

  for (i = 0; i < face->instance_count; ++i)
  {
    LDKFontInstance* instance = face->instances[i];
    u32 page_index = 0;

    if (instance == NULL)
    {
      continue;
    }

    for (page_index = 0; page_index < instance->page_count; ++page_index)
    {
      LDK_FONT_FREE(instance->pages[page_index].pixels);
    }

    LDK_FONT_FREE(instance->pages);
    LDK_FONT_FREE(instance->glyphs);
    LDK_FONT_FREE(instance);
  }

  LDK_FONT_FREE(face->instances);
  LDK_FONT_FREE(face->data);
  LDK_FONT_FREE(face);
}

LDKFontInstance* ldk_font_get_instance(LDKFontFace* face, float pixel_height, LDKFontAtlasDesc const* atlas_desc)
{
  u32 i = 0;
  LDKFontInstance* instance = NULL;
  int ascent = 0;
  int descent = 0;
  int line_gap = 0;
  LDKFontAtlasDesc desc;

  if (face == NULL || pixel_height <= 0.0f)
  {
    return NULL;
  }

  for (i = 0; i < face->instance_count; ++i)
  {
    if (face->instances[i] != NULL && face->instances[i]->pixel_height == pixel_height)
    {
      return face->instances[i];
    }
  }

  desc.page_width = 512;
  desc.page_height = 512;
  desc.padding = 1;

  if (atlas_desc != NULL)
  {
    if (atlas_desc->page_width > 0)
    {
      desc.page_width = atlas_desc->page_width;
    }

    if (atlas_desc->page_height > 0)
    {
      desc.page_height = atlas_desc->page_height;
    }

    desc.padding = atlas_desc->padding;
  }

  instance = (LDKFontInstance*)LDK_FONT_MALLOC(sizeof(*instance));

  if (instance == NULL)
  {
    return NULL;
  }

  memset(instance, 0, sizeof(*instance));
  instance->face = face;
  instance->pixel_height = pixel_height;
  instance->scale = stbtt_ScaleForPixelHeight(&face->info, pixel_height);
  instance->atlas_desc = desc;

  stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &line_gap);
  instance->metrics.ascent = (float)ascent * instance->scale;
  instance->metrics.descent = (float)descent * instance->scale;
  instance->metrics.line_gap = (float)line_gap * instance->scale;
  instance->metrics.line_height = instance->metrics.ascent - instance->metrics.descent + instance->metrics.line_gap;

  if (!ldk_font_grow_array((void**)&face->instances, &face->instance_capacity, face->instance_count, sizeof(LDKFontInstance*)))
  {
    LDK_FONT_FREE(instance);
    return NULL;
  }

  face->instances[face->instance_count] = instance;
  face->instance_count += 1;

  return instance;
}

float ldk_font_get_pixel_height(LDKFontInstance const* instance)
{
  if (instance == NULL)
  {
    return 0.0f;
  }

  return instance->pixel_height;
}

LDKFontMetrics ldk_font_get_metrics(LDKFontInstance const* instance)
{
  LDKFontMetrics metrics;

  memset(&metrics, 0, sizeof(metrics));

  if (instance == NULL)
  {
    return metrics;
  }

  return instance->metrics;
}

LDKGlyph const* ldk_font_get_glyph(LDKFontInstance* instance, u32 codepoint)
{
  LDKFontGlyphEntry* entry = NULL;

  if (instance == NULL)
  {
    return NULL;
  }

  entry = ldk_font_find_glyph_entry(instance, codepoint);

  if (entry != NULL)
  {
    return &entry->glyph;
  }

  if (!ldk_font_grow_array((void**)&instance->glyphs, &instance->glyph_capacity, instance->glyph_count, sizeof(LDKFontGlyphEntry)))
  {
    return NULL;
  }

  entry = &instance->glyphs[instance->glyph_count];
  memset(entry, 0, sizeof(*entry));

  if (!ldk_font_rasterize_glyph(instance, codepoint, &entry->glyph))
  {
    return NULL;
  }

  instance->glyph_count += 1;
  return &entry->glyph;
}

float ldk_font_get_kerning(LDKFontInstance const* instance, u32 left_codepoint, u32 right_codepoint)
{
  int kern = 0;

  if (instance == NULL)
  {
    return 0.0f;
  }

  kern = stbtt_GetCodepointKernAdvance(&instance->face->info, (int)left_codepoint, (int)right_codepoint);
  return (float)kern * instance->scale;
}

bool ldk_font_preload_range(LDKFontInstance* instance, u32 first_codepoint, u32 last_codepoint)
{
  u32 codepoint = 0;

  if (instance == NULL || first_codepoint > last_codepoint)
  {
    return false;
  }

  for (codepoint = first_codepoint; codepoint <= last_codepoint; ++codepoint)
  {
    if (ldk_font_get_glyph(instance, codepoint) == NULL)
    {
      return false;
    }

    if (codepoint == 0xFFFFFFFFu)
    {
      break;
    }
  }

  return true;
}

bool ldk_font_preload_basic_ascii(LDKFontInstance* instance)
{
  return ldk_font_preload_range(instance, 32u, 126u);
}

u32 ldk_font_get_page_count(LDKFontInstance const* instance)
{
  if (instance == NULL)
  {
    return 0;
  }

  return instance->page_count;
}

bool ldk_font_get_page_info(LDKFontInstance const* instance, u32 page_index, LDKFontPageInfo* out_page)
{
  LDKFontPage const* page = NULL;

  if (instance == NULL || out_page == NULL || page_index >= instance->page_count)
  {
    return false;
  }

  page = &instance->pages[page_index];
  out_page->page_index = page_index;
  out_page->width = page->width;
  out_page->height = page->height;
  out_page->pixels = page->pixels;
  out_page->dirty = page->dirty;

  return true;
}

void ldk_font_clear_page_dirty(LDKFontInstance* instance, u32 page_index)
{
  if (instance == NULL || page_index >= instance->page_count)
  {
    return;
  }

  instance->pages[page_index].dirty = false;
}

float ldk_font_measure_text_cstr(LDKFontInstance* instance, char const* text)
{
  float width = 0.0f;
  u32 prev_codepoint = 0;
  u32 codepoint = 0;
  char const* ptr;

  if (instance == NULL || text == NULL)
  {
    return 0.0f;
  }

  ptr = text;

  while (*ptr)
  {
    // --- UTF-8 decode ---
    u32 c = (unsigned char)*ptr;

    if (c < 0x80)
    {
      codepoint = c;
      ptr += 1;
    }
    else if ((c >> 5) == 0x6)
    {
      codepoint = ((c & 0x1F) << 6) |
                  (ptr[1] & 0x3F);
      ptr += 2;
    }
    else if ((c >> 4) == 0xE)
    {
      codepoint = ((c & 0x0F) << 12) |
                  ((ptr[1] & 0x3F) << 6) |
                  (ptr[2] & 0x3F);
      ptr += 3;
    }
    else if ((c >> 3) == 0x1E)
    {
      codepoint = ((c & 0x07) << 18) |
                  ((ptr[1] & 0x3F) << 12) |
                  ((ptr[2] & 0x3F) << 6) |
                  (ptr[3] & 0x3F);
      ptr += 4;
    }
    else
    {
      // invalid byte, skip
      ptr += 1;
      continue;
    }

    // --- glyph lookup ---
    LDKGlyph const* glyph = ldk_font_get_glyph(instance, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    // --- kerning ---
    if (prev_codepoint != 0)
    {
      width += ldk_font_get_kerning(instance, prev_codepoint, codepoint);
    }

    // --- advance ---
    width += (float)glyph->advance_x;

    prev_codepoint = codepoint;
  }

  return width;
}

float ldk_font_get_line_height(LDKFontInstance* instance)
{
  if (instance == NULL)
  {
    return 0.0f;
  }

  return instance->metrics.line_height;
}
