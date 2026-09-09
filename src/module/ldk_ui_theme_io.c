#include <module/ldk_ui.h>
#include <stdx/stdx_io.h>
#include <stdx/stdx_tml.h>

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_UI_THEME_TML_MAX_SOURCE_SIZE (1024u * 1024u)

#define S_UI_THEME_COLOR(name) {"COLOR_" #name, LDK_UI_COLOR_##name}

typedef struct LDKUIThemeColorEntry
{
  char const *name;
  LDKUIColorSlot slot;
} LDKUIThemeColorEntry;

static LDKUIThemeColorEntry const s_ui_theme_tml_colors[] = {
    S_UI_THEME_COLOR(TEXT),
    S_UI_THEME_COLOR(TEXT_DISABLED),
    S_UI_THEME_COLOR(WINDOW_BG),
    S_UI_THEME_COLOR(PANEL_BG),
    S_UI_THEME_COLOR(CONTROL_BG),
    S_UI_THEME_COLOR(CONTROL_BG_HOVERED),
    S_UI_THEME_COLOR(CONTROL_BG_ACTIVE),
    S_UI_THEME_COLOR(CONTROL_BG_ACTIVE_HOVERED),
    S_UI_THEME_COLOR(CONTROL_TEXT),
    S_UI_THEME_COLOR(CONTROL_TEXT_HOVERED),
    S_UI_THEME_COLOR(CONTROL_TEXT_ACTIVE),
    S_UI_THEME_COLOR(CONTROL_TEXT_ACTIVE_HOVERED),
    S_UI_THEME_COLOR(CONTROL_TEXT_DISABLED),
    S_UI_THEME_COLOR(CONTROL_BORDER),
    S_UI_THEME_COLOR(CONTROL_BORDER_HOVERED),
    S_UI_THEME_COLOR(CONTROL_BORDER_ACTIVE),
    S_UI_THEME_COLOR(CONTROL_BORDER_ACTIVE_HOVERED),
    S_UI_THEME_COLOR(CONTROL_BORDER_DISABLED),
    S_UI_THEME_COLOR(BORDER),
    S_UI_THEME_COLOR(FOCUS),
    S_UI_THEME_COLOR(SLIDER_TRACK),
    S_UI_THEME_COLOR(SLIDER_TRACK_HOVERED),
    S_UI_THEME_COLOR(SLIDER_TRACK_ACTIVE),
    S_UI_THEME_COLOR(SLIDER_FILL),
    S_UI_THEME_COLOR(SLIDER_THUMB),
    S_UI_THEME_COLOR(SLIDER_THUMB_HOVERED),
    S_UI_THEME_COLOR(SLIDER_THUMB_ACTIVE),
    S_UI_THEME_COLOR(TITLE),
    S_UI_THEME_COLOR(TITLE_BAR),
    S_UI_THEME_COLOR(TITLE_BAR_FOCUSED),
    S_UI_THEME_COLOR(SCROLLBAR_TRACK),
    S_UI_THEME_COLOR(SCROLLBAR_THUMB),
    S_UI_THEME_COLOR(SCROLLBAR_THUMB_HOVERED),
    S_UI_THEME_COLOR(SCROLLBAR_THUMB_ACTIVE),
    S_UI_THEME_COLOR(TAB_BAR_BG),
    S_UI_THEME_COLOR(TAB_BAR_SEPARATOR),
    S_UI_THEME_COLOR(TAB_BG),
    S_UI_THEME_COLOR(TAB_BG_HOVERED),
    S_UI_THEME_COLOR(TAB_TEXT),
    S_UI_THEME_COLOR(TAB_TEXT_HOVERED),
    S_UI_THEME_COLOR(TAB_BORDER),
    S_UI_THEME_COLOR(TAB_BORDER_HOVERED),
    S_UI_THEME_COLOR(TAB_ACTIVE_BG),
    S_UI_THEME_COLOR(TAB_ACTIVE_TEXT),
    S_UI_THEME_COLOR(TAB_ACTIVE_BORDER),
    S_UI_THEME_COLOR(SEPARATOR),
    S_UI_THEME_COLOR(INPUT_BORDER),
};

#undef S_UI_THEME_COLOR

typedef char LDKUIThemeColorSlotCoverage[
    sizeof(s_ui_theme_tml_colors) / sizeof(s_ui_theme_tml_colors[0]) ==
            LDK_UI_COLOR_COUNT
        ? 1
        : -1];

typedef enum LDKUIThemeTMLMetricType
{
  LDK_UI_THEME_TML_METRIC_FLOAT,
  LDK_UI_THEME_TML_METRIC_BOOL,
} LDKUIThemeTMLMetricType;

typedef struct LDKUIThemeMetricEntry
{
  char const *name;
  size_t offset;
  LDKUIThemeTMLMetricType type;
  bool positive;
} LDKUIThemeMetricEntry;

#define S_UI_THEME_FLOAT(name, positive)                                      \
  {#name, offsetof(LDKUITheme, name), LDK_UI_THEME_TML_METRIC_FLOAT, positive}
#define S_UI_THEME_BOOL(name)                                                 \
  {#name, offsetof(LDKUITheme, name), LDK_UI_THEME_TML_METRIC_BOOL, false}

static LDKUIThemeMetricEntry const s_ui_theme_tml_metrics[] = {
    S_UI_THEME_FLOAT(control_border_size, false),
    S_UI_THEME_FLOAT(window_border_size, false),
    S_UI_THEME_FLOAT(window_interaction_border_size, false),
    S_UI_THEME_FLOAT(slider_track_height, false),
    S_UI_THEME_FLOAT(slider_thumb_width, false),
    S_UI_THEME_BOOL(text_cursor_blink),
    S_UI_THEME_FLOAT(text_cursor_blink_interval, true),
    S_UI_THEME_FLOAT(text_cursor_width, false),
    S_UI_THEME_FLOAT(text_cursor_padding_y, false),
};

#undef S_UI_THEME_FLOAT
#undef S_UI_THEME_BOOL

typedef enum LDKUIThemeTMLSymbolKind
{
  LDK_UI_THEME_TML_SYMBOL_META,
  LDK_UI_THEME_TML_SYMBOL_METRIC,
  LDK_UI_THEME_TML_SYMBOL_COLOR,
  LDK_UI_THEME_TML_SYMBOL_ALIAS,
} LDKUIThemeTMLSymbolKind;

typedef struct LDKUIThemeTMLSymbol
{
  TMLEntry const *entry;
  u32 next;
  rgba32 color;
  u8 state;
  LDKUIThemeTMLSymbolKind kind;
  LDKUIColorSlot slot;
  LDKUIThemeMetricEntry const *metric;
} LDKUIThemeTMLSymbol;

typedef struct LDKUIThemeTMLResolver
{
  LDKUIThemeTMLSymbol *symbols;
  u32 count;
  char *error;
  size_t error_size;
} LDKUIThemeTMLResolver;

/** Write a diagnostic without requiring the caller to provide a buffer. */
static bool s_ui_theme_tml_error(
    char *error, size_t error_size, char const *format, ...)
{
  if (error != NULL && error_size > 0)
  {
    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
  }

  return false;
}

/** Compare two TML string views without relying on NUL termination. */
static int s_ui_theme_tml_name_compare(TMLString a, TMLString b)
{
  u32 size = a.size < b.size ? a.size : b.size;
  int result = memcmp(a.data, b.data, size);

  if (result != 0)
  {
    return result;
  }

  if (a.size < b.size)
  {
    return -1;
  }

  return a.size > b.size ? 1 : 0;
}

/** Compare a TML string view with a null-terminated identifier. */
static bool s_ui_theme_tml_name_equals(TMLString name, char const *text)
{
  size_t size = strlen(text);
  return name.size == size && memcmp(name.data, text, size) == 0;
}

/** Sort entries by name so duplicates and references can be found efficiently. */
static int s_ui_theme_tml_symbol_compare(void const *a, void const *b)
{
  LDKUIThemeTMLSymbol const *left = a;
  LDKUIThemeTMLSymbol const *right = b;
  return s_ui_theme_tml_name_compare(left->entry->name, right->entry->name);
}

/** Find a declared entry by name in the sorted symbol table. */
static u32 s_ui_theme_tml_symbol_find(
    LDKUIThemeTMLResolver const *resolver, TMLString name)
{
  u32 first = 0;
  u32 count = resolver->count;

  while (count > 0)
  {
    u32 step = count / 2;
    u32 middle = first + step;
    int comparison = s_ui_theme_tml_name_compare(
        resolver->symbols[middle].entry->name, name);

    if (comparison < 0)
    {
      first = middle + 1;
      count -= step + 1;
    }
    else
    {
      count = step;
    }
  }

  if (first < resolver->count &&
      s_ui_theme_tml_name_compare(
          resolver->symbols[first].entry->name, name) == 0)
  {
    return first;
  }

  return UINT32_MAX;
}

/** Identify a supported color slot by its public TML name. */
static bool s_ui_theme_tml_color_slot_find(
    TMLString name, LDKUIColorSlot *out_slot)
{
  for (u32 i = 0;
       i < sizeof(s_ui_theme_tml_colors) / sizeof(s_ui_theme_tml_colors[0]);
       ++i)
  {
    if (s_ui_theme_tml_name_equals(name, s_ui_theme_tml_colors[i].name))
    {
      *out_slot = s_ui_theme_tml_colors[i].slot;
      return true;
    }
  }

  return false;
}

/** Identify a supported numeric or boolean theme metric. */
static LDKUIThemeMetricEntry const *s_ui_theme_tml_metric_find(
    TMLString name)
{
  for (u32 i = 0;
       i < sizeof(s_ui_theme_tml_metrics) / sizeof(s_ui_theme_tml_metrics[0]);
       ++i)
  {
    if (s_ui_theme_tml_name_equals(name, s_ui_theme_tml_metrics[i].name))
    {
      return &s_ui_theme_tml_metrics[i];
    }
  }

  return NULL;
}

/** Classify an entry without interpreting its value. */
static bool s_ui_theme_tml_symbol_classify(LDKUIThemeTMLSymbol *symbol,
    char *error, size_t error_size)
{
  TMLString name = symbol->entry->name;

  if (s_ui_theme_tml_name_equals(name, "version") ||
      s_ui_theme_tml_name_equals(name, "base") ||
      s_ui_theme_tml_name_equals(name, "theme_name"))
  {
    symbol->kind = LDK_UI_THEME_TML_SYMBOL_META;
    return true;
  }

  symbol->metric = s_ui_theme_tml_metric_find(name);
  if (symbol->metric != NULL)
  {
    symbol->kind = LDK_UI_THEME_TML_SYMBOL_METRIC;
    return true;
  }

  if (name.size >= 6 && memcmp(name.data, "COLOR_", 6) == 0)
  {
    if (!s_ui_theme_tml_color_slot_find(name, &symbol->slot))
    {
      return s_ui_theme_tml_error(error, error_size,
          "Unknown UI color slot '%.*s'", (int)name.size, name.data);
    }

    symbol->kind = LDK_UI_THEME_TML_SYMBOL_COLOR;
    return true;
  }

  symbol->kind = LDK_UI_THEME_TML_SYMBOL_ALIAS;
  return true;
}

/** Extract an exact ${identifier} reference from a string value. */
static bool s_ui_theme_tml_reference_get(
    TMLString value, TMLString *out_reference)
{
  if (value.size < 4 || value.data[0] != '$' || value.data[1] != '{' ||
      value.data[value.size - 1] != '}')
  {
    return false;
  }

  TMLString reference = {value.data + 2, value.size - 3};

  for (u32 i = 0; i < reference.size; ++i)
  {
    char c = reference.data[i];
    bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 c == '_' || (i > 0 && c >= '0' && c <= '9');
    if (!valid)
    {
      return false;
    }
  }

  *out_reference = reference;
  return true;
}

/**
 * Resolve a color and all of its dependencies without recursion.
 * A visiting symbol encountered a second time indicates a reference cycle.
 */
static bool s_ui_theme_tml_color_resolve(
    LDKUIThemeTMLResolver *resolver, u32 start)
{
  u32 index = start;
  rgba32 color = 0;

  while (true)
  {
    LDKUIThemeTMLSymbol *symbol = &resolver->symbols[index];
    TMLEntry const *entry = symbol->entry;

    if (symbol->state == 2)
    {
      color = symbol->color;
      break;
    }

    if (symbol->state == 1)
    {
      return s_ui_theme_tml_error(resolver->error, resolver->error_size,
          "Cyclic color reference involving '%.*s'", (int)entry->name.size,
          entry->name.data);
    }

    symbol->state = 1;

    if (entry->type == TML_VALUE_I64)
    {
      if (entry->integer < 0 || (u64)entry->integer > UINT32_MAX)
      {
        return s_ui_theme_tml_error(resolver->error, resolver->error_size,
            "Color '%.*s' is outside the 32-bit RGBA range",
            (int)entry->name.size, entry->name.data);
      }

      color = (rgba32)entry->integer;
      symbol->next = UINT32_MAX;
      break;
    }

    if (entry->type != TML_VALUE_STRING)
    {
      return s_ui_theme_tml_error(resolver->error, resolver->error_size,
          "Color '%.*s' must be an integer or a ${...} reference",
          (int)entry->name.size, entry->name.data);
    }

    TMLString reference;
    if (!s_ui_theme_tml_reference_get(entry->string, &reference))
    {
      return s_ui_theme_tml_error(resolver->error, resolver->error_size,
          "Invalid color reference in '%.*s'", (int)entry->name.size,
          entry->name.data);
    }

    u32 next = s_ui_theme_tml_symbol_find(resolver, reference);
    if (next == UINT32_MAX)
    {
      return s_ui_theme_tml_error(resolver->error, resolver->error_size,
          "Color '%.*s' references missing entry '%.*s'",
          (int)entry->name.size, entry->name.data, (int)reference.size,
          reference.data);
    }

    LDKUIThemeTMLSymbolKind kind = resolver->symbols[next].kind;
    if (kind != LDK_UI_THEME_TML_SYMBOL_COLOR &&
        kind != LDK_UI_THEME_TML_SYMBOL_ALIAS)
    {
      return s_ui_theme_tml_error(resolver->error, resolver->error_size,
          "Color '%.*s' references a non-color entry '%.*s'",
          (int)entry->name.size, entry->name.data, (int)reference.size,
          reference.data);
    }

    symbol->next = next;
    index = next;
  }

  index = start;
  while (resolver->symbols[index].state != 2)
  {
    LDKUIThemeTMLSymbol *symbol = &resolver->symbols[index];
    symbol->color = color;
    symbol->state = 2;

    if (symbol->next == UINT32_MAX)
    {
      break;
    }

    index = symbol->next;
  }

  return true;
}

/** Read a metadata string, rejecting non-string values and excessive names. */
static bool s_ui_theme_tml_name_set(TMLEntry const *entry,
    LDKUIThemeFile *theme, char *error, size_t error_size)
{
  if (entry->type != TML_VALUE_STRING || entry->string.size == 0 ||
      entry->string.size >= sizeof(theme->name) ||
      memchr(entry->string.data, '\0', entry->string.size) != NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "theme_name must be a nonempty string shorter than %zu bytes",
        sizeof(theme->name));
  }

  memcpy(theme->name, entry->string.data, entry->string.size);
  theme->name[entry->string.size] = '\0';
  return true;
}

/** Assign a validated theme metric using its real structure offset. */
static bool s_ui_theme_tml_metric_set(TMLEntry const *entry,
    LDKUIThemeMetricEntry const *metric, LDKUITheme *theme, char *error,
    size_t error_size)
{
  char *target = (char *)theme + metric->offset;

  if (metric->type == LDK_UI_THEME_TML_METRIC_BOOL)
  {
    if (entry->type != TML_VALUE_BOOL)
    {
      return s_ui_theme_tml_error(error, error_size,
          "Metric '%s' must be a boolean", metric->name);
    }

    *(bool *)target = entry->boolean != 0;
    return true;
  }

  double value;
  if (entry->type == TML_VALUE_F64)
  {
    value = entry->number;
  }
  else if (entry->type == TML_VALUE_I64)
  {
    value = (double)entry->integer;
  }
  else
  {
    return s_ui_theme_tml_error(error, error_size,
        "Metric '%s' must be a number", metric->name);
  }

  if (!isfinite(value) || value > FLT_MAX || value < 0.0 ||
      (metric->positive && value <= 0.0))
  {
    return s_ui_theme_tml_error(error, error_size,
        "Metric '%s' must be a finite %snumber within float range",
        metric->name, metric->positive ? "positive " : "nonnegative ");
  }

  float converted = (float)value;
  if (value > 0.0 && converted == 0.0f)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Metric '%s' is too small for float precision", metric->name);
  }

  *(float *)target = converted;
  return true;
}

/** Validate the document and apply its declarations to a temporary theme. */
static bool s_ui_theme_tml_document_resolve(TMLDocument const *doc,
    LDKUIThemeFile *out_theme, char *error, size_t error_size)
{
  if (doc->root_node_count != 1)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Expected exactly one ldk_editor_theme root node");
  }

  TMLNode const *root = tml_root_node_at(doc, 0);
  if (root == NULL ||
      !s_ui_theme_tml_name_equals(root->name, "ldk_editor_theme") ||
      root->child_count != 0)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Expected a flat ldk_editor_theme root node");
  }

  u32 count = root->entry_count;
  if ((size_t)count > SIZE_MAX / sizeof(LDKUIThemeTMLSymbol))
  {
    return s_ui_theme_tml_error(error, error_size,
        "Theme contains too many entries");
  }

  LDKUIThemeTMLSymbol *symbols = calloc(
      count > 0 ? (size_t)count : 1u, sizeof(*symbols));
  if (symbols == NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Unable to allocate theme symbol table");
  }

  bool ok = false;
  LDKUIThemeTMLResolver resolver = {symbols, count, error, error_size};
  LDKUIThemeFile theme = {0};
  LDKUIThemeType base = LDK_UI_THEME_DEFAULT_DARK;

  for (u32 i = 0; i < count; ++i)
  {
    symbols[i].entry = tml_node_entry_at(doc, root, i);
    symbols[i].next = UINT32_MAX;
    if (symbols[i].entry == NULL)
    {
      s_ui_theme_tml_error(error, error_size, "Invalid theme entry");
      goto cleanup;
    }
  }

  qsort(symbols, count, sizeof(*symbols), s_ui_theme_tml_symbol_compare);

  for (u32 i = 0; i < count; ++i)
  {
    if (i > 0 && s_ui_theme_tml_name_compare(
            symbols[i - 1].entry->name, symbols[i].entry->name) == 0)
    {
      TMLString name = symbols[i].entry->name;
      s_ui_theme_tml_error(error, error_size,
          "Duplicate theme entry '%.*s'", (int)name.size, name.data);
      goto cleanup;
    }

    if (!s_ui_theme_tml_symbol_classify(&symbols[i], error, error_size))
    {
      goto cleanup;
    }
  }

  TMLString base_name = {"base", 4};
  u32 index = s_ui_theme_tml_symbol_find(&resolver, base_name);
  if (index != UINT32_MAX)
  {
    TMLEntry const *entry = symbols[index].entry;
    if (entry->type != TML_VALUE_STRING)
    {
      s_ui_theme_tml_error(error, error_size, "base must be a string");
      goto cleanup;
    }

    if (s_ui_theme_tml_name_equals(entry->string, "light"))
    {
      base = LDK_UI_THEME_DEFAULT_LIGHT;
    }
    else if (!s_ui_theme_tml_name_equals(entry->string, "dark"))
    {
      s_ui_theme_tml_error(error, error_size,
          "Unknown theme base '%.*s'", (int)entry->string.size,
          entry->string.data);
      goto cleanup;
    }
  }

  if (!ldk_ui_theme_get(base, &theme.theme))
  {
    s_ui_theme_tml_error(error, error_size,
        "Unable to initialize the built-in theme");
    goto cleanup;
  }

  snprintf(theme.name, sizeof(theme.name), "%s",
      base == LDK_UI_THEME_DEFAULT_LIGHT ? "LDK Light" : "LDK Dark");

  for (u32 i = 0; i < count; ++i)
  {
    TMLEntry const *entry = symbols[i].entry;

    if (symbols[i].kind == LDK_UI_THEME_TML_SYMBOL_METRIC)
    {
      if (!s_ui_theme_tml_metric_set(entry, symbols[i].metric,
              &theme.theme, error, error_size))
      {
        goto cleanup;
      }
    }
    else if (symbols[i].kind == LDK_UI_THEME_TML_SYMBOL_META)
    {
      if (s_ui_theme_tml_name_equals(entry->name, "theme_name"))
      {
        if (!s_ui_theme_tml_name_set(entry, &theme, error, error_size))
        {
          goto cleanup;
        }
      }
      else if (s_ui_theme_tml_name_equals(entry->name, "version"))
      {
        if (entry->type != TML_VALUE_I64 || entry->integer != 1)
        {
          s_ui_theme_tml_error(error, error_size,
              "Unsupported theme format version (expected 1)");
          goto cleanup;
        }
      }
    }
  }

  // Resolve every declaration, including aliases that are not used by a slot.
  for (u32 i = 0; i < count; ++i)
  {
    if (symbols[i].kind == LDK_UI_THEME_TML_SYMBOL_COLOR ||
        symbols[i].kind == LDK_UI_THEME_TML_SYMBOL_ALIAS)
    {
      if (!s_ui_theme_tml_color_resolve(&resolver, i))
      {
        goto cleanup;
      }
    }
  }

  // Only explicit COLOR_* declarations override the inherited palette.
  for (u32 i = 0; i < count; ++i)
  {
    if (symbols[i].kind == LDK_UI_THEME_TML_SYMBOL_COLOR)
    {
      theme.theme.colors[symbols[i].slot] = symbols[i].color;
    }
  }

  *out_theme = theme;
  ok = true;

cleanup:
  free(symbols);
  return ok;
}

bool ldk_ui_theme_tml_parse(char const *source, LDKUIThemeFile *out_theme,
    char *error, size_t error_size)
{
  if (error != NULL && error_size > 0)
  {
    error[0] = '\0';
  }

  if (source == NULL || out_theme == NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Theme source and output must not be NULL");
  }

  if (strlen(source) > LDK_UI_THEME_TML_MAX_SOURCE_SIZE)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Theme source exceeds the 1 MiB limit");
  }

  TMLParseResult result = tml_parse(source);
  if (!result.ok)
  {
    bool ok = s_ui_theme_tml_error(error, error_size,
        "TML %u:%u: %s", result.line, result.column, result.error);
    if (result.document != NULL)
    {
      tml_document_free(result.document);
    }
    return ok;
  }

  if (result.document == NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "TML parser returned no document");
  }

  LDKUIThemeFile theme;
  bool ok = s_ui_theme_tml_document_resolve(
      result.document, &theme, error, error_size);
  tml_document_free(result.document);

  if (ok)
  {
    *out_theme = theme;
  }

  return ok;
}

bool ldk_ui_theme_tml_load(char const *path, LDKUIThemeFile *out_theme,
    char *error, size_t error_size)
{
  if (error != NULL && error_size > 0)
  {
    error[0] = '\0';
  }

  if (path == NULL || path[0] == '\0' || out_theme == NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Theme path and output must not be NULL or empty");
  }

  size_t size = 0;
  char *source = x_io_read_text(path, &size);
  if (source == NULL)
  {
    return s_ui_theme_tml_error(error, error_size,
        "Unable to read theme file '%s'", path);
  }

  if (size > LDK_UI_THEME_TML_MAX_SOURCE_SIZE || strlen(source) != size)
  {
    free(source);
    return s_ui_theme_tml_error(error, error_size,
        "Theme file is too large or contains embedded NUL bytes");
  }

  bool ok = ldk_ui_theme_tml_parse(source, out_theme, error, error_size);
  free(source);
  return ok;
}
