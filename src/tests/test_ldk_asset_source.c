#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#endif

#include <ldk_package.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_filesystem.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdio.h>
#include <string.h>

static bool s_write_file(const XFSPath *path, const void *data, size_t size)
{
  FILE *file = fopen(x_fs_path_cstr(path), "wb");
  if (!file)
  {
    return false;
  }

  bool ok = size == 0 || fwrite(data, 1, size, file) == size;
  if (fclose(file) != 0)
  {
    ok = false;
  }
  return ok;
}

static bool s_test_root_create(const char *name, XFSPath *out_root)
{
  XFSPath temp;

  if (x_fs_get_temp_folder(&temp) == 0 ||
      !x_fs_path(out_root, x_fs_path_cstr(&temp), name))
  {
    return false;
  }

  x_fs_path_normalize(out_root);
  if (x_fs_path_exists(out_root) &&
      !x_fs_directory_delete_recursive(x_fs_path_cstr(out_root)))
  {
    return false;
  }

  return x_fs_directory_create_recursive(x_fs_path_cstr(out_root));
}

static bool s_package_create(const XFSPath *box_path,
    const XFSPath *source_path, const char *package_path)
{
  LDKPackageWriter *writer =
      ldk_package_writer_create(x_fs_path_cstr(box_path));
  if (!writer)
  {
    return false;
  }

  bool ok = ldk_package_writer_add_file(writer,
                x_fs_path_cstr(source_path), package_path,
                LDK_PACKAGE_COMPRESSION_NONE) &&
            ldk_package_writer_finalize(writer);
  ldk_package_writer_destroy(writer);
  return ok;
}

static int test_asset_source_package_precedes_filesystem(void)
{
  static const char filesystem_data[] = "filesystem";
  static const char package_data[] = "package";
  XFSPath root;
  XFSPath assets;
  XFSPath local_path;
  XFSPath package_source_path;
  XFSPath box_path;
  LDKAssetSource source;
  LDKAssetSourceFile file;
  char buffer[32] = {0};

  ASSERT_TRUE(s_test_root_create("ldk-asset-source-priority", &root));
  ASSERT_TRUE(x_fs_path(&assets, x_fs_path_cstr(&root), "assets"));
  ASSERT_TRUE(x_fs_directory_create_recursive(x_fs_path_cstr(&assets)));
  ASSERT_TRUE(x_fs_path(
      &local_path, x_fs_path_cstr(&assets), "value.bin"));
  ASSERT_TRUE(x_fs_path(
      &package_source_path, x_fs_path_cstr(&root), "package-source.bin"));
  ASSERT_TRUE(x_fs_path(&box_path, x_fs_path_cstr(&root), "base.box"));
  ASSERT_TRUE(s_write_file(
      &local_path, filesystem_data, sizeof(filesystem_data) - 1));
  ASSERT_TRUE(s_write_file(
      &package_source_path, package_data, sizeof(package_data) - 1));
  ASSERT_TRUE(s_package_create(
      &box_path, &package_source_path, "assets/value.bin"));

  ASSERT_TRUE(ldk_asset_source_initialize(
      &source, x_fs_path_cstr(&root)));
  LDKAssetSourcePackage *package =
      ldk_asset_source_package_open(&source, x_fs_path_cstr(&box_path));
  ASSERT_TRUE(package != NULL);

  ASSERT_TRUE(ldk_asset_source_find(&source, "assets/value.bin", &file));
  ASSERT_EQ(file.origin, LDK_ASSET_SOURCE_ORIGIN_PACKAGE);
  ASSERT_EQ(ldk_asset_source_file_size(&file), sizeof(package_data) - 1);
  ASSERT_TRUE(ldk_asset_source_file_read(&file, buffer, sizeof(buffer)));
  ASSERT_TRUE(memcmp(buffer, package_data, sizeof(package_data) - 1) == 0);

  ASSERT_TRUE(ldk_asset_source_package_close(&source, package));
  memset(buffer, 0, sizeof(buffer));
  ASSERT_TRUE(ldk_asset_source_find(&source, "assets/value.bin", &file));
  ASSERT_EQ(file.origin, LDK_ASSET_SOURCE_ORIGIN_FILESYSTEM);
  ASSERT_EQ(ldk_asset_source_file_size(&file), sizeof(filesystem_data) - 1);
  ASSERT_TRUE(ldk_asset_source_file_read(&file, buffer, sizeof(buffer)));
  ASSERT_TRUE(
      memcmp(buffer, filesystem_data, sizeof(filesystem_data) - 1) == 0);

  ldk_asset_source_terminate(&source);
  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

static int test_asset_source_walks_open_packages(void)
{
  static const char first_data[] = "first";
  static const char second_data[] = "second";
  XFSPath root;
  XFSPath first_source;
  XFSPath second_source;
  XFSPath first_box;
  XFSPath second_box;
  LDKAssetSource source;
  LDKAssetSourceFile file;
  char buffer[16] = {0};

  ASSERT_TRUE(s_test_root_create("ldk-asset-source-chain", &root));
  ASSERT_TRUE(x_fs_path(&first_source, x_fs_path_cstr(&root), "first.dat"));
  ASSERT_TRUE(
      x_fs_path(&second_source, x_fs_path_cstr(&root), "second.dat"));
  ASSERT_TRUE(x_fs_path(&first_box, x_fs_path_cstr(&root), "first.box"));
  ASSERT_TRUE(x_fs_path(&second_box, x_fs_path_cstr(&root), "second.box"));
  ASSERT_TRUE(s_write_file(&first_source, first_data, sizeof(first_data) - 1));
  ASSERT_TRUE(
      s_write_file(&second_source, second_data, sizeof(second_data) - 1));
  ASSERT_TRUE(s_package_create(&first_box, &first_source, "first/item.dat"));
  ASSERT_TRUE(
      s_package_create(&second_box, &second_source, "second/item.dat"));

  ASSERT_TRUE(ldk_asset_source_initialize(&source, x_fs_path_cstr(&root)));
  ASSERT_TRUE(ldk_asset_source_package_open(
      &source, x_fs_path_cstr(&first_box)) != NULL);
  ASSERT_TRUE(ldk_asset_source_package_open(
      &source, x_fs_path_cstr(&second_box)) != NULL);

  ASSERT_TRUE(ldk_asset_source_find(&source, "first/item.dat", &file));
  ASSERT_EQ(file.origin, LDK_ASSET_SOURCE_ORIGIN_PACKAGE);
  ASSERT_TRUE(ldk_asset_source_file_read(&file, buffer, sizeof(buffer)));
  ASSERT_TRUE(memcmp(buffer, first_data, sizeof(first_data) - 1) == 0);

  memset(buffer, 0, sizeof(buffer));
  ASSERT_TRUE(ldk_asset_source_find(&source, "second/item.dat", &file));
  ASSERT_EQ(file.origin, LDK_ASSET_SOURCE_ORIGIN_PACKAGE);
  ASSERT_TRUE(ldk_asset_source_file_read(&file, buffer, sizeof(buffer)));
  ASSERT_TRUE(memcmp(buffer, second_data, sizeof(second_data) - 1) == 0);

  ldk_asset_source_terminate(&source);
  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

static int test_asset_source_missing_and_invalid_paths(void)
{
  XFSPath root;
  LDKAssetSource source;
  LDKAssetSourceFile file;

  ASSERT_TRUE(s_test_root_create("ldk-asset-source-invalid", &root));
  ASSERT_TRUE(ldk_asset_source_initialize(&source, x_fs_path_cstr(&root)));

  ASSERT_FALSE(ldk_asset_source_find(&source, "missing.bin", &file));
  ASSERT_FALSE(ldk_asset_source_find(&source, "../outside.bin", &file));
  ASSERT_FALSE(ldk_asset_source_find(&source, "/absolute.bin", &file));
  ASSERT_FALSE(ldk_asset_source_find(&source, "folder\\file.bin", &file));

  ldk_asset_source_terminate(&source);
  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_asset_source_package_precedes_filesystem),
      X_TEST(test_asset_source_walks_open_packages),
      X_TEST(test_asset_source_missing_and_invalid_paths),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
