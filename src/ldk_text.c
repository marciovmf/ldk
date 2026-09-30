#include <ldk_text.h>

#include <string.h>

static bool s_text_codepoint_is_word_space(u32 codepoint)
{
  return codepoint == ' ' || codepoint == '\t' || codepoint == '\r';
}

static float s_text_codepoint_advance_get(
    LDKFontInstance *font, u32 previous_codepoint, u32 codepoint)
{
  LDKGlyph const *glyph = NULL;
  float advance = 0.0f;

  if (font == NULL)
  {
    return 0.0f;
  }

  glyph = ldk_ttf_get_glyph(font, codepoint);

  if (glyph == NULL || !glyph->valid)
  {
    return 0.0f;
  }

  if (previous_codepoint != 0)
  {
    advance += ldk_ttf_get_kerning(font, previous_codepoint, codepoint);
  }

  advance += (float)glyph->advance_x;
  return advance;
}

static char const *s_text_skip_word_spaces(char const *cursor)
{
  char const *it = cursor;

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

bool ldk_text_layout_begin_range(LDKTextLayoutIterator *iterator,
    LDKFontInstance *font, char const *text_start, char const *text_end,
    float x, float y, bool multiline)
{
  if (iterator == NULL || font == NULL || text_start == NULL ||
      text_end == NULL || text_start > text_end)
  {
    return false;
  }

  LDKFontMetrics metrics = ldk_ttf_get_metrics(font);
  *iterator = (LDKTextLayoutIterator){0};
  iterator->font = font;
  iterator->cursor = text_start;
  iterator->end = text_end;
  iterator->origin_x = x;
  iterator->pen_x = x;
  iterator->pen_y = y + metrics.ascent;
  iterator->line_height = metrics.line_height;
  iterator->multiline = multiline;
  return true;
}

bool ldk_text_layout_begin(LDKTextLayoutIterator *iterator,
    LDKFontInstance *font, char const *text, float x, float y,
    bool multiline)
{
  if (text == NULL)
  {
    return false;
  }

  return ldk_text_layout_begin_range(
      iterator, font, text, text + strlen(text), x, y, multiline);
}

bool ldk_text_layout_next(
    LDKTextLayoutIterator *iterator, LDKTextGlyphQuad *out_quad)
{
  if (iterator == NULL || out_quad == NULL || iterator->font == NULL ||
      iterator->cursor == NULL || iterator->end == NULL)
  {
    return false;
  }

  while (iterator->cursor < iterator->end && *iterator->cursor != '\0')
  {
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint_range(
            &iterator->cursor, iterator->end, &codepoint))
    {
      return false;
    }

    if (codepoint == '\n')
    {
      iterator->previous_codepoint = 0;

      if (!iterator->multiline)
      {
        iterator->cursor = iterator->end;
        return false;
      }

      iterator->pen_x = iterator->origin_x;
      iterator->pen_y += iterator->line_height;
      continue;
    }

    LDKGlyph const *glyph = ldk_ttf_get_glyph(iterator->font, codepoint);

    if (glyph == NULL || !glyph->valid)
    {
      iterator->previous_codepoint = 0;
      continue;
    }

    if (iterator->previous_codepoint != 0)
    {
      iterator->pen_x += ldk_ttf_get_kerning(iterator->font,
          iterator->previous_codepoint, codepoint);
    }

    LDKFontPageInfo page = {0};

    if (!ldk_ttf_get_page_info(iterator->font, glyph->page_index, &page) ||
        page.width == 0 || page.height == 0)
    {
      iterator->pen_x += (float)glyph->advance_x;
      iterator->previous_codepoint = codepoint;
      continue;
    }

    float x0 = iterator->pen_x + (float)glyph->offset_x;
    float y0 = iterator->pen_y + (float)glyph->offset_y;
    float x1 = x0 + (float)(glyph->atlas_x1 - glyph->atlas_x0);
    float y1 = y0 + (float)(glyph->atlas_y1 - glyph->atlas_y0);

    out_quad->glyph = glyph;
    out_quad->codepoint = codepoint;
    out_quad->page_index = glyph->page_index;
    out_quad->x0 = x0;
    out_quad->y0 = y0;
    out_quad->x1 = x1;
    out_quad->y1 = y1;
    out_quad->u0 = (float)glyph->atlas_x0 / (float)page.width;
    out_quad->v0 = (float)glyph->atlas_y0 / (float)page.height;
    out_quad->u1 = (float)glyph->atlas_x1 / (float)page.width;
    out_quad->v1 = (float)glyph->atlas_y1 / (float)page.height;

    iterator->pen_x += (float)glyph->advance_x;
    iterator->previous_codepoint = codepoint;
    return true;
  }

  return false;
}

bool ldk_text_wrapped_next_line(LDKFontInstance *font, char const *start,
    char const *text_end, float max_width, char const **out_line_start,
    char const **out_line_end, char const **out_next, float *out_width)
{
  char const *line_start = start;
  char const *cursor = NULL;
  char const *line_end = NULL;
  char const *last_break_next = NULL;
  char const *last_break_line_end = NULL;
  float width = 0.0f;
  float line_end_width = 0.0f;
  float last_break_width = 0.0f;
  u32 previous_codepoint = 0;
  bool has_visible_codepoint = false;

  if (out_line_start != NULL)
  {
    *out_line_start = start;
  }
  if (out_line_end != NULL)
  {
    *out_line_end = start;
  }
  if (out_next != NULL)
  {
    *out_next = start;
  }
  if (out_width != NULL)
  {
    *out_width = 0.0f;
  }

  if (font == NULL || start == NULL || text_end == NULL ||
      start >= text_end || *start == '\0')
  {
    return false;
  }

  if (max_width <= 0.0f)
  {
    max_width = 3.402823466e+38F;
  }

  line_start = s_text_skip_word_spaces(start);
  cursor = line_start;
  line_end = line_start;

  if (*cursor == '\n')
  {
    if (out_line_start != NULL)
    {
      *out_line_start = cursor;
    }
    if (out_line_end != NULL)
    {
      *out_line_end = cursor;
    }
    if (out_next != NULL)
    {
      *out_next = cursor + 1;
    }
    return true;
  }

  while (cursor < text_end && *cursor != '\0')
  {
    char const *before = cursor;
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint_range(
            &cursor, text_end, &codepoint))
    {
      break;
    }

    if (codepoint == '\n')
    {
      if (out_line_start != NULL)
      {
        *out_line_start = line_start;
      }
      if (out_line_end != NULL)
      {
        *out_line_end = line_end;
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

    if (s_text_codepoint_is_word_space(codepoint))
    {
      if (has_visible_codepoint)
      {
        last_break_next = s_text_skip_word_spaces(cursor);
        last_break_line_end = line_end;
        last_break_width = line_end_width;
      }

      width += s_text_codepoint_advance_get(
          font, previous_codepoint, codepoint);
      previous_codepoint = codepoint;
      continue;
    }

    float advance =
        s_text_codepoint_advance_get(font, previous_codepoint, codepoint);

    if (has_visible_codepoint && width + advance > max_width)
    {
      if (last_break_next != NULL && last_break_next > line_start)
      {
        if (out_line_start != NULL)
        {
          *out_line_start = line_start;
        }
        if (out_line_end != NULL)
        {
          *out_line_end = last_break_line_end;
        }
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

      if (out_line_start != NULL)
      {
        *out_line_start = line_start;
      }
      if (out_line_end != NULL)
      {
        *out_line_end = line_end;
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
    line_end = cursor;
    line_end_width = width;
    has_visible_codepoint = true;
  }

  if (out_line_start != NULL)
  {
    *out_line_start = line_start;
  }
  if (out_line_end != NULL)
  {
    *out_line_end = line_end;
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
