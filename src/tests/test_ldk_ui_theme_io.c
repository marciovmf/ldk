#if defined(LDK_SHAREDLIB)
#define X_IMPL_TML
#endif

#include <module/ldk_ui.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CHECK(expr)                                                       \
  do                                                                           \
  {                                                                            \
    if (!(expr))                                                               \
    {                                                                          \
      fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr);    \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int s_test_rejected(char const *source, char const *diagnostic)
{
  LDKUIThemeFile result;
  LDKUIThemeFile original;
  char error[256];

  memset(&result, 0xa5, sizeof(result));
  original = result;
  TEST_CHECK(!ldk_ui_theme_tml_parse(source, &result, error, sizeof(error)));
  TEST_CHECK(memcmp(&result, &original, sizeof(result)) == 0);
  TEST_CHECK(error[0] != '\0');
  if (diagnostic != NULL)
  {
    TEST_CHECK(strstr(error, diagnostic) != NULL);
  }
  return 0;
}

static int test_inheritance(void)
{
  LDKUIThemeFile result;
  LDKUITheme expected;
  char error[256];
  char const *source =
      "ldk_editor_theme:\n"
      "    version: 1\n"
      "    base: \"light\"\n"
      "    theme_name: \"My Light Theme\"\n";

  TEST_CHECK(ldk_ui_theme_tml_parse(source, &result, error, sizeof(error)));
  TEST_CHECK(ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_LIGHT, &expected));
  TEST_CHECK(strcmp(result.name, "My Light Theme") == 0);
  TEST_CHECK(memcmp(&result.theme, &expected, sizeof(expected)) == 0);

  TEST_CHECK(ldk_ui_theme_tml_parse(
      "ldk_editor_theme:\n    version: 1\n", &result, error,
      sizeof(error)));
  TEST_CHECK(ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &expected));
  TEST_CHECK(strcmp(result.name, "LDK Dark") == 0);
  TEST_CHECK(memcmp(&result.theme, &expected, sizeof(expected)) == 0);
  return 0;
}

static int test_overrides_and_references(void)
{
  LDKUIThemeFile result;
  LDKUITheme expected;
  char error[256];
  char const *source =
      "ldk_editor_theme:\n"
      "    base: \"dark\"\n"
      "    COLOR_TEXT: \"${second}\"\n"
      "    second: \"${first}\"\n"
      "    first: 0x12345678\n"
      "    COLOR_CONTROL_BORDER: \"${COLOR_TEXT}\"\n"
      "    COLOR_INPUT_BORDER: \"${COLOR_TEXT}\"\n"
      "    COLOR_INPUT_BG: 0x01020304\n"
      "    COLOR_INPUT_BG_HOVERED: 0x11121314\n"
      "    COLOR_INPUT_BG_ACTIVE: 0x21222324\n"
      "    COLOR_INPUT_BG_ACTIVE_HOVERED: 0x31323334\n"
      "    control_border_size: 0.45\n"
      "    input_border_size: 1.25\n"
      "    slider_track_height: 1\n"
      "    text_cursor_blink: false\n";

  TEST_CHECK(ldk_ui_theme_tml_parse(source, &result, error, sizeof(error)));
  TEST_CHECK(ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &expected));
  expected.colors[LDK_UI_COLOR_TEXT] = 0x12345678u;
  expected.colors[LDK_UI_COLOR_CONTROL_BORDER] = 0x12345678u;
  expected.colors[LDK_UI_COLOR_INPUT_BORDER] = 0x12345678u;
  expected.colors[LDK_UI_COLOR_INPUT_BG] = 0x01020304u;
  expected.colors[LDK_UI_COLOR_INPUT_BG_HOVERED] = 0x11121314u;
  expected.colors[LDK_UI_COLOR_INPUT_BG_ACTIVE] = 0x21222324u;
  expected.colors[LDK_UI_COLOR_INPUT_BG_ACTIVE_HOVERED] = 0x31323334u;
  expected.control_border_size = 0.45f;
  expected.input_border_size = 1.25f;
  expected.slider_track_height = 1.0f;
  expected.text_cursor_blink = false;
  TEST_CHECK(memcmp(&result.theme, &expected, sizeof(expected)) == 0);
  return 0;
}

static int test_aliases_do_not_implicitly_override(void)
{
  LDKUIThemeFile result;
  LDKUITheme expected;
  char error[256];
  char const *source =
      "ldk_editor_theme:\n"
      "    text: 0x01020304\n"
      "    input_border: \"${text}\"\n";

  TEST_CHECK(ldk_ui_theme_tml_parse(source, &result, error, sizeof(error)));
  TEST_CHECK(ldk_ui_theme_get(LDK_UI_THEME_DEFAULT_DARK, &expected));
  TEST_CHECK(memcmp(&result.theme, &expected, sizeof(expected)) == 0);
  return 0;
}

static int test_invalid_documents(void)
{
  TEST_CHECK(s_test_rejected("", "root") == 0);
  TEST_CHECK(s_test_rejected("other:\n    text: 1\n", "root") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    child:\n        text: 1\n", "flat") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text = 1\n", "TML") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: 1\n    text: 2\n", "duplicate") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    base: \"other\"\n", "base") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    version: 2\n", "version") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    theme_name: \"\"\n", "theme_name") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    COLOR_NOT_A_SLOT: 1\n", "Unknown") == 0);
  return 0;
}

static int test_invalid_references(void)
{
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${missing}\"\n", "missing") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${a}\"\n", "Cyclic") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${b}\"\n    b: \"${a}\"\n",
      "Cyclic") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${base}\"\n", "missing") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    base: \"dark\"\n"
      "    a: \"${base}\"\n", "non-color") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    control_border_size: 1.0\n"
      "    a: \"${control_border_size}\"\n",
      "non-color") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${}\"\n", "Invalid") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${not-valid}\"\n", "Invalid") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    a: \"${COLOR_TEXT}\"\n", "missing") == 0);
  return 0;
}

static int test_invalid_values(void)
{
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: -1\n", "range") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: 0x100000000\n", "range") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: 1.0\n", "integer") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: true\n", "integer") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text: \"hello\"\n", "reference") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    control_border_size: -1\n", "Metric") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    input_border_size: -1\n", "Metric") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    control_border_size: \"1\"\n", "number") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    control_border_size: 1e100\n", "float") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text_cursor_blink_interval: 0\n", "positive") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    text_cursor_blink: 1\n", "boolean") == 0);
  TEST_CHECK(s_test_rejected(
      "ldk_editor_theme:\n    version: \"1\"\n", "version") == 0);
  return 0;
}

static int test_long_reference_chain(void)
{
  char *source = malloc(32768);
  size_t used = 0;
  LDKUIThemeFile result;
  char error[256];
  TEST_CHECK(source != NULL);

  used += (size_t)snprintf(source + used, 32768 - used,
      "ldk_editor_theme:\n    COLOR_TEXT: \"${alias_0}\"\n");
  for (int i = 0; i < 256; ++i)
  {
    used += (size_t)snprintf(source + used, 32768 - used,
        "    alias_%d: \"${alias_%d}\"\n", i, i + 1);
  }
  used += (size_t)snprintf(source + used, 32768 - used,
      "    alias_256: 0xAABBCCDD\n");
  TEST_CHECK(used < 32768);
  TEST_CHECK(ldk_ui_theme_tml_parse(source, &result, error, sizeof(error)));
  TEST_CHECK(result.theme.colors[LDK_UI_COLOR_TEXT] == 0xAABBCCDDu);
  free(source);
  return 0;
}

static int test_file_loading(void)
{
  char const *path = "ldk_ui_theme_tml_test.tml";
  char const *source =
      "ldk_editor_theme:\n"
      "    theme_name: \"File Theme\"\n"
      "    COLOR_TEXT: 0x01020304\n";
  LDKUIThemeFile result;
  LDKUIThemeFile original;
  char error[256];
  FILE *file = fopen(path, "wb");
  TEST_CHECK(file != NULL);
  TEST_CHECK(fwrite(source, 1, strlen(source), file) == strlen(source));
  TEST_CHECK(fclose(file) == 0);

  TEST_CHECK(ldk_ui_theme_tml_load(path, &result, error, sizeof(error)));
  TEST_CHECK(strcmp(result.name, "File Theme") == 0);
  TEST_CHECK(result.theme.colors[LDK_UI_COLOR_TEXT] == 0x01020304u);

  file = fopen(path, "wb");
  TEST_CHECK(file != NULL);
  TEST_CHECK(fwrite(source, 1, strlen(source), file) == strlen(source));
  TEST_CHECK(fputc(0, file) != EOF);
  TEST_CHECK(fclose(file) == 0);
  original = result;
  TEST_CHECK(!ldk_ui_theme_tml_load(path, &result, error, sizeof(error)));
  TEST_CHECK(memcmp(&result, &original, sizeof(result)) == 0);
  TEST_CHECK(remove(path) == 0);

  TEST_CHECK(!ldk_ui_theme_tml_load(path, &result, error, sizeof(error)));
  TEST_CHECK(memcmp(&result, &original, sizeof(result)) == 0);
  return 0;
}

static int test_invalid_arguments(void)
{
  LDKUIThemeFile result;
  char error[256];
  TEST_CHECK(!ldk_ui_theme_tml_parse(NULL, &result, error, sizeof(error)));
  TEST_CHECK(!ldk_ui_theme_tml_parse("", NULL, error, sizeof(error)));
  TEST_CHECK(!ldk_ui_theme_tml_load(NULL, &result, error, sizeof(error)));
  TEST_CHECK(!ldk_ui_theme_tml_load("", &result, error, sizeof(error)));
  return 0;
}

int main(void)
{
  int failures = 0;
  failures += test_inheritance();
  failures += test_overrides_and_references();
  failures += test_aliases_do_not_implicitly_override();
  failures += test_invalid_documents();
  failures += test_invalid_references();
  failures += test_invalid_values();
  failures += test_long_reference_chain();
  failures += test_file_loading();
  failures += test_invalid_arguments();

  if (failures == 0)
  {
    printf("LDK TML theme tests passed.\n");
  }
  return failures != 0;
}
