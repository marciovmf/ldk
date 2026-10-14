#if defined(LDK_SHAREDLIB)
#define X_IMPL_FILESYSTEM
#define X_IMPL_HPOOL
#define X_IMPL_STRING
#endif

#include <module/ldk_asset_manager.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

static int test_prefab_asset_loads_and_is_shared(void)
{
  static const char prefab_source[] =
      "prefab:\n"
      "  version: 1\n"
      "  root: 0\n"
      "  entities:\n"
      "    - entity: 0\n"
      "      parent: -1\n"
      "      components:\n";
  LDKAssetSource source = {0};
  LDKAssetManager manager = {0};
  XFSPath runtree = {0};
  XFSPath path = {0};
  char name[96];
  FILE *file;

  ASSERT_TRUE(x_fs_cwd_get(&runtree) != 0);
  snprintf(name, sizeof(name), "ldk-prefab-%llu-%lu.prefab",
      (unsigned long long)time(NULL), (unsigned long)clock());
  ASSERT_TRUE(x_fs_path(&path, runtree.buf, name));
  file = fopen(path.buf, "wb");
  ASSERT_TRUE(file != NULL);
  ASSERT_EQ(fwrite(prefab_source, 1, sizeof(prefab_source) - 1u, file),
      sizeof(prefab_source) - 1u);
  ASSERT_EQ(fclose(file), 0);

  ASSERT_TRUE(ldk_asset_source_initialize(&source, runtree.buf));
  ASSERT_TRUE(ldk_asset_manager_initialize(&manager, &source, 16, 1));
  LDKAssetPrefab first =
      ldk_asset_manager_prefab_load_shared(&manager, name);
  LDKAssetPrefab second =
      ldk_asset_manager_prefab_load_shared(&manager, name);
  ASSERT_FALSE(x_handle_is_null(first.h));
  ASSERT_TRUE(ldk_asset_manager_prefab_is_alive(&manager, first));
  ASSERT_EQ(first.h.index, second.h.index);
  ASSERT_EQ(first.h.version, second.h.version);
  const LDKAssetPrefabData *data =
      ldk_asset_manager_prefab_get_const(&manager, first);
  ASSERT_TRUE(data != NULL && data->source != NULL);
  ASSERT_TRUE(strcmp(data->source, prefab_source) == 0);
  ASSERT_TRUE(x_handle_is_null(
      ldk_asset_manager_prefab_load_shared(&manager, "invalid.scene").h));

  ldk_asset_manager_terminate(&manager);
  ldk_asset_source_terminate(&source);
  ASSERT_EQ(remove(path.buf), 0);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_prefab_asset_loads_and_is_shared),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
