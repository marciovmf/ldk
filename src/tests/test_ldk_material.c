#if defined(LDK_SHAREDLIB)
#define X_IMPL_HPOOL
#define X_IMPL_FILESYSTEM
#define X_IMPL_MATH
#endif

#include <ldk_material.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_instanced_mesh_source.h>
#include <math.h>
#include <module/ldk_renderer.h>
#include <stdx/stdx_math.h>

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
    ASSERT_EQ(desc.surface.specular, 0.0f);
    ASSERT_EQ(desc.surface.shininess, 32.0f);
    ASSERT_EQ(desc.surface.emission, 0.0f);
    ASSERT_EQ(desc.surface.normal_map.h.index, X_HPOOL_NULL_INDEX);
    ASSERT_EQ(desc.surface.normal_map.h.version, 0u);
    ASSERT_EQ(desc.surface.specular_map.h.index, X_HPOOL_NULL_INDEX);
    ASSERT_EQ(desc.surface.specular_map.h.version, 0u);

    if (types[i] == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
        types[i] == LDK_MATERIAL_TYPE_TEXTURED)
    {
      ASSERT_EQ(desc.args.textured.texture.h.index, X_HPOOL_NULL_INDEX);
      ASSERT_EQ(desc.args.textured.texture.h.version, 0u);
      ASSERT_EQ(desc.args.textured.color, 0xffffffffu);
      ASSERT_EQ(desc.args.textured.alpha_mode, LDK_MATERIAL_ALPHA_MODE_OPAQUE);
      ASSERT_EQ(desc.args.textured.alpha_cutoff, 0.5f);
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
  a.surface.specular = b.surface.specular = 0.0f;
  a.surface.shininess = b.surface.shininess = 32.0f;
  a.surface.emission = b.surface.emission = 0.0f;
  a.surface.normal_map.h = b.surface.normal_map.h = x_handle_null();
  a.surface.specular_map.h = b.surface.specular_map.h = x_handle_null();

  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b.surface.shininess = 0.0f;
  ASSERT_TRUE(ldk_material_desc_is_valid(&b));
  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  b.surface.shininess = a.surface.shininess;
  b.surface.specular = -0.0f;
  b.surface.emission = -0.0f;
  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b.args.vertex_color.color = 0x10203041u;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b = a;
  b.surface.specular = 0.5f;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  b = a;
  b.surface.shininess = 64.0f;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  b = a;
  b.surface.emission = 0.25f;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  b = a;
  b.surface.normal_map.h.index = 9u;
  b.surface.normal_map.h.version = 2u;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  b = a;
  b.surface.specular_map.h.index = 10u;
  b.surface.specular_map.h.version = 3u;
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
  b.args.textured.alpha_cutoff = 0.75f;
  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b = a;
  b.args.textured.alpha_mode = LDK_MATERIAL_ALPHA_MODE_CUTOUT;
  b.args.textured.alpha_cutoff = 0.25f;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
  a = b;
  b.args.textured.alpha_cutoff = 0.75f;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  b = a;
  b.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  ASSERT_FALSE(ldk_material_desc_equal(&a, &b));
  ASSERT_NEQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));

  ASSERT_TRUE(ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, &a));
  b = a;
  b.surface.specular = 3.0f;
  b.surface.shininess = 128.0f;
  b.surface.emission = 2.0f;
  b.surface.normal_map.h.index = 11u;
  b.surface.specular_map.h.index = 12u;
  ASSERT_TRUE(ldk_material_desc_equal(&a, &b));
  ASSERT_EQ(ldk_material_desc_hash(&a), ldk_material_desc_hash(&b));
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

  invalid = valid;
  invalid.surface.specular = -1.0f;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));
  invalid = valid;
  invalid.surface.shininess = -1.0f;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));
  invalid = valid;
  invalid.surface.emission = -1.0f;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));

  ASSERT_TRUE(ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &valid));
  invalid = valid;
  invalid.args.textured.alpha_mode = (LDKMaterialAlphaMode)99;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));
  invalid = valid;
  invalid.args.textured.alpha_mode = LDK_MATERIAL_ALPHA_MODE_CUTOUT;
  invalid.args.textured.alpha_cutoff = -0.1f;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));
  invalid.args.textured.alpha_cutoff = 1.1f;
  ASSERT_FALSE(ldk_material_desc_is_valid(&invalid));
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
  desc.specular = 0.25f;
  desc.shininess = 0.0f;
  desc.emission = 0.5f;

  LDKResourceMaterial material = ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, material));
  ASSERT_EQ(renderer.material_count, 1u);
  ASSERT_EQ(renderer.materials[0].desc.type, LDK_MATERIAL_TYPE_VERTEX_COLOR);
  ASSERT_EQ(renderer.materials[0].desc.texture.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(renderer.materials[0].desc.normal_map.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(
      renderer.materials[0].desc.specular_map.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(renderer.materials[0].desc.color, 0x10203040u);
  ASSERT_EQ(renderer.materials[0].desc.specular, 0.25f);
  ASSERT_EQ(renderer.materials[0].desc.shininess, 32.0f);
  ASSERT_EQ(renderer.materials[0].desc.emission, 0.5f);
  ASSERT_EQ(renderer.materials[0].desc.alpha_mode,
      LDK_MATERIAL_ALPHA_MODE_OPAQUE);
  ASSERT_EQ(renderer.materials[0].desc.alpha_cutoff, 0.5f);
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
      3, sizeof(LDKRendererTextureResource));
  ASSERT_TRUE(renderer.textures != NULL);
  renderer.texture_count = 3;
  renderer.texture_capacity = 3;
  renderer.textures[0].alive = true;
  renderer.textures[1].alive = true;
  renderer.textures[2].alive = true;
  desc.texture.id = 1u;
  desc.normal_map.id = 2u;
  desc.specular_map.id = 3u;
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  desc.alpha_cutoff = 0.5f;

  LDKResourceMaterial material = ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, material));
  ASSERT_EQ(renderer.materials[0].desc.texture.id, 1u);
  ASSERT_EQ(renderer.materials[0].desc.normal_map.id, 2u);
  ASSERT_EQ(renderer.materials[0].desc.specular_map.id, 3u);
  ASSERT_EQ(renderer.materials[0].desc.color, 0xffffffffu);
  ASSERT_EQ(renderer.materials[0].desc.shininess, 32.0f);
  ASSERT_EQ(renderer.materials[0].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED);
  ASSERT_NEQ(renderer.materials[0].render_key, 0u);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  LDKResourceMaterial unlit_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, unlit_material));
  ASSERT_EQ(renderer.materials[1].desc.normal_map.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(
      renderer.materials[1].desc.specular_map.id, LDK_RHI_INVALID_RESOURCE);
  ASSERT_EQ(renderer.materials[1].desc.shininess, 32.0f);
  ASSERT_EQ(renderer.materials[1].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT);
  ASSERT_NEQ(renderer.materials[0].render_key,
      renderer.materials[1].render_key);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED;
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_CUTOUT;
  desc.alpha_cutoff = 0.35f;
  LDKResourceMaterial cutout_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, cutout_material));
  ASSERT_EQ(renderer.materials[2].desc.alpha_mode,
      LDK_MATERIAL_ALPHA_MODE_CUTOUT);
  ASSERT_EQ(renderer.materials[2].desc.alpha_cutoff, 0.35f);
  ASSERT_EQ(renderer.materials[2].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT);
  ASSERT_NEQ(renderer.materials[0].render_key,
      renderer.materials[2].render_key);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  LDKResourceMaterial unlit_cutout_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(
      ldk_renderer_material_is_valid(&renderer, unlit_cutout_material));
  ASSERT_EQ(renderer.materials[3].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED;
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_BLEND;
  LDKResourceMaterial blend_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, blend_material));
  ASSERT_EQ(renderer.materials[4].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND);

  desc.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  LDKResourceMaterial unlit_blend_material =
      ldk_renderer_material_create(&renderer, &desc);
  ASSERT_TRUE(ldk_renderer_material_is_valid(&renderer, unlit_blend_material));
  ASSERT_EQ(renderer.materials[5].selection,
      LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND);

  ldk_renderer_material_destroy(&renderer, material);
  ldk_renderer_material_destroy(&renderer, unlit_material);
  ldk_renderer_material_destroy(&renderer, cutout_material);
  ldk_renderer_material_destroy(&renderer, unlit_cutout_material);
  ldk_renderer_material_destroy(&renderer, blend_material);
  ldk_renderer_material_destroy(&renderer, unlit_blend_material);
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

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  desc.shininess = -1.0f;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  desc.shininess = 32.0f;
  desc.specular = -1.0f;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  desc.specular = 0.0f;
  desc.emission = -1.0f;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  desc.emission = 0.0f;
  desc.normal_map.id = 1u;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));

  renderer.textures = (LDKRendererTextureResource *)calloc(
      1, sizeof(LDKRendererTextureResource));
  ASSERT_TRUE(renderer.textures != NULL);
  renderer.texture_count = 1;
  renderer.texture_capacity = 1;
  renderer.textures[0].alive = true;
  desc = (LDKRendererMaterialDesc){0};
  desc.type = LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  desc.texture.id = 1u;
  desc.alpha_mode = (LDKMaterialAlphaMode)99;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_CUTOUT;
  desc.alpha_cutoff = -0.1f;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  desc.alpha_cutoff = 1.1f;
  ASSERT_FALSE(ldk_renderer_material_is_valid(
      &renderer, ldk_renderer_material_create(&renderer, &desc)));
  free(renderer.textures);

  return 0;
}

static int test_mesh_source_authored_material(void)
{
  LDKMeshSource mesh_source = {0};
  LDKMaterialDesc material = {0};

  ASSERT_TRUE(ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, &material));
  material.args.vertex_color.color = 0x204060ffu;

  ASSERT_TRUE(ldk_mesh_source_set_material(&mesh_source, &material));
  ASSERT_TRUE(mesh_source.material_dirty);
  ASSERT_TRUE(ldk_material_desc_equal(&mesh_source.material, &material));

  mesh_source.material_dirty = false;
  ASSERT_TRUE(ldk_mesh_source_set_material(&mesh_source, &material));
  ASSERT_FALSE(mesh_source.material_dirty);

  LDKMaterialDesc invalid = {0};
  ASSERT_FALSE(ldk_mesh_source_set_material(&mesh_source, &invalid));
  ASSERT_FALSE(ldk_mesh_source_set_material(NULL, &material));
  ASSERT_FALSE(ldk_mesh_source_set_material(&mesh_source, NULL));
  ASSERT_TRUE(ldk_material_desc_equal(&mesh_source.material, &material));

  LDKAssetMesh asset = {0};
  asset.h.index = 3u;
  asset.h.version = 7u;
  mesh_source.renderer_mesh.id = 12u;
  ASSERT_TRUE(ldk_mesh_source_set_data(&mesh_source, asset));
  ASSERT_TRUE(mesh_source.dirty);
  ASSERT_EQ(mesh_source.renderer_mesh.id, 12u);

  return 0;
}

static LDKRHITexture s_test_image_upload(void* user, const LDKRHITextureDesc* desc)
{
  (void)desc;
  return ++*(u32*)user;
}

static LDKRHISampler s_test_image_sampler(void* user, const LDKRHISamplerDesc* desc)
{
  (void)user;
  (void)desc;
  return 100u;
}

static void s_test_image_destroy(void* user, LDKRHIResource resource)
{
  (void)user;
  (void)resource;
}

static int test_shared_image_cache(void)
{
  LDKAssetManager assets = {0};
  XHPoolConfig config = {0};
  config.page_capacity = 4;
  config.initial_pages = 1;
  ASSERT_TRUE(x_hpool_init(&assets.pool, sizeof(LDKAssetInfo), config,
      NULL, NULL, NULL));
  u32 pixel = 0xffffffffu;
  LDKAssetImageData data = {ldk_image_create(1, 1, &pixel)};
  ASSERT_TRUE(data.image != NULL);
  LDKAssetImage image = {x_hpool_alloc(&assets.pool)};
  LDKAssetInfo* info = x_hpool_get(&assets.pool, image.h);
  memset(info, 0, sizeof(*info));
  info->type = LDK_ASSET_TYPE_IMAGE;
  info->data = &data;

  u32 uploads = 0;
  LDKRHIContext rhi = {0};
  LDKRHIContextDesc rhi_desc = {0};
  LDKRHIFunctions functions = {0};
  rhi_desc.backend_type = LDK_RHI_BACKEND_OPENGL33;
  rhi_desc.backend_user_data = &uploads;
  functions.texture_create = s_test_image_upload;
  functions.create_sampler = s_test_image_sampler;
  functions.texture_destroy = s_test_image_destroy;
  functions.destroy_sampler = s_test_image_destroy;
  ASSERT_TRUE(ldk_rhi_initialize(&rhi, &rhi_desc, &functions));
  LDKRenderer renderer = {0};
  renderer.rhi = &rhi;
  renderer.is_initialized = true;
  LDKResourceTexture a = ldk_renderer_image_acquire(&renderer, &assets, image);
  LDKResourceTexture b = ldk_renderer_image_acquire(&renderer, &assets, image);
  ASSERT_TRUE(ldk_renderer_texture_is_valid(&renderer, a));
  ASSERT_EQ(a.id, b.id);
  ASSERT_EQ(uploads, 1u);
  ldk_renderer_texture_destroy(&renderer, a);
  ASSERT_TRUE(ldk_renderer_texture_is_valid(&renderer, a));
  ldk_renderer_image_release(&renderer, a);
  ASSERT_TRUE(ldk_renderer_texture_is_valid(&renderer, b));

  x_hpool_free(&assets.pool, image.h);
  ASSERT_EQ(ldk_renderer_image_acquire(&renderer, &assets, image).id, 0u);
  LDKAssetImage replacement = {x_hpool_alloc(&assets.pool)};
  info = x_hpool_get(&assets.pool, replacement.h);
  memset(info, 0, sizeof(*info));
  info->type = LDK_ASSET_TYPE_IMAGE;
  info->data = &data;
  LDKResourceTexture c =
      ldk_renderer_image_acquire(&renderer, &assets, replacement);
  ASSERT_NEQ(c.id, b.id);
  ASSERT_EQ(uploads, 2u);
  ldk_renderer_image_release(&renderer, b);
  ASSERT_FALSE(ldk_renderer_texture_is_valid(&renderer, b));
  ldk_renderer_image_release(&renderer, c);
  ASSERT_FALSE(ldk_renderer_texture_is_valid(&renderer, c));
  ldk_renderer_image_release(&renderer, c);
  ldk_rhi_terminate(&rhi);
  free(renderer.textures);
  ldk_image_destroy(data.image);
  x_hpool_term(&assets.pool);
  return 0;
}

typedef struct TestRendererInstancingBackend
{
  u64 next_resource;
  u32 draw_indexed_count;
  u32 draw_indexed_instanced_count;
  u32 instance_count;
  bool fail_instance_upload;
  bool fail_instance_allocation;
} TestRendererInstancingBackend;

static LDKRHIBuffer s_test_renderer_instancing_buffer_create(
    void *user, const LDKRHIBufferDesc *desc)
{
  TestRendererInstancingBackend *backend =
      (TestRendererInstancingBackend *)user;
  if (backend->fail_instance_allocation &&
      (desc->usage & LDK_RHI_BUFFER_USAGE_VERTEX))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }
  return ++backend->next_resource;
}

static bool s_test_renderer_instancing_buffer_update(void *user,
    LDKRHIBuffer buffer, u32 offset, u32 size, const void *data)
{
  TestRendererInstancingBackend *backend = user;
  (void)buffer;
  (void)offset;
  (void)data;
  return !backend->fail_instance_upload || size != 2 * sizeof(Mat4);
}

static void s_test_renderer_instancing_pass_begin(
    void *user, const LDKRHIPassDesc *desc)
{
  (void)user;
  (void)desc;
}

static void s_test_renderer_instancing_pass_end(void *user)
{
  (void)user;
}

static void s_test_renderer_instancing_pipeline_bind(
    void *user, LDKRHIPipeline pipeline)
{
  (void)user;
  (void)pipeline;
}

static void s_test_renderer_instancing_bindings_bind(
    void *user, LDKRHIBindings bindings)
{
  (void)user;
  (void)bindings;
}

static void s_test_renderer_instancing_vertex_buffer_bind_at(void *user,
    u32 slot, LDKRHIBuffer buffer, u32 offset)
{
  (void)user;
  (void)slot;
  (void)buffer;
  (void)offset;
}

static void s_test_renderer_instancing_index_buffer_bind(void *user,
    LDKRHIBuffer buffer, u32 offset, LDKRHIIndexType index_type)
{
  (void)user;
  (void)buffer;
  (void)offset;
  (void)index_type;
}

static void s_test_renderer_instancing_draw_indexed(
    void *user, const LDKRHIDrawIndexedDesc *desc)
{
  TestRendererInstancingBackend *backend =
      (TestRendererInstancingBackend *)user;
  (void)desc;
  backend->draw_indexed_count++;
}

static void s_test_renderer_instancing_draw_indexed_instanced(
    void *user, const LDKRHIDrawIndexedInstancedDesc *desc)
{
  TestRendererInstancingBackend *backend =
      (TestRendererInstancingBackend *)user;
  backend->draw_indexed_instanced_count++;
  backend->instance_count += desc->instance_count;
}

static bool s_test_renderer_instancing_setup(LDKRenderer *renderer,
    LDKRHIContext *rhi, TestRendererInstancingBackend *backend,
    bool instancing_supported, bool explicit_instances)
{
  LDKRHIContextDesc rhi_desc = {0};
  LDKRHIFunctions functions = {0};
  rhi_desc.backend_type = LDK_RHI_BACKEND_OPENGL33;
  rhi_desc.backend_user_data = backend;
  functions.buffer_create = s_test_renderer_instancing_buffer_create;
  functions.buffer_update = s_test_renderer_instancing_buffer_update;
  functions.pass_begin = s_test_renderer_instancing_pass_begin;
  functions.pass_end = s_test_renderer_instancing_pass_end;
  functions.pipeline_bind = s_test_renderer_instancing_pipeline_bind;
  functions.bindings_bind = s_test_renderer_instancing_bindings_bind;
  functions.vertex_buffer_bind_at =
      s_test_renderer_instancing_vertex_buffer_bind_at;
  functions.index_buffer_bind = s_test_renderer_instancing_index_buffer_bind;
  functions.draw_indexed = s_test_renderer_instancing_draw_indexed;
  if (instancing_supported)
  {
    functions.draw_indexed_instanced =
        s_test_renderer_instancing_draw_indexed_instanced;
  }

  if (!ldk_rhi_initialize(rhi, &rhi_desc, &functions))
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = rhi;
  renderer->game_width = 640;
  renderer->game_height = 480;
  renderer->ambient_light.color = 0xffffffffu;
  renderer->is_initialized = true;

  renderer->shadow_pass.rhi = rhi;
  renderer->shadow_pass.resolution = 512;
  renderer->shadow_pass.distance = 60.0f;
  renderer->shadow_pass.depth_texture = 201;
  renderer->shadow_pass.camera_buffer = 202;
  renderer->shadow_pass.object_buffer = 203;
  renderer->shadow_pass.material_buffer = 204;
  renderer->shadow_pass.bindings = 205;
  renderer->shadow_pass.pipeline = 206;
  renderer->shadow_pass.instanced_pipeline = 207;

  renderer->mesh_pass.rhi = rhi;
  renderer->mesh_pass.is_initialized = true;
  renderer->mesh_pass.camera_buffer = 301;
  renderer->mesh_pass.object_buffer = 302;
  renderer->mesh_pass.material_buffer = 303;
  renderer->mesh_pass.lighting_buffer = 304;
  renderer->mesh_pass.bindings = 305;
  renderer->mesh_pass.vertex_color_unlit_pipeline = 306;
  renderer->mesh_pass.vertex_color_unlit_instanced_pipeline = 307;

  renderer->meshes =
      (LDKRendererMeshResource *)calloc(1, sizeof(*renderer->meshes));
  renderer->materials =
      (LDKRendererMaterialResource *)calloc(1, sizeof(*renderer->materials));
  if (!renderer->meshes || !renderer->materials)
  {
    return false;
  }

  renderer->mesh_count = renderer->mesh_capacity = 1;
  renderer->meshes[0].vertex_buffer = 401;
  renderer->meshes[0].index_buffer = 402;
  renderer->meshes[0].vertex_count = 3;
  renderer->meshes[0].index_count = 3;
  renderer->meshes[0].alive = true;

  renderer->material_count = renderer->material_capacity = 1;
  renderer->materials[0].desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT;
  renderer->materials[0].desc.color = 0xffffffffu;
  renderer->materials[0].selection =
      LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT;
  renderer->materials[0].render_key = 1;
  renderer->materials[0].alive = true;

  Mat4 view = mat4_look_at_rh(vec3_make(0.0f, 0.0f, 5.0f),
      vec3_make(0.0f, 0.0f, 0.0f), vec3_make(0.0f, 1.0f, 0.0f));
  Mat4 projection = mat4_perspective_rh_no(
      STDXM_PI / 3.0f, 640.0f / 480.0f, 0.1f, 100.0f);
  if (!ldk_renderer_submit_view(renderer, 1, view, projection) ||
      !ldk_renderer_game_view_set(renderer, 1))
  {
    return false;
  }

  renderer->views[0].target.color_texture = 501;
  renderer->views[0].target.depth_texture = 502;
  renderer->views[0].target.width = 640;
  renderer->views[0].target.height = 480;

  LDKResourceMesh mesh = {1};
  LDKResourceMaterial material = {1};
  Mat4 world = mat4_identity();
  if (explicit_instances)
  {
    Mat4 instances[] = {world, mat4_translate(vec3_make(2, 0, 0))};
    if (!ldk_renderer_submit_mesh_instances(renderer, LDK_RENDERER_VIEW_ALL,
            mesh, material, 0, 3, world, instances, 2,
            LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS))
    {
      return false;
    }
  }
  else if (!ldk_renderer_submit_mesh(renderer, mesh, material, world) ||
      !ldk_renderer_submit_mesh(renderer, mesh, material, world))
  {
    return false;
  }

  LDKRendererLightSubmit light = {0};
  light.type = LDK_RENDERER_LIGHT_DIRECTIONAL;
  light.view_id = LDK_RENDERER_VIEW_ALL;
  light.direction = vec3_make(-1.0f, -1.0f, -1.0f);
  light.color = 0xffffffffu;
  light.intensity = 1.0f;
  light.casts_shadows = true;
  return ldk_renderer_submit_light(renderer, &light);
}

static void s_test_renderer_instancing_cleanup(
    LDKRenderer *renderer, LDKRHIContext *rhi)
{
  ldk_renderer_terminate(renderer);
  ldk_rhi_terminate(rhi);
}

static int test_renderer_instancing_batches_color_and_shadow(void)
{
  TestRendererInstancingBackend backend = {0};
  LDKRHIContext rhi = {0};
  LDKRenderer renderer = {0};
  LDKRendererFrameDesc frame = {0};

  backend.next_resource = 1000;
  ASSERT_TRUE(s_test_renderer_instancing_setup(
      &renderer, &rhi, &backend, true, true));

  frame.framebuffer_width = 640;
  frame.framebuffer_height = 480;
  frame.clear_color = 0x000000ffu;
  frame.clear_color_enabled = true;
  ldk_renderer_render_frame(&renderer, &frame);

  LDKRendererFrameStats stats = ldk_renderer_last_frame_stats_get(&renderer);
  ASSERT_EQ(backend.draw_indexed_instanced_count, 2u);
  ASSERT_EQ(backend.draw_indexed_count, 0u);
  ASSERT_EQ(stats.game.shadow_draw_call_count, 1u);
  ASSERT_EQ(stats.game.opaque_mesh_draw_call_count, 1u);
  ASSERT_EQ(stats.game.instanced_batch_count, 1u);
  ASSERT_EQ(stats.game.instanced_instance_count, 2u);

  s_test_renderer_instancing_cleanup(&renderer, &rhi);
  return 0;
}

static int test_renderer_ordinary_batches_never_instance(void)
{
  TestRendererInstancingBackend backend = {0};
  LDKRHIContext rhi = {0};
  LDKRenderer renderer = {0};
  ASSERT_TRUE(s_test_renderer_instancing_setup(
      &renderer, &rhi, &backend, true, false));
  LDKRendererFrameDesc frame = {0};
  frame.framebuffer_width = 640;
  frame.framebuffer_height = 480;
  ldk_renderer_render_frame(&renderer, &frame);
  LDKRendererFrameStats stats = ldk_renderer_last_frame_stats_get(&renderer);
  ASSERT_EQ(backend.draw_indexed_instanced_count, 0u);
  ASSERT_EQ(backend.draw_indexed_count, 4u);
  ASSERT_EQ(stats.game.batch_count, 1u);
  ASSERT_EQ(stats.game.max_batch_size, 2u);
  s_test_renderer_instancing_cleanup(&renderer, &rhi);
  return 0;
}

static int test_renderer_instances_copy_transform_and_validate(void)
{
  TestRendererInstancingBackend backend = {0};
  LDKRHIContext rhi = {0};
  LDKRenderer renderer = {0};
  ASSERT_TRUE(s_test_renderer_instancing_setup(
      &renderer, &rhi, &backend, true, false));
  LDKResourceMesh mesh = {1};
  LDKResourceMaterial material = {1};
  Mat4 parent = mat4_translate(vec3_make(10, 0, 0));
  Mat4 local = mat4_translate(vec3_make(3, 0, 0));
  ASSERT_TRUE(ldk_renderer_submit_mesh_instances(&renderer,
      LDK_RENDERER_VIEW_ALL, mesh, material, 0, 3, parent, &local, 1, 0));
  local.m[12] = 99;
  ASSERT_EQ(renderer.submitted_mesh_count, 3u);
  ASSERT_EQ(renderer.submitted_instance_worlds[0].m[12], 13.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].r, 1.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].g, 1.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].b, 1.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].a, 1.0f);
  ASSERT_FALSE(ldk_renderer_submit_mesh_instances(&renderer,
      LDK_RENDERER_VIEW_ALL, mesh, material, 0, 3, parent, NULL, 1, 0));
  ASSERT_FALSE(ldk_renderer_submit_mesh_instances(&renderer,
      LDK_RENDERER_VIEW_ALL, mesh, material, 2, 3, parent, &local, 1, 0));
  ASSERT_TRUE(ldk_renderer_submit_mesh_instances(&renderer,
      LDK_RENDERER_VIEW_ALL, mesh, material, 0, 3, parent, NULL, 0, 0));
  ASSERT_EQ(renderer.submitted_mesh_count, 3u);
  LDKRendererFrameDesc frame = {0};
  frame.framebuffer_width = 640;
  frame.framebuffer_height = 480;
  ldk_renderer_render_frame(&renderer, &frame);
  ASSERT_EQ(backend.draw_indexed_count, 4u);
  ASSERT_EQ(backend.draw_indexed_instanced_count, 1u);
  ASSERT_EQ(backend.instance_count, 1u);
  ASSERT_EQ(renderer.submitted_instance_count, 0u);
  ASSERT_EQ(renderer.submitted_mesh_count, 0u);
  s_test_renderer_instancing_cleanup(&renderer, &rhi);
  return 0;
}

static int test_renderer_instances_copy_colors(void)
{
  TestRendererInstancingBackend backend = {0};
  LDKRHIContext rhi = {0};
  LDKRenderer renderer = {0};
  ASSERT_TRUE(s_test_renderer_instancing_setup(
      &renderer, &rhi, &backend, true, false));
  LDKResourceMesh mesh = {1};
  LDKResourceMaterial material = {1};
  Mat4 local = mat4_identity();
  u32 color = 0x80402010u;
  ASSERT_TRUE(ldk_renderer_submit_mesh_instances_colored(&renderer,
      LDK_RENDERER_VIEW_ALL, mesh, material, 0, 3, mat4_identity(), &local,
      &color, 1, 0));
  color = 0xffffffffu;
  ASSERT_EQ(renderer.submitted_instance_colors[0].r, 128.0f / 255.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].g, 64.0f / 255.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].b, 32.0f / 255.0f);
  ASSERT_EQ(renderer.submitted_instance_colors[0].a, 16.0f / 255.0f);
  s_test_renderer_instancing_cleanup(&renderer, &rhi);
  return 0;
}

static int test_renderer_instances_fallback_resources(void)
{
  for (u32 failure = 0; failure < 3; ++failure)
  {
    TestRendererInstancingBackend backend = {0};
    LDKRHIContext rhi = {0};
    LDKRenderer renderer = {0};
    ASSERT_TRUE(s_test_renderer_instancing_setup(
        &renderer, &rhi, &backend, true, true));
    if (failure == 0)
    {
      backend.fail_instance_upload = true;
    }
    else if (failure == 1)
    {
      renderer.mesh_pass.vertex_color_unlit_instanced_pipeline = 0;
      renderer.shadow_pass.instanced_pipeline = 0;
    }
    else
    {
      backend.fail_instance_allocation = true;
    }
    LDKRendererFrameDesc frame = {0};
    frame.framebuffer_width = 640;
    frame.framebuffer_height = 480;
    ldk_renderer_render_frame(&renderer, &frame);
    ASSERT_EQ(backend.draw_indexed_count, 4u);
    ASSERT_EQ(backend.draw_indexed_instanced_count, 0u);
    s_test_renderer_instancing_cleanup(&renderer, &rhi);
  }
  return 0;
}

static int test_renderer_instancing_falls_back_without_backend_support(void)
{
  TestRendererInstancingBackend backend = {0};
  LDKRHIContext rhi = {0};
  LDKRenderer renderer = {0};
  LDKRendererFrameDesc frame = {0};

  backend.next_resource = 1000;
  ASSERT_TRUE(s_test_renderer_instancing_setup(
      &renderer, &rhi, &backend, false, true));

  frame.framebuffer_width = 640;
  frame.framebuffer_height = 480;
  frame.clear_color = 0x000000ffu;
  frame.clear_color_enabled = true;
  ldk_renderer_render_frame(&renderer, &frame);

  LDKRendererFrameStats stats = ldk_renderer_last_frame_stats_get(&renderer);
  ASSERT_EQ(backend.draw_indexed_instanced_count, 0u);
  ASSERT_EQ(backend.draw_indexed_count, 4u);
  ASSERT_EQ(stats.game.shadow_draw_call_count, 2u);
  ASSERT_EQ(stats.game.opaque_mesh_draw_call_count, 2u);
  ASSERT_EQ(stats.game.instanced_batch_count, 0u);
  ASSERT_EQ(stats.game.instanced_instance_count, 0u);

  s_test_renderer_instancing_cleanup(&renderer, &rhi);
  return 0;
}

static int test_missing_image_checker(void)
{
  LDKAssetManager assets = {0};
  XHPoolConfig config = {0};
  config.page_capacity = 4;
  config.initial_pages = 1;
  ASSERT_TRUE(x_hpool_init(&assets.pool, sizeof(LDKAssetInfo), config,
      NULL, NULL, NULL));
  LDKAssetImage a = ldk_asset_manager_image_missing(
      &assets, "/project/runtree/missing.png");
  LDKAssetImage b = ldk_asset_manager_image_missing(
      &assets, "/project/runtree/missing.png");
  ASSERT_FALSE(x_handle_is_null(a.h));
  ASSERT_EQ(a.h.index, b.h.index);
  ASSERT_EQ(a.h.version, b.h.version);
  LDKAssetHandle handle = {a.h};
  const LDKAssetInfo *info = ldk_asset_get_info_const(&assets, handle);
  XFSPath expected = {0};
  x_fs_path_set(&expected, "/project/runtree/missing.png");
  x_fs_path_normalize(&expected);
  ASSERT_TRUE(strcmp(info->asset_path.buf, expected.buf) == 0);
  LDKAssetImageData *data = ldk_asset_manager_image_get(&assets, a);
  ASSERT_TRUE(data->is_missing);
  ASSERT_EQ(ldk_image_get_width(data->image), 8u);
  ASSERT_EQ(ldk_image_get_height(data->image), 8u);
  const u8 *pixels = ldk_image_get_pixels(data->image);
  for (u32 y = 0; y < 8; ++y)
    for (u32 x = 0; x < 8; ++x)
    {
      u32 offset = (y * 8 + x) * 4;
      u8 bright = ((x / 2) ^ (y / 2)) & 1 ? 255 : 0;
      ASSERT_EQ(pixels[offset], bright);
      ASSERT_EQ(pixels[offset + 1], 0);
      ASSERT_EQ(pixels[offset + 2], bright);
      ASSERT_EQ(pixels[offset + 3], 255);
    }
  ldk_image_destroy(data->image);
  free(data);
  x_hpool_term(&assets.pool);
  return 0;
}

static int test_instanced_mesh_source_owns_transforms(void)
{
  LDKInstancedMeshSource source = {0};
  Mat4 local[2] = {mat4_identity(), mat4_translate(vec3_make(1, 2, 3))};
  ASSERT_TRUE(ldk_instanced_mesh_source_set_instances(&source, local, 2));
  ASSERT_TRUE(source.instances != local);
  ASSERT_TRUE(source.instance_colors != NULL);
  ASSERT_EQ(source.instance_colors[0], 0xffffffffu);
  ASSERT_EQ(source.instance_colors[1], 0xffffffffu);
  u32 colors[2] = {0x10203040u, 0x50607080u};
  ASSERT_TRUE(ldk_instanced_mesh_source_set_instance_colors(
      &source, colors, 2));
  colors[0] = 0xffffffffu;
  ASSERT_EQ(source.instance_colors[0], 0x10203040u);
  ASSERT_FALSE(ldk_instanced_mesh_source_set_instance_colors(
      &source, colors, 1));
  local[1].m[12] = 8;
  ASSERT_EQ(source.instances[1].m[12], 1.0f);
  ASSERT_TRUE(ldk_instanced_mesh_source_set_instances(
      &source, source.instances + 1, 1));
  ASSERT_EQ(source.instance_count, 1u);
  ASSERT_EQ(source.instances[0].m[14], 3.0f);
  local[0].m[0] = NAN;
  ASSERT_FALSE(ldk_instanced_mesh_source_set_instances(&source, local, 2));
  ASSERT_FALSE(ldk_instanced_mesh_source_set_instances(&source, NULL, 1));
  ASSERT_EQ(source.instance_count, 1u);
  ASSERT_TRUE(ldk_instanced_mesh_source_set_instances(&source, NULL, 0));
  ASSERT_TRUE(source.instances != NULL);
  ASSERT_TRUE(source.instance_colors != NULL);
  ASSERT_TRUE(source.instance_capacity >= 2u);
  ASSERT_EQ(source.instance_count, 0u);
  free(source.instances);
  free(source.instance_colors);
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
      X_TEST(test_mesh_source_authored_material),
      X_TEST(test_shared_image_cache),
      X_TEST(test_renderer_instancing_batches_color_and_shadow),
      X_TEST(test_renderer_ordinary_batches_never_instance),
      X_TEST(test_renderer_instances_copy_transform_and_validate),
      X_TEST(test_renderer_instances_copy_colors),
      X_TEST(test_renderer_instances_fallback_resources),
      X_TEST(test_renderer_instancing_falls_back_without_backend_support),
      X_TEST(test_missing_image_checker),
      X_TEST(test_instanced_mesh_source_owns_transforms),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
