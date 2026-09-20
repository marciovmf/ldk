#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
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
static u32 s_neutral_diagnostics;

static void s_diagnostic(const char *message, void *user)
{
  (void)user;
  if (strstr(message, "magenta checkerboard"))
    ++s_diagnostics;
  if (strstr(message, "neutral fallback"))
    ++s_neutral_diagnostics;
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
  ASSERT_EQ(desc.surface.specular, 0.0f);
  ASSERT_EQ(desc.surface.shininess, 32.0f);
  ASSERT_EQ(desc.surface.emission, 0.0f);
  ASSERT_TRUE(x_handle_is_null(desc.surface.normal_map.h));
  ASSERT_TRUE(x_handle_is_null(desc.surface.specular_map.h));

  ASSERT_TRUE(s_read("material:\n  material_type: 4\n  material_shininess: 0\n",
      &context, &loaded, &result));
  ASSERT_EQ(loaded.surface.shininess, 32.0f);

  XStrBuilder *out = x_strbuilder_create();
  ASSERT_TRUE(out != NULL);
  desc.args.vertex_color.color = 0xAABBCCFFu;
  desc.surface.specular = 0.25f;
  desc.surface.shininess = 64.0f;
  desc.surface.emission = 0.5f;
  x_strbuilder_append(out, "material:\n");
  ASSERT_TRUE(ldk_material_desc_write(&context, &desc, out, 1, &result));

  ASSERT_TRUE(strcmp(out->data,
      "material:\n"
      "  material_type: 4\n"
      "  material_color: 0xaabbccff\n"
      "  material_specular: 0.25\n"
      "  material_shininess: 64\n"
      "  material_emission: 0.5\n") == 0);
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
      "material:\n  material_type: 4\n  material_specular: -0.1\n",
      "material:\n  material_type: 4\n  material_shininess: -1\n",
      "material:\n  material_type: 4\n  material_emission: -1\n",
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
  s_neutral_diagnostics = 0;
  const char *text = "material:\n  material_type: 2\n"
                     "  material_color: 0xa1b2c3ff\n"
                     "  material_texture: \"missing.png\"\n"
                     "  material_normal_map: \"missing_normal.png\"\n"
                     "  material_specular_map: \"missing_specular.png\"\n";
  ASSERT_TRUE(s_read(text, &context, &first, &result));
  ASSERT_EQ(s_diagnostics, 1u);
  ASSERT_EQ(s_neutral_diagnostics, 2u);
  ASSERT_EQ(first.surface.specular, 0.0f);
  ASSERT_EQ(first.surface.shininess, 32.0f);
  ASSERT_EQ(first.surface.emission, 0.0f);
  const LDKAssetImageData *image = ldk_asset_manager_image_get_const(
      &assets, first.args.textured.texture);
  ASSERT_TRUE(image && image->image && image->is_missing);
  const LDKAssetImageData *normal =
      ldk_asset_manager_image_get_const(&assets, first.surface.normal_map);
  const LDKAssetImageData *specular =
      ldk_asset_manager_image_get_const(&assets, first.surface.specular_map);
  ASSERT_TRUE(normal && normal->is_missing);
  ASSERT_TRUE(specular && specular->is_missing);
  ASSERT_TRUE(s_read(text, &context, &second, &result));
  ASSERT_EQ(s_diagnostics, 2u);
  ASSERT_EQ(s_neutral_diagnostics, 4u);
  ASSERT_TRUE(ldk_material_desc_equal(&first, &second));
  XStrBuilder *out = x_strbuilder_create();
  ASSERT_TRUE(out != NULL);
  x_strbuilder_append(out, "material:\n");
  ASSERT_TRUE(ldk_material_desc_write(&context, &first, out, 1, &result));
  ASSERT_TRUE(strcmp(out->data,
                  "material:\n  material_type: 2\n"
                  "  material_color: 0xa1b2c3ff\n"
                  "  material_texture: \"missing.png\"\n"
                  "  material_specular: 0\n"
                  "  material_shininess: 32\n"
                  "  material_emission: 0\n"
                  "  material_normal_map: \"missing_normal.png\"\n"
                  "  material_specular_map: \"missing_specular.png\"\n") == 0);
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
