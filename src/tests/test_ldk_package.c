#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_STRING
#endif

#include <ldk_package.h>
#include <stdx/stdx_filesystem.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_write_file(const XFSPath *path, const void *data, size_t size)
{
  FILE *file = fopen(x_fs_path_cstr(path), "wb");
  if (file == NULL)
  {
    return false;
  }

  bool result = size == 0 || fwrite(data, 1, size, file) == size;
  if (fclose(file) != 0)
  {
    result = false;
  }
  return result;
}

static bool s_read_file(
    const XFSPath *path, void *data, size_t capacity, size_t *out_size)
{
  FILE *file = fopen(x_fs_path_cstr(path), "rb");
  long size;
  bool result;

  if (file == NULL || fseek(file, 0, SEEK_END) != 0)
  {
    if (file != NULL)
    {
      fclose(file);
    }
    return false;
  }

  size = ftell(file);
  if (size < 0 || (size_t)size > capacity || fseek(file, 0, SEEK_SET) != 0)
  {
    fclose(file);
    return false;
  }

  result = size == 0 || fread(data, 1, (size_t)size, file) == (size_t)size;
  if (fclose(file) != 0)
  {
    result = false;
  }

  if (result && out_size != NULL)
  {
    *out_size = (size_t)size;
  }
  return result;
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
  if (x_fs_path_exists(out_root))
  {
    if (!x_fs_directory_delete_recursive(x_fs_path_cstr(out_root)))
    {
      return false;
    }
  }

  return x_fs_directory_create_recursive(x_fs_path_cstr(out_root));
}

static int test_package_round_trip(void)
{
  static const char text[] = "box test\n";
  static const u8 binary[] = {0, 1, 2, 3, 250, 251, 252, 253};
  XFSPath root;
  XFSPath text_path;
  XFSPath binary_path;
  XFSPath empty_path;
  XFSPath box_path;
  XFSPath extracted_path;
  XFSPath extracted_root;
  XFSPath extracted_text_path;
  char read_text[sizeof(text)] = {0};
  u8 read_binary[sizeof(binary)] = {0};
  size_t read_size = 0;

  ASSERT_TRUE(strcmp(LDK_PACKAGE_FILE_EXTENSION, ".box") == 0);
  ASSERT_TRUE(s_test_root_create("ldk-package-round-trip", &root));
  ASSERT_TRUE(x_fs_path(&text_path, x_fs_path_cstr(&root), "source.txt"));
  ASSERT_TRUE(x_fs_path(&binary_path, x_fs_path_cstr(&root), "source.bin"));
  ASSERT_TRUE(x_fs_path(&empty_path, x_fs_path_cstr(&root), "empty.dat"));
  ASSERT_TRUE(x_fs_path(&box_path, x_fs_path_cstr(&root), "content.box"));
  ASSERT_TRUE(s_write_file(&text_path, text, sizeof(text) - 1));
  ASSERT_TRUE(s_write_file(&binary_path, binary, sizeof(binary)));
  ASSERT_TRUE(s_write_file(&empty_path, NULL, 0));

  LDKPackageWriter *writer =
      ldk_package_writer_create(x_fs_path_cstr(&box_path));
  ASSERT_TRUE(writer != NULL);
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&text_path),
      "docs/readme.txt", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&binary_path),
      "data/test.bin", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&empty_path),
      "empty.dat", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_TRUE(ldk_package_writer_finalize(writer));
  ldk_package_writer_destroy(writer);

  LDKPackage *package = ldk_package_open(x_fs_path_cstr(&box_path));
  ASSERT_TRUE(package != NULL);
  ASSERT_EQ(ldk_package_entry_count(package), 3u);

  const LDKPackageEntry *text_entry =
      ldk_package_entry_find(package, "docs/readme.txt");
  const LDKPackageEntry *binary_entry =
      ldk_package_entry_find(package, "data/test.bin");
  const LDKPackageEntry *empty_entry =
      ldk_package_entry_find(package, "empty.dat");
  ASSERT_TRUE(text_entry != NULL);
  ASSERT_TRUE(binary_entry != NULL);
  ASSERT_TRUE(empty_entry != NULL);
  ASSERT_TRUE(ldk_package_entry_find(package, "missing.txt") == NULL);

  ASSERT_EQ(ldk_package_entry_get_size(text_entry), sizeof(text) - 1);
  ASSERT_EQ(ldk_package_entry_get_stored_size(text_entry), sizeof(text) - 1);
  ASSERT_EQ(ldk_package_entry_get_compression(text_entry),
      LDK_PACKAGE_COMPRESSION_NONE);
  ASSERT_TRUE(strcmp(ldk_package_entry_get_path(text_entry),
                  "docs/readme.txt") == 0);
  ASSERT_TRUE(ldk_package_entry_read(
      package, text_entry, read_text, sizeof(read_text)));
  ASSERT_TRUE(memcmp(read_text, text, sizeof(text) - 1) == 0);

  ASSERT_TRUE(ldk_package_entry_read(
      package, binary_entry, read_binary, sizeof(read_binary)));
  ASSERT_TRUE(memcmp(read_binary, binary, sizeof(binary)) == 0);
  ASSERT_TRUE(ldk_package_entry_read(package, empty_entry, NULL, 0));

  for (u32 i = 0; i < ldk_package_entry_count(package); ++i)
  {
    ASSERT_TRUE(ldk_package_entry_at(package, i) != NULL);
  }
  ASSERT_TRUE(ldk_package_entry_at(package, 3) == NULL);

  ASSERT_TRUE(
      x_fs_path(&extracted_path, x_fs_path_cstr(&root), "one", "readme.txt"));
  ASSERT_TRUE(ldk_package_entry_extract(
      package, text_entry, x_fs_path_cstr(&extracted_path)));
  memset(read_text, 0, sizeof(read_text));
  ASSERT_TRUE(s_read_file(
      &extracted_path, read_text, sizeof(read_text), &read_size));
  ASSERT_EQ(read_size, sizeof(text) - 1);
  ASSERT_TRUE(memcmp(read_text, text, sizeof(text) - 1) == 0);

  ASSERT_TRUE(x_fs_path(
      &extracted_root, x_fs_path_cstr(&root), "all"));
  ASSERT_TRUE(ldk_package_extract_all(
      package, x_fs_path_cstr(&extracted_root)));
  ASSERT_TRUE(x_fs_path(&extracted_text_path, x_fs_path_cstr(&extracted_root),
      "docs", "readme.txt"));
  memset(read_text, 0, sizeof(read_text));
  ASSERT_TRUE(s_read_file(
      &extracted_text_path, read_text, sizeof(read_text), &read_size));
  ASSERT_EQ(read_size, sizeof(text) - 1);
  ASSERT_TRUE(memcmp(read_text, text, sizeof(text) - 1) == 0);

  ldk_package_close(package);
  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

static int test_package_rejects_invalid_entries(void)
{
  static const char content[] = "data";
  static const char *invalid_paths[] = {
      "",
      "/absolute.txt",
      "folder/",
      "folder//file.txt",
      "folder/./file.txt",
      "folder/../file.txt",
      "folder\\file.txt",
      "C:/file.txt",
  };
  XFSPath root;
  XFSPath source_path;
  XFSPath box_path;

  ASSERT_TRUE(s_test_root_create("ldk-package-invalid", &root));
  ASSERT_TRUE(x_fs_path(&source_path, x_fs_path_cstr(&root), "source.txt"));
  ASSERT_TRUE(x_fs_path(&box_path, x_fs_path_cstr(&root), "invalid.box"));
  ASSERT_TRUE(s_write_file(&source_path, content, sizeof(content) - 1));

  LDKPackageWriter *writer =
      ldk_package_writer_create(x_fs_path_cstr(&box_path));
  ASSERT_TRUE(writer != NULL);

  for (u32 i = 0; i < sizeof(invalid_paths) / sizeof(invalid_paths[0]); ++i)
  {
    ASSERT_FALSE(ldk_package_writer_add_file(writer,
        x_fs_path_cstr(&source_path), invalid_paths[i],
        LDK_PACKAGE_COMPRESSION_NONE));
  }

  ASSERT_FALSE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&source_path),
      "valid.txt", (LDKPackageCompression)1));
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&source_path),
      "valid.txt", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_TRUE(ldk_package_writer_finalize(writer));
  ldk_package_writer_destroy(writer);

  LDKPackage *package = ldk_package_open(x_fs_path_cstr(&box_path));
  ASSERT_TRUE(package != NULL);
  ASSERT_EQ(ldk_package_entry_count(package), 1u);
  ASSERT_TRUE(ldk_package_entry_find(package, "valid.txt") != NULL);
  ldk_package_close(package);

  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

static int test_package_rejects_duplicate_paths(void)
{
  static const char content[] = "duplicate";
  XFSPath root;
  XFSPath source_path;
  XFSPath box_path;

  ASSERT_TRUE(s_test_root_create("ldk-package-duplicate", &root));
  ASSERT_TRUE(x_fs_path(&source_path, x_fs_path_cstr(&root), "source.txt"));
  ASSERT_TRUE(x_fs_path(&box_path, x_fs_path_cstr(&root), "duplicate.box"));
  ASSERT_TRUE(s_write_file(&source_path, content, sizeof(content) - 1));

  LDKPackageWriter *writer =
      ldk_package_writer_create(x_fs_path_cstr(&box_path));
  ASSERT_TRUE(writer != NULL);
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&source_path),
      "same.txt", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_TRUE(ldk_package_writer_add_file(writer, x_fs_path_cstr(&source_path),
      "same.txt", LDK_PACKAGE_COMPRESSION_NONE));
  ASSERT_FALSE(ldk_package_writer_finalize(writer));
  ldk_package_writer_destroy(writer);
  ASSERT_FALSE(x_fs_path_exists(&box_path));

  ASSERT_TRUE(x_fs_directory_delete_recursive(x_fs_path_cstr(&root)));
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_package_round_trip),
      X_TEST(test_package_rejects_invalid_entries),
      X_TEST(test_package_rejects_duplicate_paths),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
