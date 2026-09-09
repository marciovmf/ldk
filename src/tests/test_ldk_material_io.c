#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#define X_IMPL_STRBUILDER
#define X_IMPL_TML
#endif

#include <ldk_material_io.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <string.h>

static u32 s_diagnostics;

static void s_diagnostic(const char *message, void *user)
{
  (void)user;
  if (strstr(message, "magenta checkerboard"))
    ++s_diagnostics;
}

static bool s_read(const char *text, LDKMaterialIOContext *context,
    LDKMaterialDesc *desc, LDKMaterialIOResult *result)
{
  TMLParseResult parsed = tml_parse(text);
  if (!parsed.ok)
    return false;
  bool ok = ldk_material_desc_read(context, parsed.document,
      tml_root_node_at(parsed.document, 0), desc, result);
  tml_document_free(parsed.document);
  return ok;
}

static int test_material_io_round_trip(void)
{
  LDKMaterialIOContext context = {0};
  LDKMaterialIOResult result;
  LDKMaterialDesc desc, loaded;
  ASSERT_TRUE(s_read("material:\n  material_type: 4\n", &context,
      &desc, &result));
  ASSERT_EQ(desc.args.vertex_color.color, 0xffffffffu);
  XStrBuilder *out = x_strbuilder_create();
  ASSERT_TRUE(out != NULL);
  desc.args.vertex_color.color = 0xAABBCCFFu;
  x_strbuilder_append(out, "material:\n");
  ASSERT_TRUE(ldk_material_desc_write(&context, &desc, out, 1, &result));

  ASSERT_TRUE(strcmp(out->data,
      "material:\n  material_type: 4\n  material_color: 0xaabbccff\n") == 0);
  ASSERT_TRUE(s_read(out->data, &context, &loaded, &result));
  ASSERT_TRUE(ldk_material_desc_equal(&desc, &loaded));
  x_strbuilder_destroy(out);
  return 0;
}

static int test_material_io_invalid(void)
{
  const char *invalid[] = {
      "material:\n  material_type: 99\n",
      "material:\n  material_type: -1\n",
      "material:\n  material_type: 4\n  material_color: 4294967296\n",
      "material:\n  material_type: 4\n  material_color: -1\n",
      "material:\n  material_color: 1\n",
  };
  LDKMaterialIOContext context = {0};
  LDKMaterialIOResult result;
  LDKMaterialDesc desc, original;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &original);
  for (u32 i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
  {
    desc = original;
    ASSERT_FALSE(s_read(invalid[i], &context, &desc, &result));
    ASSERT_TRUE(result.error[0] != 0);
    ASSERT_TRUE(ldk_material_desc_equal(&desc, &original));
  }
  return 0;
}

static int test_material_io_missing_texture(void)
{
  LDKAssetManager assets = {0};
  ASSERT_TRUE(ldk_asset_manager_initialize(&assets, 16, 1));
  LDKMaterialIOContext context = {0};
  context.assets = &assets;
  context.diagnostic = s_diagnostic;
  /* This path deliberately does not exist; no checkerboard file is needed. */
#ifdef _WIN32
  x_fs_path_set(&context.runtree_path, "C:/ldk-material-io-test/runtree");
#else
  x_fs_path_set(&context.runtree_path, "/ldk-material-io-test/runtree");
#endif
  x_fs_path_normalize(&context.runtree_path);
  LDKMaterialIOResult result;
  LDKMaterialDesc first, second;
  s_diagnostics = 0;
  const char *text = "material:\n  material_type: 2\n"
      "  material_color: 0xa1b2c3ff\n"
      "  material_texture: \"missing.png\"\n";
  ASSERT_TRUE(s_read(text, &context, &first, &result));
  ASSERT_EQ(s_diagnostics, 1u);
  const LDKAssetImageData *image = ldk_asset_manager_image_get_const(
      &assets, first.args.textured.texture);
  ASSERT_TRUE(image && image->image && image->is_missing);
  ASSERT_TRUE(s_read(text, &context, &second, &result));
  ASSERT_EQ(s_diagnostics, 2u);
  ASSERT_TRUE(ldk_material_desc_equal(&first, &second));
  XStrBuilder *out = x_strbuilder_create();
  ASSERT_TRUE(out != NULL);
  x_strbuilder_append(out, "material:\n");
  ASSERT_TRUE(ldk_material_desc_write(&context, &first, out, 1, &result));
  ASSERT_TRUE(strcmp(out->data, text) == 0);
  x_strbuilder_destroy(out);
  ASSERT_TRUE(s_read("material:\n  material_type: 1\n", &context,
      &second, &result));
  ASSERT_EQ(s_diagnostics, 3u);
  ASSERT_EQ(second.args.textured.color, 0xffffffffu);
  ldk_asset_manager_terminate(&assets);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_material_io_round_trip),
      X_TEST(test_material_io_invalid),
      X_TEST(test_material_io_missing_texture),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
