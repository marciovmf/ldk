#include <ldk_ttf.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

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

static bool ldk_ttf_grow_array(void** ptr, u32* capacity, u32 count, size_t stride)
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

static u32 ldk_ttf_round_to_u32(float value)
{
  if (value <= 0.0f)
  {
    return 0;
  }

  return (u32)(value + 0.5f);
}

static i16 ldk_ttf_round_to_i16(float value)
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

static LDKFontGlyphEntry* ldk_ttf_find_glyph_entry(LDKFontInstance const* instance, u32 codepoint)
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

static bool ldk_ttf_append_page(LDKFontInstance* instance)
{
  LDKFontPage* page = NULL;
  size_t pixel_count = 0;

  if (instance == NULL)
  {
    return false;
  }

  if (!ldk_ttf_grow_array((void**)&instance->pages, &instance->page_capacity, instance->page_count, sizeof(LDKFontPage)))
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

static bool ldk_ttf_page_pack_rect(LDKFontPage* page, u16 padding, u16 w, u16 h, u16* out_x, u16* out_y)
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

static bool ldk_ttf_store_bitmap(LDKFontPage* page, u16 dst_x, u16 dst_y, int w, int h, u8 const* src)
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

static bool ldk_ttf_rasterize_glyph(LDKFontInstance* instance, u32 codepoint, LDKGlyph* out_glyph)
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
    if (!ldk_ttf_append_page(instance))
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

    if (ldk_ttf_page_pack_rect(page, instance->atlas_desc.padding, (u16)ldk_ttf_round_to_u32((float)width), (u16)ldk_ttf_round_to_u32((float)height), &packed_x, &packed_y))
    {
      break;
    }
  }

  if (page_index == instance->page_count)
  {
    if (!ldk_ttf_append_page(instance))
    {
      if (bitmap != NULL)
      {
        stbtt_FreeBitmap(bitmap, NULL);
      }

      return false;
    }

    page = &instance->pages[instance->page_count - 1u];

    if (!ldk_ttf_page_pack_rect(page, instance->atlas_desc.padding, (u16)ldk_ttf_round_to_u32((float)width), (u16)ldk_ttf_round_to_u32((float)height), &packed_x, &packed_y))
    {
      if (bitmap != NULL)
      {
        stbtt_FreeBitmap(bitmap, NULL);
      }

      return false;
    }

    page_index = instance->page_count - 1u;
  }

  if (!ldk_ttf_store_bitmap(page, packed_x, packed_y, width, height, bitmap))
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
  glyph.atlas_x1 = (u16)((u32)packed_x + (u32)ldk_ttf_round_to_u32((float)width));
  glyph.atlas_y1 = (u16)((u32)packed_y + (u32)ldk_ttf_round_to_u32((float)height));
  glyph.offset_x = ldk_ttf_round_to_i16((float)x0);
  glyph.offset_y = ldk_ttf_round_to_i16((float)y0);
  glyph.advance_x = ldk_ttf_round_to_i16((float)advance_width * instance->scale);
  glyph.valid = true;

  *out_glyph = glyph;
  return true;
}

bool ldk_ttf_utf8_consume_codepoint_range(char const** cursor, char const* end, u32* out_codepoint)
{
  char const* ptr = NULL;
  size_t remaining = 0;
  size_t consumed = 0;
  int32_t decoded = 0;

  if (cursor == NULL || *cursor == NULL || end == NULL || out_codepoint == NULL)
  {
    return false;
  }

  ptr = *cursor;

  if (ptr >= end || *ptr == '\0')
  {
    return false;
  }

  remaining = (size_t)(end - ptr);
  decoded = x_utf8_decode(ptr, end, &consumed);

  if (decoded < 0)
  {
    if (consumed == 0)
    {
      consumed = 1;
    }

    if (consumed > remaining)
    {
      consumed = remaining;
    }

    *cursor = ptr + consumed;
    *out_codepoint = 0xFFFDu;
    return true;
  }

  if (consumed == 0 || consumed > remaining)
  {
    return false;
  }

  *cursor = ptr + consumed;
  *out_codepoint = (u32)decoded;
  return true;
}

bool ldk_ttf_utf8_consume_codepoint(char const** cursor, u32* out_codepoint)
{
  char const* ptr = NULL;
  char const* end = NULL;

  if (cursor == NULL || *cursor == NULL || out_codepoint == NULL)
  {
    return false;
  }

  ptr = *cursor;

  if (*ptr == '\0')
  {
    return false;
  }

  end = ptr + strlen(ptr);
  return ldk_ttf_utf8_consume_codepoint_range(cursor, end, out_codepoint);
}

LDKFontFace* ldk_ttf_face_create(void const* data, u32 data_size)
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

void ldk_ttf_face_destroy(LDKFontFace* face)
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

LDKFontInstance* ldk_ttf_get_instance(LDKFontFace* face, float pixel_height, LDKFontAtlasDesc const* atlas_desc)
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

  if (!ldk_ttf_grow_array((void**)&face->instances, &face->instance_capacity, face->instance_count, sizeof(LDKFontInstance*)))
  {
    LDK_FONT_FREE(instance);
    return NULL;
  }

  face->instances[face->instance_count] = instance;
  face->instance_count += 1;

  return instance;
}

float ldk_ttf_get_pixel_height(LDKFontInstance const* instance)
{
  if (instance == NULL)
  {
    return 0.0f;
  }

  return instance->pixel_height;
}

LDKFontMetrics ldk_ttf_get_metrics(LDKFontInstance const* instance)
{
  LDKFontMetrics metrics;

  memset(&metrics, 0, sizeof(metrics));

  if (instance == NULL)
  {
    return metrics;
  }

  return instance->metrics;
}

LDKGlyph const* ldk_ttf_get_glyph(LDKFontInstance* instance, u32 codepoint)
{
  LDKFontGlyphEntry* entry = NULL;

  if (instance == NULL)
  {
    return NULL;
  }

  entry = ldk_ttf_find_glyph_entry(instance, codepoint);

  if (entry != NULL)
  {
    return &entry->glyph;
  }

  if (!ldk_ttf_grow_array((void**)&instance->glyphs, &instance->glyph_capacity, instance->glyph_count, sizeof(LDKFontGlyphEntry)))
  {
    return NULL;
  }

  entry = &instance->glyphs[instance->glyph_count];
  memset(entry, 0, sizeof(*entry));

  if (!ldk_ttf_rasterize_glyph(instance, codepoint, &entry->glyph))
  {
    return NULL;
  }

  instance->glyph_count += 1;
  return &entry->glyph;
}

float ldk_ttf_get_kerning(LDKFontInstance const* instance, u32 left_codepoint, u32 right_codepoint)
{
  int kern = 0;

  if (instance == NULL)
  {
    return 0.0f;
  }

  kern = stbtt_GetCodepointKernAdvance(&instance->face->info, (int)left_codepoint, (int)right_codepoint);
  return (float)kern * instance->scale;
}

bool ldk_ttf_preload_range(LDKFontInstance* instance, u32 first_codepoint, u32 last_codepoint)
{
  u32 codepoint = 0;

  if (instance == NULL || first_codepoint > last_codepoint)
  {
    return false;
  }

  for (codepoint = first_codepoint; codepoint <= last_codepoint; ++codepoint)
  {
    if (ldk_ttf_get_glyph(instance, codepoint) == NULL)
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

bool ldk_ttf_preload_basic_ascii(LDKFontInstance* instance)
{
  return ldk_ttf_preload_range(instance, 32u, 126u);
}

u32 ldk_ttf_get_page_count(LDKFontInstance const* instance)
{
  if (instance == NULL)
  {
    return 0;
  }

  return instance->page_count;
}

bool ldk_ttf_get_page_info(LDKFontInstance const* instance, u32 page_index, LDKFontPageInfo* out_page)
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

void ldk_ttf_clear_page_dirty(LDKFontInstance* instance, u32 page_index)
{
  if (instance == NULL || page_index >= instance->page_count)
  {
    return;
  }

  instance->pages[page_index].dirty = false;
}

static bool ldk_ttf_codepoint_is_word_space(u32 codepoint)
{
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\r';
}

static float ldk_ttf_codepoint_advance(LDKFontInstance* instance, u32 previous_codepoint, u32 codepoint)
{
  LDKGlyph const* glyph = NULL;
  float advance = 0.0f;

  if (instance == NULL)
  {
    return 0.0f;
  }

  glyph = ldk_ttf_get_glyph(instance, codepoint);

  if (glyph == NULL || !glyph->valid)
  {
    return 0.0f;
  }

  if (previous_codepoint != 0)
  {
    advance += ldk_ttf_get_kerning(instance, previous_codepoint, codepoint);
  }

  advance += (float)glyph->advance_x;

  return advance;
}

static char const* ldk_ttf_skip_word_spaces(char const* cursor)
{
  char const* it = cursor;

  if (it == NULL)
  {
    return NULL;
  }

  while (*it == ' ' || *it == '\t' || *it == '\r')
  {
    it += 1;
  }

  return it;
}

static bool ldk_ttf_wrapped_next_line(LDKFontInstance* instance, char const* start, float max_width,
    char const** out_next, float* out_width)
{
  char const* line_start = start;
  char const* cursor = NULL;
  char const* last_break_next = NULL;
  float width = 0.0f;
  float line_end_width = 0.0f;
  float last_break_width = 0.0f;
  u32 previous_codepoint = 0;
  bool has_visible_codepoint = false;

  if (out_next != NULL)
  {
    *out_next = start;
  }

  if (out_width != NULL)
  {
    *out_width = 0.0f;
  }

  if (instance == NULL || start == NULL || *start == '\0')
  {
    return false;
  }

  line_start = ldk_ttf_skip_word_spaces(start);
  cursor = line_start;

  if (*cursor == '\n')
  {
    if (out_next != NULL)
    {
      *out_next = cursor + 1;
    }

    return true;
  }

  while (*cursor != '\0')
  {
    char const* before = cursor;
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint(&cursor, &codepoint))
    {
      break;
    }

    if (codepoint == '\n')
    {
      if (out_next != NULL)
      {
        *out_next = cursor;
      }

      if (out_width != NULL)
      {
        *out_width = line_end_width;
      }

      return true;
    }

    if (ldk_ttf_codepoint_is_word_space(codepoint))
    {
      if (has_visible_codepoint)
      {
        last_break_next = ldk_ttf_skip_word_spaces(cursor);
        last_break_width = line_end_width;
      }

      width += ldk_ttf_codepoint_advance(instance, previous_codepoint, codepoint);
      previous_codepoint = codepoint;
      continue;
    }

    float advance = ldk_ttf_codepoint_advance(instance, previous_codepoint, codepoint);

    if (has_visible_codepoint && width + advance > max_width)
    {
      if (last_break_next != NULL && last_break_next > line_start)
      {
        if (out_next != NULL)
        {
          *out_next = last_break_next;
        }

        if (out_width != NULL)
        {
          *out_width = last_break_width;
        }

        return true;
      }

      if (out_next != NULL)
      {
        *out_next = before;
      }

      if (out_width != NULL)
      {
        *out_width = line_end_width;
      }

      return true;
    }

    width += advance;
    previous_codepoint = codepoint;
    line_end_width = width;
    has_visible_codepoint = true;
  }

  if (out_next != NULL)
  {
    *out_next = cursor;
  }

  if (out_width != NULL)
  {
    *out_width = line_end_width;
  }

  return true;
}

LDKSizef ldk_ttf_measure_text_cstr_wrapped(LDKFontInstance* instance, char const* text, float max_width)
{
  LDKSizef size;

  size.w = 0.0f;
  size.h = 0.0f;

  if (instance == NULL || text == NULL)
  {
    return size;
  }

  if (max_width <= 0.0f)
  {
    return ldk_ttf_measure_text_cstr(instance, text);
  }

  float line_height = ldk_ttf_get_line_height(instance);
  char const* cursor = text;

  while (cursor != NULL && *cursor != '\0')
  {
    char const* next = NULL;
    float line_width = 0.0f;

    if (!ldk_ttf_wrapped_next_line(instance, cursor, max_width, &next, &line_width))
    {
      break;
    }

    if (line_width > size.w)
    {
      size.w = line_width;
    }

    size.h += line_height;

    if (next == NULL || next <= cursor)
    {
      break;
    }

    cursor = next;
  }

  if (size.h == 0.0f)
  {
    size.h = line_height;
  }

  return size;
}

LDKTextSize ldk_ttf_measure_text_cstr(LDKFontInstance* instance, char const* text)
{
  if (text == NULL)
  {
    return (LDKTextSize){0};
  }

  return ldk_ttf_measure_text_cstrn(instance, text, (u32)strlen(text));
}

LDKTextSize ldk_ttf_measure_text_cstrn(LDKFontInstance* instance, char const* text, u32 byte_count)
{
  LDKSizef size = {0};

  float line_width = 0.0f;
  u32 prev_codepoint = 0;
  u32 codepoint = 0;

  if (instance == NULL || text == NULL)
  {
    return size;
  }

  float line_height = ldk_ttf_get_line_height(instance);
  size.h = line_height;

  char const* ptr = text;
  char const* end = ptr + byte_count;

  while (*ptr != '\0' && ptr < end)
  {
    if (!ldk_ttf_utf8_consume_codepoint_range(&ptr, end, &codepoint))
    {
      break;
    }

    if (codepoint == '\n')
    {
      if (line_width > size.w)
      {
        size.w = line_width;
      }

      line_width = 0.0f;
      prev_codepoint = 0;
      size.h += line_height;

      continue;
    }

    if (codepoint == '\r')
    {
      continue;
    }

    LDKGlyph const* glyph = ldk_ttf_get_glyph(instance, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      prev_codepoint = 0;
      continue;
    }

    if (prev_codepoint != 0)
    {
      line_width += ldk_ttf_get_kerning(instance, prev_codepoint, codepoint);
    }

    line_width += (float)glyph->advance_x;
    prev_codepoint = codepoint;
  }

  if (line_width > size.w)
  {
    size.w = line_width;
  }

  return size;
}

float ldk_ttf_get_line_height(LDKFontInstance* instance)
{
  if (instance == NULL)
  {
    return 0.0f;
  }

  return instance->metrics.line_height;
}
