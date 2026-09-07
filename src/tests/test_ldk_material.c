#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
#endif

#include <ldk_material.h>
#include <module/ldk_renderer.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <stdlib.h>
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

static int test_renderer_material_lifecycle(void)
{
  LDKRenderer renderer = {0};
  LDKRendererMaterialDesc desc = {0};
  renderer.is_initialized = true;

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  desc.texture.id = 77u;
  desc.color = 0x10203040u;

  LDKResourceMaterial material = ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, material));
  ASSERT_EQ(renderer.material_count, 1u);
  ASSERT_EQ(renderer.materials[0].desc.type, LDK_MATERIAL_TYPE_VERTEX_COLOR);
  ASSERT_EQ(renderer.materials[0].desc.texture.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(renderer.materials[0].desc.color, 0x10203040u);
  ASSERT_EQ(renderer.materials[0].selection,
      LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR);
  ASSERT_NEQ(renderer.materials[0].render_key, 0u);

  renderer.default_material = material;
  ASSERT_EQ(ldk_renderer_material_default_get(&renderer).id, material.id);

  ldk_renderer_material_destroy(&renderer, material);
  ASSERT_FALSE(ldk_renderer_material_is_valid(&renderer, material));
  ASSERT_EQ(ldk_renderer_material_default_get(&renderer).id,
      LDK_RHI_INVALID_RESOURCE);
  ldk_renderer_material_destroy(&renderer, material);

  LDKResourceMaterial null_material = ldk_renderer_material_null();
  ASSERT_FALSE(ldk_renderer_material_is_valid(&renderer, null_material));

  free(renderer.materials);
  return 0;
}

static int test_renderer_mesh_submission_requires_material(void)
{
  LDKRenderer renderer = {0};
  LDKRendererMaterialDesc material_desc = {0};
  LDKResourceMesh mesh = {0};
  Mat4 world = {0};
  renderer.is_initialized = true;

  renderer.meshes =
      (LDKRendererMeshResource *)calloc(1, sizeof(LDKRendererMeshResource));
  ASSERT_TRUE(renderer.meshes != NULL);
  renderer.mesh_count = 1;
  renderer.mesh_capacity = 1;
  renderer.meshes[0].alive = true;
  renderer.meshes[0].index_count = 3;
  mesh.id = 1u;

  material_desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  material_desc.color = 0xffffffffu;
  LDKResourceMaterial material =
      ldk_renderer_material_create(&renderer, &material_desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, material));

  ASSERT_TRUE(ldk_renderer_submit_mesh(&renderer, mesh, material, world));
  ASSERT_EQ(renderer.submitted_mesh_count, 1u);
  ASSERT_EQ(renderer.submitted_meshes[0].mesh.id, mesh.id);
  ASSERT_EQ(renderer.submitted_meshes[0].material.id, material.id);
  ASSERT_EQ(renderer.submitted_meshes[0].view_id, LDK_RENDERER_VIEW_ALL);

  ASSERT_FALSE(ldk_renderer_submit_mesh(
      &renderer, mesh, ldk_renderer_material_null(), world));
  ASSERT_EQ(renderer.submitted_mesh_count, 1u);

  ldk_renderer_material_destroy(&renderer, material);
  free(renderer.submitted_meshes);
  free(renderer.materials);
  free(renderer.meshes);
  return 0;
}

static int test_renderer_textured_material(void)
{
  LDKRenderer renderer = {0};
  LDKRendererMaterialDesc desc = {0};
  renderer.is_initialized = true;

  desc.type = LDK_MATERIAL_TYPE_TEXTURED;
  desc.color = 0xffffffffu;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));

  renderer.textures = (LDKRendererTextureResource *)calloc(
      1, sizeof(LDKRendererTextureResource));
  ASSERT_TRUE(renderer.textures != NULL);
  renderer.texture_count = 1;
  renderer.texture_capacity = 1;
  renderer.textures[0].alive = true;
  desc.texture.id = 1u;

  LDKResourceMaterial material = ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, material));
  ASSERT_EQ(renderer.materials[0].desc.texture.id, 1u);
  ASSERT_EQ(renderer.materials[0].desc.color, 0xffffffffu);
  ASSERT_EQ(renderer.materials[0].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED);
  ASSERT_NEQ(renderer.materials[0].render_key, 0u);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  LDKResourceMaterial unlit_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, unlit_material));
  ASSERT_EQ(renderer.materials[1].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT);
  ASSERT_NEQ(renderer.materials[0].render_key,
      renderer.materials[1].render_key);

  ldk_renderer_material_destroy(&renderer, material);
  ldk_renderer_material_destroy(&renderer, unlit_material);
  free(renderer.materials);
  free(renderer.textures);
  return 0;
}

static int test_renderer_material_selection_and_render_key(void)
{
  LDKRenderer renderer = {0};
  LDKRendererMaterialDesc desc = {0};
  renderer.is_initialized = true;

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  desc.color = 0xffffffffu;
  LDKResourceMaterial lit_white =
      ldk_renderer_material_create(&renderer, &desc);

  desc.color = 0xff0000ffu;
  LDKResourceMaterial lit_red =
      ldk_renderer_material_create(&renderer, &desc);

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT;
  LDKResourceMaterial unlit_red =
      ldk_renderer_material_create(&renderer, &desc);

  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, lit_white));
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, lit_red));
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, unlit_red));

  LDKRendererMaterialResource* lit_white_resource =
      &renderer.materials[lit_white.id - 1u];
  LDKRendererMaterialResource* lit_red_resource =
      &renderer.materials[lit_red.id - 1u];
  LDKRendererMaterialResource* unlit_red_resource =
      &renderer.materials[unlit_red.id - 1u];

  ASSERT_EQ(lit_white_resource->selection,
      LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR);
  ASSERT_EQ(lit_red_resource->selection,
      LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR);
  ASSERT_EQ(unlit_red_resource->selection,
      LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT);
  ASSERT_EQ(lit_white_resource->render_key, lit_red_resource->render_key);
  ASSERT_NEQ(lit_white_resource->render_key, unlit_red_resource->render_key);

  free(renderer.materials);
  return 0;
}

static int test_renderer_material_rejects_invalid_input(void)
{
  LDKRenderer renderer = {0};
  LDKRendererMaterialDesc desc = {0};

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT;
  desc.color = 0xffffffffu;

  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));

  renderer.is_initialized = true;
  desc.type = LDK_MATERIAL_TYPE_INVALID;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, NULL)));
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(NULL, &desc)));

  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
      X_TEST(test_material_types),
      X_TEST(test_material_defaults),
      X_TEST(test_material_equality),
      X_TEST(test_invalid_material_descriptors),
      X_TEST(test_renderer_material_lifecycle),
      X_TEST(test_renderer_mesh_submission_requires_material),
      X_TEST(test_renderer_textured_material),
      X_TEST(test_renderer_material_selection_and_render_key),
      X_TEST(test_renderer_material_rejects_invalid_input),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
