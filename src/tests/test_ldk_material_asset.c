#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#define X_IMPL_HPOOL
#endif

#include <ldk_material_asset.h>
#include <component/ldk_mesh_source.h>
#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

static u32 s_diagnostics;
static u32 s_neutral_diagnostics;
static void s_diagnostic(const char *message, void *user)
{
  (void)user;
  if (strstr(message, "magenta checkerboard") ||
      strstr(message, "unlit magenta checker material"))
    ++s_diagnostics;
  if (strstr(message, "neutral fallback"))
    ++s_neutral_diagnostics;
}

static int test_material_asset_lifecycle(void)
{
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  XFSPath runtree = {0};
  ASSERT_TRUE(x_fs_cwd_get(&runtree) != 0);
  ASSERT_TRUE(ldk_asset_source_initialize(&source, runtree.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16, 1));
  LDKMaterialIOContext context = {0};
  context.assets = &manager;
  context.diagnostic = s_diagnostic;
  char name[96];
  snprintf(name, sizeof(name), "ldk-material-test-%llu-%lu.tml",
      (unsigned long long)time(NULL), (unsigned long)clock());
  XFSPath path = {0};
  x_fs_path(&path, runtree.buf, name);
  ASSERT_FALSE(x_fs_path_exists(&path));
  LDKMaterialIOResult result;
  LDKMaterialDesc desc;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &desc);
  LDKAssetMaterial asset = ldk_asset_manager_material_create(
      &context, name, &desc, &result);
  ASSERT_FALSE(x_handle_is_null(asset.h));
  const LDKAssetMaterialData *data =
      ldk_asset_manager_material_get_const(&manager, asset);
  ASSERT_TRUE(data != NULL && data->dirty);
  ASSERT_EQ(data->revision, 1u);
  LDKAssetMaterial shared = ldk_asset_manager_material_load_shared(
      &context, name, &result);
  ASSERT_EQ(shared.h.index, asset.h.index);
  ASSERT_EQ(shared.h.version, asset.h.version);
  ASSERT_TRUE(ldk_asset_manager_material_update(&manager, asset, &desc));
  ASSERT_EQ(data->revision, 1u);
  LDKMeshSource first = {0}, second = {0};
  ASSERT_TRUE(ldk_mesh_source_set_material_asset(&first, &manager, asset));
  ASSERT_TRUE(ldk_mesh_source_set_material_asset(&second, &manager, shared));
  first.material_dirty = second.material_dirty = false;
  first.dirty = second.dirty = false;
  desc.args.vertex_color.color = 0x12345678u;
  ASSERT_TRUE(ldk_asset_manager_material_update(&manager, shared, &desc));
  ASSERT_EQ(data->revision, 2u);
  ASSERT_TRUE(ldk_mesh_source_material_sync(&first, &manager));
  ASSERT_TRUE(ldk_mesh_source_material_sync(&second, &manager));
  ASSERT_EQ(first.material.args.vertex_color.color, 0x12345678u);
  ASSERT_EQ(second.material.args.vertex_color.color, 0x12345678u);
  ASSERT_TRUE(first.material_dirty && second.material_dirty);
  ASSERT_FALSE(first.dirty || second.dirty);
  first.material_dirty = false;
  ASSERT_TRUE(ldk_mesh_source_material_sync(&first, &manager));
  ASSERT_FALSE(first.material_dirty);
  ASSERT_TRUE(ldk_mesh_source_set_material(&first, &desc));
  ASSERT_EQ(first.material_revision, 0u);
  ASSERT_EQ(data->descriptor.args.vertex_color.color, 0x12345678u);
  ASSERT_TRUE(ldk_asset_manager_material_save(&context, asset, &result));
  ASSERT_FALSE(data->dirty);
  ASSERT_EQ(data->revision, 2u);
  ASSERT_TRUE(ldk_asset_manager_material_update(&manager, asset, &desc));
  ASSERT_FALSE(data->dirty);
  LDKMaterialDesc invalid = {0};
  ASSERT_FALSE(ldk_asset_manager_material_update(&manager, asset, &invalid));
  ASSERT_EQ(data->revision, 2u);
  ASSERT_TRUE(x_handle_is_null(ldk_asset_manager_material_create(
      &context, name, &desc, &result).h));

  /* A cached load must not discard unsaved edits. */
  desc.args.vertex_color.color = 0xabcdef01u;
  ASSERT_TRUE(ldk_asset_manager_material_update(&manager, asset, &desc));
  ASSERT_TRUE(ldk_mesh_source_material_sync(&first, &manager));
  ASSERT_TRUE(ldk_mesh_source_material_sync(&second, &manager));
  ASSERT_EQ(first.material.args.vertex_color.color, 0x12345678u);
  ASSERT_EQ(second.material.args.vertex_color.color, 0xabcdef01u);
  shared = ldk_asset_manager_material_load_shared(&context, name, &result);
  ASSERT_TRUE(ldk_asset_manager_material_get_const(&manager, shared) == data);
  ASSERT_EQ(data->descriptor.args.vertex_color.color, 0xabcdef01u);
  ldk_asset_manager_clear(&manager);
  ASSERT_TRUE(ldk_asset_manager_material_get_const(&manager, asset) == NULL);
  asset = ldk_asset_manager_material_load_shared(&context, name, &result);
  data = ldk_asset_manager_material_get_const(&manager, asset);
  ASSERT_TRUE(data != NULL && !data->dirty);
  ASSERT_EQ(data->revision, 1u);
  ASSERT_EQ(data->descriptor.args.vertex_color.color, 0x12345678u);

  /* Missing texture/map authored paths survive save and reload. */
  XFSPath image_path = {0};
  XFSPath normal_path = {0};
  XFSPath specular_path = {0};
  char image_name[110];
  char normal_name[110];
  char specular_name[110];
  snprintf(image_name, sizeof(image_name), "%s.png", name);
  snprintf(normal_name, sizeof(normal_name), "%s-normal.png", name);
  snprintf(specular_name, sizeof(specular_name), "%s-specular.png", name);
  x_fs_path(&image_path, runtree.buf, image_name);
  x_fs_path(&normal_path, runtree.buf, normal_name);
  x_fs_path(&specular_path, runtree.buf, specular_name);
  ASSERT_FALSE(x_fs_path_exists(&image_path));
  ASSERT_FALSE(x_fs_path_exists(&normal_path));
  ASSERT_FALSE(x_fs_path_exists(&specular_path));
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &desc);
  desc.args.textured.texture =
      ldk_asset_manager_image_missing(&manager, image_name);
  desc.surface.normal_map =
      ldk_asset_manager_image_missing(&manager, normal_name);
  desc.surface.specular_map =
      ldk_asset_manager_image_missing(&manager, specular_name);
  ASSERT_FALSE(x_handle_is_null(desc.args.textured.texture.h));
  ASSERT_FALSE(x_handle_is_null(desc.surface.normal_map.h));
  ASSERT_FALSE(x_handle_is_null(desc.surface.specular_map.h));
  ASSERT_TRUE(ldk_asset_manager_material_update(&manager, asset, &desc));
  ASSERT_TRUE(ldk_asset_manager_material_save(&context, asset, &result));
  ldk_asset_manager_clear(&manager);
  s_diagnostics = 0;
  s_neutral_diagnostics = 0;
  asset = ldk_asset_manager_material_load_shared(&context, name, &result);
  data = ldk_asset_manager_material_get_const(&manager, asset);
  ASSERT_TRUE(data != NULL);
  ASSERT_EQ(s_diagnostics, 1u);
  ASSERT_EQ(s_neutral_diagnostics, 2u);
  const LDKAssetImageData *image = ldk_asset_manager_image_get_const(
      &manager, data->descriptor.args.textured.texture);
  const LDKAssetImageData *normal = ldk_asset_manager_image_get_const(
      &manager, data->descriptor.surface.normal_map);
  const LDKAssetImageData *specular = ldk_asset_manager_image_get_const(
      &manager, data->descriptor.surface.specular_map);
  ASSERT_TRUE(image != NULL && image->is_missing);
  ASSERT_TRUE(normal != NULL && normal->is_missing);
  ASSERT_TRUE(specular != NULL && specular->is_missing);
  ASSERT_TRUE(ldk_asset_manager_material_save(&context, asset, &result));
  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  ASSERT_EQ(remove(path.buf), 0);
  return 0;
}

static int test_material_asset_failures(void)
{
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  XFSPath runtree = {0};
  ASSERT_TRUE(x_fs_cwd_get(&runtree) != 0);
  ASSERT_TRUE(ldk_asset_source_initialize(&source, runtree.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16, 1));
  LDKMaterialIOContext context = {0};
  context.assets = &manager;
  LDKMaterialIOResult result;
  ASSERT_TRUE(x_handle_is_null(ldk_asset_manager_material_load_shared(
      &context, "../outside.tml", &result).h));
  ASSERT_TRUE(result.error[0] != 0);
  char name[96];
  snprintf(name, sizeof(name), "ldk-material-bad-%llu-%lu.tml",
      (unsigned long long)time(NULL), (unsigned long)clock());
  XFSPath path = {0};
  x_fs_path(&path, runtree.buf, name);
  ASSERT_FALSE(x_fs_path_exists(&path));
  context.diagnostic = s_diagnostic;
  s_diagnostics = 0;
  LDKAssetMaterial missing = ldk_asset_manager_material_load_shared(
      &context, name, &result);
  ASSERT_FALSE(x_handle_is_null(missing.h));
  ASSERT_EQ(s_diagnostics, 1u);
  const LDKAssetMaterialData *fallback =
      ldk_asset_manager_material_get_const(&manager, missing);
  ASSERT_TRUE(fallback && fallback->is_missing && !fallback->dirty);
  ASSERT_EQ(fallback->descriptor.type, LDK_MATERIAL_TYPE_TEXTURED_UNLIT);
  ASSERT_EQ(fallback->descriptor.args.textured.color, 0xffffffffu);
  const LDKAssetImageData *checker = ldk_asset_manager_image_get_const(
      &manager, fallback->descriptor.args.textured.texture);
  ASSERT_TRUE(checker && checker->image && checker->is_missing);
  LDKAssetImage default_checker = ldk_asset_manager_image_missing(&manager, NULL);
  ASSERT_EQ(default_checker.h.index, fallback->descriptor.args.textured.texture.h.index);
  ASSERT_EQ(default_checker.h.version, fallback->descriptor.args.textured.texture.h.version);
  LDKAssetMaterial cached = ldk_asset_manager_material_load_shared(
      &context, name, &result);
  ASSERT_EQ(cached.h.index, missing.h.index);
  ASSERT_EQ(cached.h.version, missing.h.version);
  ASSERT_EQ(s_diagnostics, 2u);
  LDKAssetHandle handle = {missing.h};
  ASSERT_TRUE(strcmp(ldk_asset_get_info_const(&manager, handle)->asset_path.buf,
      name) == 0);
  ASSERT_FALSE(ldk_asset_manager_material_save(&context, missing, &result));
  ASSERT_FALSE(x_fs_path_exists(&path));
  ASSERT_FALSE(ldk_asset_manager_material_update(
      &manager, missing, &fallback->descriptor));
  ldk_asset_manager_clear(&manager);
  FILE *file = fopen(path.buf, "wb");
  ASSERT_TRUE(file != NULL);
  fputs("material:\n  material_type: 99\n", file);
  ASSERT_EQ(fclose(file), 0);
  ASSERT_TRUE(x_handle_is_null(ldk_asset_manager_material_load_shared(
      &context, name, &result).h));
  ASSERT_EQ(ldk_asset_alive_count(&manager), 0u);
  ASSERT_EQ(remove(path.buf), 0);
  LDKMaterialDesc desc;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &desc);
  /* Parent directory does not exist: failed save must retain dirty state. */
  char child[120];
  snprintf(child, sizeof(child), "%s/material.tml", name);
  LDKAssetMaterial asset = ldk_asset_manager_material_create(
      &context, child, &desc, &result);
  ASSERT_FALSE(x_handle_is_null(asset.h));
  ASSERT_FALSE(ldk_asset_manager_material_save(&context, asset, &result));
  ASSERT_TRUE(result.error[0] != 0);
  const LDKAssetMaterialData *data =
      ldk_asset_manager_material_get_const(&manager, asset);
  ASSERT_TRUE(data->dirty);
  ASSERT_EQ(data->revision, 1u);
  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_material_asset_lifecycle),
      X_TEST(test_material_asset_failures),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
