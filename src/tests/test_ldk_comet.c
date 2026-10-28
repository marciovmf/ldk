/* Test the real Comet parser and generator, without a second parser. */
#define LDK_COMET_NO_MAIN
#include "../tools/ldk_tool_comet.c"

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#ifndef LDK_COMET_TEST_FIXTURE
#define LDK_COMET_TEST_FIXTURE "src/tests/comet_fixture.h"
#endif
#ifndef LDK_COMET_TEST_OUTPUT
#define LDK_COMET_TEST_OUTPUT "test_comet_generated.h"
#endif

typedef struct CometFailureCase
{
  const char *declaration;
  const char *message;
  u32 line;
} CometFailureCase;

static void test_comet_clear(LDKMetaState *state)
{
  free(state->structures);
  free(state->components);
  free(state->fields);
  free(state->groups);
  free(state->systems);
  free(state->enums);
  free(state->editor_callbacks);
  memset(state, 0, sizeof(*state));
}

static bool test_comet_parse(LDKMetaState *state, const char *text)
{
  const char *marker = strstr(text, "//@component");
  memset(state, 0, sizeof(*state));
  state->source_path = "comet_test.h";
  state->source_text = text;
  if (!marker) return false;
  ldk_meta_error_location(state, state->source_path,
      ldk_meta_line_at(text, marker));
  return ldk_meta_parse_component_at(state, marker);
}

static int test_comet_reflected_fields(void)
{
  const char *source =
      "//@component\n"
      "typedef struct TestComponent\n"
      "{\n"
      "  bool active;\n"
      "  //@inspect slider min=0 max=1\n"
      "  float alpha;\n"
      "  //@inspect label=Health readonly\n"
      "  i32 health;\n"
      "} TestComponent;\n";
  LDKMetaState state;
  ASSERT_TRUE(test_comet_parse(&state, source));
  ASSERT_EQ(state.component_count, 1u);
  ASSERT_EQ(state.field_count, 3u);
  ASSERT_TRUE(strcmp(state.fields[1].widget, "LDK_FIELD_WIDGET_SLIDER") == 0);
  ASSERT_TRUE(state.fields[1].has_min && state.fields[1].has_max);
  ASSERT_TRUE(strcmp(state.fields[2].display_name, "Health") == 0);
  ASSERT_TRUE(state.fields[2].flags & LDK_META_FLAG_READONLY);
  ASSERT_EQ(state.fields[1].source_line, 5u);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_hidden_opaque_fields(void)
{
  const char *source =
      "//@component\n"
      "typedef struct PrivateComponent\n"
      "{\n"
      "  //@inspect hidden runtime\n"
      "  InternalStruct *internal;\n"
      "  //@inspect hidden\n"
      "  InternalStruct stored;\n"
      "  float visible;\n"
      "} PrivateComponent;\n";
  LDKMetaState state;
  ASSERT_TRUE(test_comet_parse(&state, source));
  ASSERT_EQ(state.field_count, 1u);
  ASSERT_TRUE(strcmp(state.fields[0].field_name, "visible") == 0);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_unannotated_unsupported_ignored(void)
{
  const char *source =
      "//@component\n"
      "typedef struct MiscComponent\n"
      "{\n"
      "  PrivateData internal;\n"
      "  int values[4];\n"
      "  float supported;\n"
      "} MiscComponent;\n";
  LDKMetaState state;
  ASSERT_TRUE(test_comet_parse(&state, source));
  ASSERT_EQ(state.field_count, 1u);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_hidden_comment_before_field(void)
{
  const char *source =
      "//@component\n"
      "typedef struct CommentComponent\n"
      "{\n"
      "  //@inspect hidden\n"
      "  /* This comment contains a semicolon; keep the annotation. */\n"
      "  u32 secret;\n"
      "  u32 visible;\n"
      "} CommentComponent;\n";
  LDKMetaState state;
  ASSERT_TRUE(test_comet_parse(&state, source));
  ASSERT_EQ(state.field_count, 1u);
  ASSERT_TRUE(strcmp(state.fields[0].field_name, "visible") == 0);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_groups(void)
{
  const char *source =
      "//@component\n"
      "typedef struct TestComponent\n"
      "{\n"
      "  //@begin_group \"Main\"\n"
      "  float alpha;\n"
      "  //@begin_group \"Nested\"\n"
      "  bool beta;\n"
      "  //@end_group\n"
      "  //@end_group\n"
      "} TestComponent;\n";
  LDKMetaState state;
  ASSERT_TRUE(test_comet_parse(&state, source));
  ASSERT_EQ(state.group_count, 2u);
  ASSERT_EQ(state.fields[1].group_index, 1);
  ASSERT_EQ(state.groups[1].parent_index, 0);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_inspect_failures(void)
{
  static const CometFailureCase cases[] =
  {
    {"//@inspect typo=1\n  float speed;", "unknown //@inspect option", 4u},
    {"//@inspect widget=NOT_A_WIDGET\n  float speed;", "unknown //@inspect widget", 4u},
    {"//@inspect min=banana\n  float speed;", "invalid //@inspect min", 4u},
    {"//@inspect max=\n  float speed;", "invalid //@inspect max", 4u},
    {"//@inspect max_count=-1\n  float speed;", "invalid //@inspect max_count", 4u},
    {"//@inspect max_count=9999999999999999\n  float speed;", "invalid //@inspect max_count", 4u},
    {"//@inspect slider\n  UnknownType value;", "unsupported reflected type", 4u},
    {"//@inspect enum\n  UnknownEnum value;", "unsupported reflected type", 4u},
    {"//@inspect min=0\n  float samples[4];", "does not support C arrays", 4u},
    {"//@inspect readonly\n  float (*callback)(void);", "does not support C arrays", 4u},
    {"//@inspect readonly\n  float *ptr;", "unsupported pointer", 4u},
    {"//@inspect initializer=init\n  float amount;", "array-only", 4u},
    {"//@inspect enum\n  u32 amount;", "requires a reflected //@enum", 4u},
    {"//@inspect readonly\n  XArray_Item *items;", "requires element_type", 4u},
    {"//@inspect element_type=Item\n  float speed;", "element_type requires an XArray pointer", 4u},
    {"//@inspect\n  float speed;", "empty //@inspect", 4u},
    {"//@inspect readonly\n  //@inspect min=0\n  float speed;", "consecutive //@inspect", 4u},
    {"//@inspect readonly\n  //@begin_group \"New\"\n  float speed;\n  //@end_group", "not followed by a field", 4u},
    {"//@inspect readonly\n  float speed", "single-line field declaration", 4u},
    {"//@inspect readonly", "not followed by a field", 4u},
    {"//@inspect readonly\n  #if FEATURE\n  float speed;\n  #endif", "preprocessor directives", 4u},
  };
  for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    char source[1024];
    LDKMetaState state;
    snprintf(source, sizeof(source),
        "//@component\ntypedef struct TestComponent\n{\n  %s\n} TestComponent;\n",
        cases[i].declaration);
    bool ok = test_comet_parse(&state, source);
    if (ok || !strstr(state.error, cases[i].message) ||
        state.error_line != cases[i].line ||
        strcmp(state.error_path ? state.error_path : "", "comet_test.h") != 0)
    {
      fprintf(stderr, "case %u: expected '%s' at line %u, got '%s' at %u\n",
          (unsigned)i, cases[i].message, cases[i].line,
          state.error, state.error_line);
      test_comet_clear(&state);
      return 1;
    }
    test_comet_clear(&state);
  }
  return 0;
}

static int test_comet_group_failures(void)
{
  const char *source =
      "//@component\n"
      "typedef struct TestComponent\n"
      "{\n"
      "  //@end_group\n"
      "} TestComponent;\n";
  LDKMetaState state;
  ASSERT_FALSE(test_comet_parse(&state, source));
  ASSERT_TRUE(strstr(state.error, "without matching") != NULL);
  ASSERT_EQ(state.error_line, 4u);
  test_comet_clear(&state);
  source =
      "//@component\n"
      "typedef struct TestComponent\n"
      "{\n"
      "  //@begin_group \"Unfinished\"\n"
      "  float value;\n"
      "} TestComponent;\n";
  ASSERT_FALSE(test_comet_parse(&state, source));
  ASSERT_TRUE(strstr(state.error, "unclosed") != NULL);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_system_validation(void)
{
  const char *source =
      "//@system name=Demo components=LDK_COMPONENT_A,LDK_COMPONENT_A\n"
      "void demo_update(void *data, const LDKEntityGroup *group, float dt);\n";
  LDKMetaState state = {0};
  state.source_path = "comet_test.h";
  state.source_text = source;
  ldk_meta_error_location(&state, state.source_path, 1u);
  ASSERT_FALSE(ldk_meta_parse_system_at(&state, source, "comet_test.h"));
  ASSERT_TRUE(strstr(state.error, "duplicate component") != NULL);
  ASSERT_EQ(state.error_line, 1u);
  test_comet_clear(&state);

  source =
      "//@system name=Demo components=LDK_COMPONENT_A\n"
      "void demo_update(void *data, const LDKEntityGroup *group, float dt);\n";
  state.source_path = "comet_test.h";
  state.source_text = source;
  ASSERT_TRUE(ldk_meta_parse_system_at(&state, source, "comet_test.h"));
  ASSERT_EQ(state.system_count, 1u);
  ASSERT_EQ(state.systems[0].component_count, 1u);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_editor_callback_validation(void)
{
  const char *source =
      "//@editor_callback component=Unknown flags=Manual\n"
      "void incorrect(LDKEntity entity, u32 dt);\n";
  LDKMetaState state = {0};
  state.source_path = "comet_test.h";
  state.source_text = source;
  ldk_meta_error_location(&state, state.source_path, 1u);
  ASSERT_FALSE(ldk_meta_parse_editor_callback_at(&state, source));
  ASSERT_TRUE(strstr(state.error, "expected void") != NULL);
  test_comet_clear(&state);

  source =
      "//@editor_callback component=Unknown flags=Manual\n"
      "void valid_signature(LDKEntity entity, float dt);\n";
  state.source_path = "comet_test.h";
  state.source_text = source;
  ASSERT_TRUE(ldk_meta_parse_editor_callback_at(&state, source));
  ASSERT_FALSE(ldk_meta_validate_editor_callbacks(&state));
  ASSERT_TRUE(strstr(state.error, "unknown component") != NULL);
  test_comet_clear(&state);
  return 0;
}

static int test_comet_generator_fixture(void)
{
  const char *inputs[] = {LDK_COMET_TEST_FIXTURE};
  ASSERT_TRUE(ldk_meta_generate_header(inputs, 1u, LDK_COMET_TEST_OUTPUT));
  FILE *fp = fopen(LDK_COMET_TEST_OUTPUT, "rb");
  ASSERT_TRUE(fp != NULL);
  char buffer[32768];
  size_t length = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
  fclose(fp);
  buffer[length] = 0;
  ASSERT_TRUE(strstr(buffer, "LDK_COMPONENT_TestCometComponent") != NULL);
  ASSERT_TRUE(strstr(buffer, "LDK_SYSTEM_TestCometSystem") != NULL);
  ASSERT_TRUE(strstr(buffer, "ldk_array_element_TestCometElement") != NULL);
  ASSERT_TRUE(strstr(buffer, "ldk_enum_TestCometMode") != NULL);
  ASSERT_TRUE(strstr(buffer, "test_comet_editor_callback") != NULL);
  remove(LDK_COMET_TEST_OUTPUT);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_comet_reflected_fields),
    X_TEST(test_comet_hidden_opaque_fields),
    X_TEST(test_comet_unannotated_unsupported_ignored),
    X_TEST(test_comet_hidden_comment_before_field),
    X_TEST(test_comet_groups),
    X_TEST(test_comet_inspect_failures),
    X_TEST(test_comet_group_failures),
    X_TEST(test_comet_system_validation),
    X_TEST(test_comet_editor_callback_validation),
    X_TEST(test_comet_generator_fixture),
  };
  return x_tests_run(tests, (i32)(sizeof(tests) / sizeof(tests[0])), NULL);
}
