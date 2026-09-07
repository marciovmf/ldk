#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
#endif

#include <ldk_material.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <string.h>

static int test_material_types(void)
{
  ASSERT_FALSE(ldk_material_type_is_valid(LDK_MATERIAL_TYPE_INVALID));
  ASSERT_TRUE(ldk_material_type_is_valid(LDK_MATERIAL_TYPE_TEXTURED_UNLIT));
  ASSERT_TRUE(ldk_material_type_is_valid(LDK_MATERIAL_TYPE_TEXTURED));
  ASSERT_TRUE(ldk_material_type_is_valid(LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT));
  ASSERT_TRUE(ldk_material_type_is_valid(LDK_MATERIAL_TYPE_VERTEX_COLOR));
  ASSERT_FALSE(ldk_material_type_is_valid((LDKMaterialType)5));
  return 0;
}

static int test_material_defaults(void)
{
  LDKMaterialDesc desc;
  LDKMaterialType types[] = {
      LDK_MATERIAL_TYPE_TEXTURED_UNLIT,
      LDK_MATERIAL_TYPE_TEXTURED,
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT,
      LDK_MATERIAL_TYPE_VERTEX_COLOR,
  };

  for (u32 i = 0; i < sizeof(types) / sizeof(types[0]); i++)
  {
    memset(&desc, 0xff, sizeof(desc));
    ASSERT_TRUE(ldk_material_desc_defaults(types[i], &desc));
    ASSERT_EQ(desc.type, types[i]);

    if (types[i] == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
        types[i] == LDK_MATERIAL_TYPE_TEXTURED)
    {
      ASSERT_EQ(desc.args.textured.texture.h.index, X_HPOOL_NULL_INDEX);
      ASSERT_EQ(desc.args.textured.texture.h.version, 0u);
      ASSERT_EQ(desc.args.textured.color, 0xffffffffu);
    }
    else
    {
      ASSERT_EQ(desc.args.vertex_color.color, 0xffffffffu);
    }
  }

  memset(&desc, 0xff, sizeof(desc));
  ASSERT_FALSE(ldk_material_desc_defaults(LDK_MATERIAL_TYPE_INVALID, &desc));
  ASSERT_EQ(desc.type, LDK_MATERIAL_TYPE_INVALID);
  ASSERT_FALSE(ldk_material_desc_is_valid(&desc));
  ASSERT_FALSE(
      ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, NULL));
  return 0;
}

static int test_material_equality(void)
{
  LDKMaterialDesc a;
  LDKMaterialDesc b;

  memset(&a, 0xaa, sizeof(a));
  memset(&b, 0xbb, sizeof(b));
  a.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  b.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  a.args.vertex_color.color = 0x10203040u;
  b.args.vertex_color.color = 0x10203040u;

  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b.args.vertex_color.color = 0x10203041u;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  ASSERT_TRUE(ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &a));
  ASSERT_TRUE(ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &b));
  a.args.textured.texture.h.index = 3u;
  a.args.textured.texture.h.version = 7u;
  b.args.textured.texture = a.args.textured.texture;

  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b.args.textured.texture.h.version++;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b = a;
  b.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  return 0;
}

static int test_invalid_material_descriptors(void)
{
  LDKMaterialDesc invalid = {0};
  LDKMaterialDesc valid;

  ASSERT_TRUE(
      ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &valid));
  ASSERT_FALSE(ldk_material_desc_equal(NULL, &valid));
  ASSERT_FALSE(ldk_material_desc_equal(&valid, NULL));
  ASSERT_FALSE(ldk_material_desc_equal(&invalid, &valid));
  ASSERT_EQ(ldk_material_desc_hash(NULL), 0u);
  ASSERT_EQ(ldk_material_desc_hash(&invalid), 0u);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_material_types),
      X_TEST(test_material_defaults),
      X_TEST(test_material_equality),
      X_TEST(test_invalid_material_descriptors),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
