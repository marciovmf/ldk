/*
* Renderer queue, sorting, and batching notes
* ---
*
* Mesh submission and mesh rendering are intentionally separate concerns.
* Callers submit independent meshes through LDKRendererMeshSubmit. The renderer
* does not require gameplay/editor code to know whether a submitted object will
* eventually be rendered individually or as part of an instanced batch.
*
* The main opaque color-pass flow is:
*
* ```
*  mesh submissions
*      -> build a render queue for a view
*      -> assign compact queue-local IDs
*      -> radix-sort packed render items
*      -> scan consecutive compatible items
*      -> draw a singleton normally or a compatible run instanced
*  ```
*
* IMPORTANT INVARIANTS
* ---
*
* 1. submitted_meshes[] remains in submission order.
*
* The global submission array is not sorted in place. This is important
* because other passes may have different ordering/filtering requirements.
*
* In particular:
*
* ```
*  - the shadow pass builds its own filtered queue from submitted_meshes[];
*  - overlays preserve submission order;
*  - each view builds its own opaque render queue;
*  - translucent meshes build a separate back-to-front queue per view.
*  ```
*
* Future code should not assume that changing the opaque queue also changes
* the ordering seen by other passes.
*
*
* 2. Opaque render queues are view-local.
*
* A queue contains all opaque submissions visible to one view. This includes
* both submissions explicitly targeting that view and submissions targeting
* LDK_RENDERER_VIEW_ALL.
*
* Consequently, a VIEW_ALL submission and a view-specific submission may be
* sorted next to each other and may form the same batch when all other batch
* requirements match.
*
* View identity is therefore routing information, not part of the opaque
* render-state sort key.
*
*
* 3. The packed queue item is a u64, but only the low 40 bits are sorted.
*
* Current layout:
*
* ```
*     63                40 39       32 31              16 15               0
*    +--------------------+-----------+------------------+------------------+
*    |  submit index (24) | pipeline  | material id (16) |   mesh id (16)   |
*    +--------------------+-----------+------------------+------------------+
*           payload              40-bit sortable render-state key
*  ```
*
* The radix sorter performs five 8-bit passes over bits 0..39. The upper
* 24-bit submit index is payload: it travels with the sorted item but does
* not participate in ordering.
*
* Keeping the entire sort item in one u64 is deliberate. Radix sorting is
* bandwidth-sensitive, so moving one 8-byte value per item is preferable to
* sorting larger structures or moving complete mesh submissions containing
* transforms and other state.
*
*
* 4. Compact material and mesh IDs are queue-local IDs.
*
* The 16-bit material and mesh fields in the packed key are NOT renderer
* resource handles and are NOT lifetime-wide resource IDs.
*
* They are compact IDs assigned while building a render queue.
*
* Therefore, the 16-bit limit means:
*
* ```
*     <= 65536 distinct materials participating in one queue
*     <= 65536 distinct meshes participating in one queue
*  ```
*
* It does NOT mean that the renderer, project, or application may only ever
* own 65536 material or mesh resources.
*
* Do not truncate LDKResourceMaterial or LDKResourceMesh handles into these
* fields. Any future change to resource-handle representation must remain
* independent from the compact render-queue IDs.
*
*
* 5. LDKRendererRenderKey is not the packed queue item.
*
* LDKRendererRenderKey currently belongs to the material resource and
* classifies the pipeline/material rendering path. It is used as the
* pipeline portion of the packed sort state.
*
* Do not reinterpret LDKRendererRenderKey as the complete packed queue key
* unless the surrounding design is intentionally changed.
*
* The distinction is:
*
* ```
*     material->render_key = pipeline/render-path classification
*     packed queue item = pipeline + compact material + compact mesh + submit payload
*  ```
*
* 6. Sort order groups expensive render state first.
*
* The current ordering hierarchy is effectively:
*
* ```
*     pipeline -> material -> mesh
*  ```
*
* This groups draws that can reuse the same pipeline, material parameters,
* texture bindings, vertex buffer, and index buffer.
*
* For lit materials, equality of the material resource also implies equality
* of all renderer material state owned by that resource, including albedo,
* normal/specular maps, surface parameters, etc. These values therefore do
* not need to be duplicated in the sort key.
*
*
* 7. Geometry range is deliberately NOT part of the packed sort key.
*
* first_index and index_count remain on LDKRendererMeshSubmit and are checked
* while refining a sorted sequence into actual draw batches.
*
* Ordinary submissions may share a batch only when they have the same:
*
* ```
* - mesh
* - material
* - first_index
* - index_count
* - relevant pass state
* ```
*
* World transform is intentionally excluded because it is the per-instance
* data.
*
* The consequence of keeping first_index/index_count out of the radix key is
* that ranges of the same mesh may occasionally fragment:
*
* ```
*     range A, range B, range A
*  ```
*
* may produce:
*
* ```
*     A | B | A
*  ```
*
* rather than:
*
* ```
*     A A | B
*  ```
*
* This is currently an intentional trade-off in favor of a compact key.
* If geometry-range fragmentation becomes measurable, prefer introducing a
* compact geometry/submesh ID rather than placing raw first_index and
* index_count values directly into the packed key.
*
*
* 8. CAST_SHADOWS is not part of opaque color-pass batch identity.
*
* Whether an object casts shadows does not affect how it is drawn in the
* color pass, so it must not split otherwise compatible color batches.
*
* The shadow pass evaluates CAST_SHADOWS independently while building its own
* filtered queue. Only explicit instance submissions use shadow instancing.
*
* Pass-specific state should only participate in a queue key when it changes
* the rendering state of that pass.
*
*
* 9. Overlays are not radix-sorted or instanced by this path.
*
* Overlay ordering is observable and therefore preserves submission order.
* Do not add overlays to the opaque state sorter merely because they happen
* to use the same mesh/material resources.
*
* Transparent rendering, if introduced later, will similarly require an
* explicit ordering policy rather than automatically reusing the opaque
* state sort.
*
*
* 10. Instancing is an optimization, never a correctness requirement.
*
* Only explicit instance submissions use instanced pipelines when possible.
* Ordinary runs reuse render state but issue one draw per object.
* Per-instance data consists of a world matrix and optional RGBA tint.
*
* Mesh vertex data remains in vertex-buffer stream 0:
*
* - location 0 : position
* - location 1 : normal
* - location 2 : uv
* - location 3 : color
* - location 8 : tangent
*
* Instanced world matrices use stream 1:
*
* - location 4 : world column 0
* - location 5 : world column 1
* - location 6 : world column 2
* - location 7 : world column 3
*
* Instance colors use stream 2:
*
* - location 9 : RGBA tint
*
* Locations 4..7, location 8, and location 9 must remain non-overlapping when
* changing LDKMeshVertex or the built-in mesh shaders.
*
* Normal mapping does not require additional per-instance data: normals and
* tangents are transformed using the instance world matrix in the existing
* instanced vertex shader.
*
*
* 11. Failure must degrade to ordinary rendering.
*
* Queue scratch allocation, compact-ID exhaustion, instance-buffer growth,
* instance-buffer upload, or instanced-pipeline availability must not make
* otherwise valid geometry disappear.
*
* Where practical, batching/instancing failures should fall back to the
* ordinary non-instanced draw path.
*
* Optimization failures are not rendering failures.
*
*
* 12. Radix-sort assumptions must remain explicit.
*
* The current sorter uses:
*
* - 256 buckets
* - 8 bits per pass
* - 5 passes
*
* because exactly 40 low bits participate in ordering.
*
* If the packed layout changes, update the number of passes and masks
* together. Do not silently sort payload bits such as submit_index.
*
* The scatter phase must remain stable so that successive least-significant
* digit passes produce the intended lexicographic ordering.
*
*
* FUTURE CHANGES
* ---
*
* When extending the renderer, prefer adding information according to its
* actual semantic level:
*
* resource handle       -> persistent resource identity
* render_key            -> pipeline/render-path classification
* compact queue ID      -> temporary queue-local resource identity
* packed sort item      -> sortable render state + submission reference
* batch compatibility   -> exact requirements for one draw/instanced draw
*
* Avoid making persistent resource handles smaller merely to fit the packed
* render key. If more sorting dimensions are required, first consider whether
* they can be represented by compact queue-local IDs.
*
* Likewise, do not add state to the sort key unless ordering by that state
* materially improves state reuse or is required for correctness. A field may
* be relevant to final batch compatibility without needing to participate in
* the radix key.
*
*/


#include <ldk_common.h>
#include <ldk.h>
#include <ldk_os.h>
#include <ldk_text.h>
#include <math.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer);
static void s_renderer_text_pass_terminate(LDKRendererTextPass *pass);
static void s_renderer_text_pass_draw(LDKRenderer *renderer,
    LDKRendererTextPass *pass, LDKRendererView const *view);
static void s_renderer_mesh_pass_terminate(LDKRendererMeshPass* pass);
static void s_renderer_grid_pass_terminate(LDKRendererGridPass* pass);
static void s_renderer_skybox_pass_terminate(LDKRendererSkyboxPass *pass);
static void s_renderer_post_process_pass_terminate(
    LDKRendererPostProcessPass* pass);
static void s_renderer_blur_pass_terminate(LDKRendererBlurPass* pass);
static void s_renderer_post_process_view_destroy(
    LDKRenderer* renderer, LDKRendererView* view);
static bool s_renderer_overlay_display_draw(
    LDKRenderer* renderer, LDKRendererView* view);
static void s_renderer_post_process_bindings_destroy(
    LDKRenderer* renderer, LDKRendererView* view);
static void s_renderer_post_process_blur_bindings_destroy(
    LDKRenderer* renderer, LDKRendererView* view);
static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer);
static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer);
static void s_renderer_destroy_instance_set_resources(LDKRenderer* renderer);
static void s_renderer_destroy_texture_resources(LDKRenderer* renderer);
static void s_renderer_destroy_skybox_resources(LDKRenderer *renderer);
static void s_renderer_destroy_material_resources(LDKRenderer* renderer);
static void s_renderer_destroy_material_samplers(LDKRenderer* renderer);
static LDKRendererMaterialResource* s_renderer_material_get_resource(
    LDKRenderer* renderer,
    LDKResourceMaterial material);
static LDKRendererTextureResource* s_renderer_texture_get_resource(
    LDKRenderer* renderer,
    LDKResourceTexture texture);
static LDKRendererSkyboxResource *s_renderer_skybox_get_resource(
    LDKRenderer *renderer, LDKResourceSkybox skybox);
static void s_renderer_ui_pass_remove_texture_bindings(
    LDKRendererUIPass* renderer,
    LDKRHITexture texture);
static void s_renderer_text_pass_remove_texture_bindings(
    LDKRendererTextPass *pass, LDKRHITexture texture);
static void s_renderer_mesh_pass_remove_texture_bindings(
    LDKRendererMeshPass* pass,
    LDKRHITexture texture);
static void s_renderer_shadow_pass_remove_texture_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture);
static LDKRHISampler s_renderer_texture_sampler_from_rhi_texture(
    LDKRenderer* renderer,
    LDKRHITexture texture);
static LDKRHISampler s_renderer_material_sampler_get_or_create(
    LDKRenderer *renderer, const LDKRHISamplerDesc *desc);
static LDKRHIFilter s_renderer_material_filter_to_rhi(
    LDKMaterialTextureFilter filter);
static LDKRHIFilter s_renderer_material_mip_filter_to_rhi(
    LDKMaterialTextureMipFilter filter);
static LDKRHIWrap s_renderer_material_wrap_to_rhi(LDKMaterialTextureWrap wrap);

typedef struct LDKRendererUIParams
{
  float viewport_size[2];
} LDKRendererUIParams;

typedef struct LDKRendererTextCameraParams
{
  Mat4 view;
  Mat4 projection;
} LDKRendererTextCameraParams;

typedef struct LDKRendererTextObjectParams
{
  Mat4 world;
  float options[4];
} LDKRendererTextObjectParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererTextCameraParams) == 128,
    text_camera_std140_size);
LDK_STATIC_ASSERT(sizeof(LDKRendererTextObjectParams) == 80,
    text_object_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererTextObjectParams, options) == 64,
    text_object_options_std140_offset);

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(
    LDKRHIContext* rhi, u32 shader, u32 stage);

static LDKRHIColor s_renderer_color_from_rgba32_unorm(u32 color)
{
  LDKRHIColor result = {0};
  result.r = (float)((color >> 24) & 0xffu) / 255.0f;
  result.g = (float)((color >> 16) & 0xffu) / 255.0f;
  result.b = (float)((color >> 8) & 0xffu) / 255.0f;
  result.a = (float)(color & 0xffu) / 255.0f;
  return result;
}

static float s_renderer_srgb_to_linear(float value)
{
  return value <= 0.04045f ? value / 12.92f :
      powf((value + 0.055f) / 1.055f, 2.4f);
}

inline LDKRHIColor ldk_renderer_color_from_rgba32(u32 color)
{
  LDKRHIColor result = s_renderer_color_from_rgba32_unorm(color);
  result.r = s_renderer_srgb_to_linear(result.r);
  result.g = s_renderer_srgb_to_linear(result.g);
  result.b = s_renderer_srgb_to_linear(result.b);
  return result;
}

static LDKRendererFrameDomainStats* s_renderer_frame_stats_for_view(
    LDKRenderer* renderer, const LDKRendererView* view)
{
  if (renderer == NULL || view == NULL)
  {
    return NULL;
  }

  return view->id == renderer->game_view
      ? &renderer->current_frame_stats.game
      : &renderer->current_frame_stats.non_game;
}

static void s_renderer_frame_stats_finalize(LDKRendererFrameStats* stats)
{
  if (stats == NULL)
  {
    return;
  }

  const LDKRendererFrameDomainStats* game = &stats->game;
  const LDKRendererFrameDomainStats* non_game = &stats->non_game;

  stats->rendered_view_count =
      game->rendered_view_count + non_game->rendered_view_count;
  stats->opaque_mesh_render_count =
      game->opaque_mesh_render_count + non_game->opaque_mesh_render_count;
  stats->overlay_mesh_render_count =
      game->overlay_mesh_render_count + non_game->overlay_mesh_render_count;
  stats->batch_count = game->batch_count + non_game->batch_count;
  stats->instanced_batch_count =
      game->instanced_batch_count + non_game->instanced_batch_count;
  stats->instanced_instance_count =
      game->instanced_instance_count + non_game->instanced_instance_count;
  stats->max_batch_size = game->max_batch_size > non_game->max_batch_size
      ? game->max_batch_size : non_game->max_batch_size;
  stats->draw_call_count = game->draw_call_count + non_game->draw_call_count;
  stats->opaque_mesh_draw_call_count = game->opaque_mesh_draw_call_count +
      non_game->opaque_mesh_draw_call_count;
  stats->overlay_mesh_draw_call_count = game->overlay_mesh_draw_call_count +
      non_game->overlay_mesh_draw_call_count;
  stats->shadow_draw_call_count =
      game->shadow_draw_call_count + non_game->shadow_draw_call_count;
  stats->line_draw_call_count =
      game->line_draw_call_count + non_game->line_draw_call_count;
  stats->grid_draw_call_count =
      game->grid_draw_call_count + non_game->grid_draw_call_count;
  stats->skybox_draw_call_count =
      game->skybox_draw_call_count + non_game->skybox_draw_call_count;
  stats->ui_draw_call_count =
      game->ui_draw_call_count + non_game->ui_draw_call_count;
  stats->present_draw_call_count =
      game->present_draw_call_count + non_game->present_draw_call_count;
}

static void s_renderer_target_destroy(
    LDKRenderer* renderer, LDKRendererTarget* target)
{
  if (renderer == NULL || renderer->rhi == NULL || target == NULL)
  {
    return;
  }

  if (target->color_texture != LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_ui_pass_remove_texture_bindings(
        &renderer->ui_pass, target->color_texture);
    ldk_rhi_texture_destroy(renderer->rhi, target->color_texture);
  }

  if (target->depth_texture != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, target->depth_texture);
  }

  memset(target, 0, sizeof(*target));
}

static bool s_renderer_target_create(LDKRenderer* renderer,
    LDKRendererTarget* target, i32 width, i32 height,
    LDKRHIFormat color_format)
{
  if (renderer == NULL || renderer->rhi == NULL || target == NULL)
  {
    return false;
  }

  if (width <= 0 || height <= 0)
  {
    return false;
  }

  target->color_format = color_format;
  target->depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  LDKRHITextureDesc color_desc = {0};
  ldk_rhi_texture_desc_defaults(&color_desc);
  color_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  color_desc.format = target->color_format;
  color_desc.width = (u32)width;
  color_desc.height = (u32)height;
  color_desc.depth = 1;
  color_desc.mip_count = 1;
  color_desc.layer_count = 1;
  color_desc.usage =
      LDK_RHI_TEXTURE_USAGE_RENDER_TARGET | LDK_RHI_TEXTURE_USAGE_SAMPLED;

  target->color_texture =
      ldk_rhi_texture_create(renderer->rhi, &color_desc);
  if (target->color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHITextureDesc depth_desc = {0};
  ldk_rhi_texture_desc_defaults(&depth_desc);
  depth_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  depth_desc.format = target->depth_format;
  depth_desc.width = (u32)width;
  depth_desc.height = (u32)height;
  depth_desc.depth = 1;
  depth_desc.mip_count = 1;
  depth_desc.layer_count = 1;
  depth_desc.usage = LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL |
      LDK_RHI_TEXTURE_USAGE_SAMPLED;

  target->depth_texture =
      ldk_rhi_texture_create(renderer->rhi, &depth_desc);
  if (target->depth_texture == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, target->color_texture);
    memset(target, 0, sizeof(*target));
    return false;
  }

  target->width = width;
  target->height = height;
  return true;
}

static bool s_renderer_target_ensure(LDKRenderer* renderer,
    LDKRendererTarget* target, i32 width, i32 height,
    LDKRHIFormat color_format)
{
  if (target != NULL &&
      target->color_texture != LDK_RHI_INVALID_RESOURCE &&
      target->depth_texture != LDK_RHI_INVALID_RESOURCE &&
      target->width == width && target->height == height &&
      target->color_format == color_format)
  {
    return true;
  }

  s_renderer_target_destroy(renderer, target);
  return s_renderer_target_create(
      renderer, target, width, height, color_format);
}

static bool s_renderer_view_extent_apply(
    LDKRenderer* renderer, LDKRendererView* view, u32 width, u32 height)
{
  if (renderer == NULL || view == NULL || width == 0 || height == 0)
  {
    return false;
  }

  if (view->width == width && view->height == height)
  {
    return true;
  }

  view->width = width;
  view->height = height;
  s_renderer_post_process_view_destroy(renderer, view);
  s_renderer_target_destroy(renderer, &view->overlay_target);

  if (view->target.color_texture == LDK_RHI_INVALID_RESOURCE ||
      view->target.depth_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return true;
  }

  LDKRHIFormat color_format = view->target.color_format;
  if (color_format == LDK_RHI_FORMAT_INVALID)
  {
    color_format = LDK_RHI_FORMAT_RGBA8_UNORM;
  }

  return s_renderer_target_ensure(renderer, &view->target,
      (i32)width, (i32)height, color_format);
}

static LDKRendererView* s_renderer_view_find(
    LDKRenderer* renderer, LDKRendererViewId view_id)
{
  if (renderer == NULL || view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return NULL;
  }

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    LDKRendererView* view = &renderer->views[i];
    if (view->id == view_id)
    {
      return view;
    }
  }

  return NULL;
}

static LDKRendererView const* s_renderer_view_find_const(
    LDKRenderer const* renderer, LDKRendererViewId view_id)
{
  if (renderer == NULL || view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return NULL;
  }

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    LDKRendererView const* view = &renderer->views[i];
    if (view->id == view_id)
    {
      return view;
    }
  }

  return NULL;
}

static bool s_renderer_grow_view_cache(LDKRenderer* renderer)
{
  u32 old_capacity = renderer->view_capacity;
  u32 new_capacity = old_capacity == 0 ? 4 : old_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererView);
  LDKRendererView* new_views = renderer->views == NULL
      ? (LDKRendererView*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererView*)LDK_RENDERER_REALLOC(renderer->views, new_size);

  if (new_views == NULL)
  {
    return false;
  }

  memset(new_views + old_capacity, 0,
      (size_t)(new_capacity - old_capacity) * sizeof(LDKRendererView));
  renderer->views = new_views;
  renderer->view_capacity = new_capacity;
  return true;
}

static void s_renderer_destroy_views(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    s_renderer_post_process_view_destroy(renderer, &renderer->views[i]);
    s_renderer_target_destroy(renderer, &renderer->views[i].target);
    s_renderer_target_destroy(renderer, &renderer->views[i].overlay_target);
  }

  LDK_RENDERER_FREE(renderer->views);
  renderer->views = NULL;
  renderer->view_count = 0;
  renderer->view_capacity = 0;
  renderer->game_view = LDK_RENDERER_VIEW_INVALID;
}

static void s_renderer_finish_views(LDKRenderer* renderer)
{
  u32 index = 0;

  while (index < renderer->view_count)
  {
    LDKRendererView* view = &renderer->views[index];

    if (view->submitted)
    {
      view->submitted = false;
      view->grid_submitted = false;
      view->skybox = ldk_renderer_skybox_null();
      view->separate_overlay = false;
      index += 1;
      continue;
    }

    s_renderer_post_process_view_destroy(renderer, view);
    s_renderer_target_destroy(renderer, &view->target);
    s_renderer_target_destroy(renderer, &view->overlay_target);
    renderer->view_count -= 1;

    if (index != renderer->view_count)
    {
      *view = renderer->views[renderer->view_count];
    }

    memset(&renderer->views[renderer->view_count], 0,
        sizeof(LDKRendererView));
  }

  renderer->game_view = LDK_RENDERER_VIEW_INVALID;
}

// ---------------------------------------------------------------------------
// Internal renderer resources: Mesh
// ---------------------------------------------------------------------------

LDKResourceMesh ldk_renderer_mesh_null(void)
{
  return LDK_RESOURCE_MESH_INVALID;
}

static LDKRendererMeshResource* s_renderer_mesh_get_resource(
    LDKRenderer* renderer, LDKResourceMesh mesh)
{
  if (renderer == NULL || mesh.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)(mesh.id - 1u);
  if (index >= renderer->mesh_count)
  {
    return NULL;
  }

  LDKRendererMeshResource* resource = &renderer->meshes[index];
  if (!resource->alive)
  {
    return NULL;
  }

  return resource;
}

bool ldk_renderer_mesh_is_valid(LDKRenderer* renderer, LDKResourceMesh mesh)
{
  return s_renderer_mesh_get_resource(renderer, mesh) != NULL;
}

static bool s_renderer_grow_mesh_cache(LDKRenderer* renderer)
{
  u32 new_capacity =
      renderer->mesh_capacity == 0 ? 64 : renderer->mesh_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMeshResource);
  LDKRendererMeshResource* new_meshes = renderer->meshes == NULL
      ? (LDKRendererMeshResource*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererMeshResource*)LDK_RENDERER_REALLOC(
            renderer->meshes, new_size);

  if (new_meshes == NULL)
  {
    return false;
  }

  memset(new_meshes + renderer->mesh_capacity, 0,
      (size_t)(new_capacity - renderer->mesh_capacity) *
          sizeof(LDKRendererMeshResource));

  renderer->meshes = new_meshes;
  renderer->mesh_capacity = new_capacity;
  return true;
}

static bool s_renderer_mesh_desc_is_valid(
    LDKRendererMeshDesc const* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->vertices == NULL || desc->vertex_count == 0)
  {
    return false;
  }

  if (desc->indices == NULL || desc->index_count == 0)
  {
    return false;
  }

  if (desc->vertex_count > UINT32_MAX / (u32)sizeof(LDKMeshVertex) ||
      desc->index_count > UINT32_MAX / (u32)sizeof(u32))
  {
    return false;
  }

  return true;
}

static const LDKMeshVertex* s_renderer_mesh_upload_vertices_prepare(
    LDKRendererMeshDesc const* desc, LDKMeshVertex** out_owned)
{
  LDKMeshVertex* sanitized_vertices;
  size_t vertex_bytes;

  if (out_owned == NULL || desc == NULL)
  {
    return NULL;
  }

  *out_owned = NULL;
  if (desc->has_tangents)
  {
    return desc->vertices;
  }

  vertex_bytes = (size_t)desc->vertex_count * sizeof(LDKMeshVertex);
  sanitized_vertices = (LDKMeshVertex*)LDK_RENDERER_ALLOC(vertex_bytes);
  if (sanitized_vertices == NULL)
  {
    return NULL;
  }

  for (u32 i = 0; i < desc->vertex_count; ++i)
  {
    sanitized_vertices[i].position = desc->vertices[i].position;
    sanitized_vertices[i].normal = desc->vertices[i].normal;
    sanitized_vertices[i].uv = desc->vertices[i].uv;
    sanitized_vertices[i].color = desc->vertices[i].color;
    sanitized_vertices[i].tangent =
        vec4_make(0.0f, 0.0f, 0.0f, 0.0f);
  }

  *out_owned = sanitized_vertices;
  return sanitized_vertices;
}

static bool s_renderer_mesh_resource_create_buffers(LDKRenderer* renderer,
    LDKRendererMeshResource* resource, LDKRendererMeshDesc const* desc)
{
  LDKMeshVertex* owned_vertices = NULL;
  const LDKMeshVertex* upload_vertices =
      s_renderer_mesh_upload_vertices_prepare(desc, &owned_vertices);
  if (upload_vertices == NULL)
  {
    return false;
  }

  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size =
      desc->vertex_count * (u32)sizeof(LDKMeshVertex);
  vertex_desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = upload_vertices;

  resource->vertex_buffer =
      ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  LDK_RENDERER_FREE(owned_vertices);
  if (resource->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = desc->index_count * (u32)sizeof(u32);
  index_desc.usage =
      LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  index_desc.initial_data = desc->indices;

  resource->index_buffer =
      ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (resource->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
    resource->vertex_buffer = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  resource->vertex_count = desc->vertex_count;
  resource->index_count = desc->index_count;
  resource->vertex_capacity = desc->vertex_count;
  resource->index_capacity = desc->index_count;
  return true;
}

LDKResourceMesh ldk_renderer_mesh_create(
    LDKRenderer* renderer, LDKRendererMeshDesc const* desc)
{
  LDKResourceMesh invalid = ldk_renderer_mesh_null();

  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_mesh_desc_is_valid(desc))
  {
    return invalid;
  }

  if (renderer->mesh_count == renderer->mesh_capacity)
  {
    if (!s_renderer_grow_mesh_cache(renderer))
    {
      return invalid;
    }
  }

  u32 index = renderer->mesh_count;
  LDKRendererMeshResource* resource = &renderer->meshes[index];
  memset(resource, 0, sizeof(*resource));

  if (!s_renderer_mesh_resource_create_buffers(renderer, resource, desc))
  {
    memset(resource, 0, sizeof(*resource));
    return invalid;
  }

  resource->alive = true;
  resource->ref_count = 1;
  renderer->mesh_count += 1;

  LDKResourceMesh mesh = {0};
  mesh.id = (LDKRHIResource)(index + 1u);
  return mesh;
}

LDKResourceMesh ldk_renderer_mesh_asset_acquire(LDKRenderer *renderer,
    LDKAssetMesh asset, u32 mesh_index, const LDKRendererMeshDesc *desc)
{
  if (!renderer || x_handle_is_null(asset.h) ||
      !s_renderer_mesh_desc_is_valid(desc))
    return ldk_renderer_mesh_null();
  for (u32 i = 0; i < renderer->mesh_count; ++i)
  {
    LDKRendererMeshResource *resource = &renderer->meshes[i];
    if (resource->alive && resource->shared_asset_mesh &&
        resource->ref_count < UINT32_MAX &&
        resource->source_asset.h.index == asset.h.index &&
        resource->source_asset.h.version == asset.h.version &&
        resource->source_mesh_index == mesh_index)
    {
      ++resource->ref_count;
      LDKResourceMesh mesh = {0};
      mesh.id = (LDKRHIResource)(i + 1u);
      return mesh;
    }
  }
  LDKResourceMesh mesh = ldk_renderer_mesh_create(renderer, desc);
  LDKRendererMeshResource *resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource)
  {
    resource->source_asset = asset;
    resource->source_mesh_index = mesh_index;
    resource->shared_asset_mesh = true;
  }
  return mesh;
}

bool ldk_renderer_mesh_update(LDKRenderer* renderer, LDKResourceMesh mesh,
    LDKRendererMeshDesc const* desc)
{
  LDKRendererMeshResource* resource;
  LDKMeshVertex* owned_vertices = NULL;
  const LDKMeshVertex* upload_vertices;

  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_mesh_desc_is_valid(desc))
  {
    return false;
  }

  resource = s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  /* Reuse the existing GPU objects while the new geometry fits. Procedural
   * meshes such as IslandTerrain rebuild frequently as their streamed tile
   * window moves; destroying and recreating both buffers for each rebuild is
   * unnecessary driver churn. */
  if (desc->vertex_count <= resource->vertex_capacity &&
      desc->index_count <= resource->index_capacity)
  {
    upload_vertices =
        s_renderer_mesh_upload_vertices_prepare(desc, &owned_vertices);
    if (upload_vertices == NULL)
    {
      return false;
    }

    bool vertex_updated = ldk_rhi_buffer_update(renderer->rhi,
        resource->vertex_buffer, 0,
        desc->vertex_count * (u32)sizeof(LDKMeshVertex),
        upload_vertices);
    LDK_RENDERER_FREE(owned_vertices);
    if (!vertex_updated ||
        !ldk_rhi_buffer_update(renderer->rhi, resource->index_buffer, 0,
            desc->index_count * (u32)sizeof(u32), desc->indices))
    {
      return false;
    }

    resource->vertex_count = desc->vertex_count;
    resource->index_count = desc->index_count;
    return true;
  }

  /* Growth is transactional. Build a complete replacement before touching
   * the live mesh so a failed allocation cannot turn a valid mesh into an
   * invalid one. Capacity then becomes a high-water mark for later updates. */
  LDKRendererMeshResource replacement = {0};
  if (!s_renderer_mesh_resource_create_buffers(renderer, &replacement, desc))
  {
    return false;
  }

  LDKRHIBuffer old_vertex_buffer = resource->vertex_buffer;
  LDKRHIBuffer old_index_buffer = resource->index_buffer;
  bool alive = resource->alive;

  *resource = replacement;
  resource->alive = alive;

  ldk_rhi_buffer_destroy(renderer->rhi, old_vertex_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, old_index_buffer);
  return true;
}

void ldk_renderer_mesh_destroy(LDKRenderer* renderer, LDKResourceMesh mesh)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return;
  }

  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return;
  }

  if (resource->ref_count > 1)
  {
    --resource->ref_count;
    return;
  }
  ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->index_buffer);
  memset(resource, 0, sizeof(*resource));
}

static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->mesh_count; i++)
    {
      LDKResourceMesh mesh = {0};
      mesh.id = (LDKRHIResource)(i + 1u);
      renderer->meshes[i].ref_count = 1;
      ldk_renderer_mesh_destroy(renderer, mesh);
    }
  }

  LDK_RENDERER_FREE(renderer->meshes);
  renderer->meshes = NULL;
  renderer->mesh_count = 0;
  renderer->mesh_capacity = 0;

  LDK_RENDERER_FREE(renderer->submitted_lines);
  renderer->submitted_lines = NULL;
  renderer->submitted_line_count = 0;
  renderer->submitted_line_capacity = 0;
  renderer->line_mesh = ldk_renderer_mesh_null();

  LDK_RENDERER_FREE(renderer->submitted_wireframes);
  renderer->submitted_wireframes = NULL;
  renderer->submitted_wireframe_count = 0;
  renderer->submitted_wireframe_capacity = 0;

  LDK_RENDERER_FREE(renderer->submitted_lights);
  renderer->submitted_lights = NULL;
  renderer->submitted_light_count = 0;
  renderer->submitted_light_capacity = 0;

  LDK_RENDERER_FREE(renderer->submitted_instance_worlds);
  LDK_RENDERER_FREE(renderer->submitted_instance_colors);
  renderer->submitted_instance_worlds = NULL;
  renderer->submitted_instance_colors = NULL;
  renderer->submitted_instance_count = 0;
  renderer->submitted_instance_capacity = 0;

  LDK_RENDERER_FREE(renderer->submitted_meshes);
  renderer->submitted_meshes = NULL;
  renderer->submitted_mesh_count = 0;
  renderer->submitted_mesh_capacity = 0;

  LDK_RENDERER_FREE(renderer->mesh_sort_items);
  LDK_RENDERER_FREE(renderer->mesh_sort_scratch);
  LDK_RENDERER_FREE(renderer->mesh_sort_mesh_ids);
  LDK_RENDERER_FREE(renderer->mesh_sort_material_ids);
  renderer->mesh_sort_items = NULL;
  renderer->mesh_sort_scratch = NULL;
  renderer->mesh_sort_mesh_ids = NULL;
  renderer->mesh_sort_material_ids = NULL;
  renderer->mesh_sort_capacity = 0;
  renderer->mesh_sort_mesh_id_capacity = 0;
  renderer->mesh_sort_material_id_capacity = 0;
}

LDKResourceInstanceSet ldk_renderer_instance_set_null(void)
{
  return LDK_RESOURCE_INSTANCE_SET_INVALID;
}

static LDKRendererInstanceSetResource *s_renderer_instance_set_get_resource(
    LDKRenderer *renderer, LDKResourceInstanceSet instance_set)
{
  if (renderer == NULL || instance_set.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)instance_set.id - 1u;
  if (index >= renderer->instance_set_count ||
      !renderer->instance_sets[index].alive)
  {
    return NULL;
  }
  return &renderer->instance_sets[index];
}

bool ldk_renderer_instance_set_is_valid(
    LDKRenderer *renderer, LDKResourceInstanceSet instance_set)
{
  return s_renderer_instance_set_get_resource(renderer, instance_set) != NULL;
}

static bool s_renderer_grow_instance_set_cache(LDKRenderer *renderer)
{
  u32 capacity = renderer->instance_set_capacity == 0u
      ? 16u : renderer->instance_set_capacity * 2u;
  if (capacity <= renderer->instance_set_capacity ||
      capacity > UINT32_MAX / sizeof(LDKRendererInstanceSetResource))
  {
    return false;
  }

  size_t size = (size_t)capacity * sizeof(LDKRendererInstanceSetResource);
  LDKRendererInstanceSetResource *resources =
      (LDKRendererInstanceSetResource *)LDK_RENDERER_REALLOC(
          renderer->instance_sets, size);
  if (resources == NULL)
  {
    return false;
  }

  memset(resources + renderer->instance_set_capacity, 0,
      (size_t)(capacity - renderer->instance_set_capacity) *
          sizeof(*resources));
  renderer->instance_sets = resources;
  renderer->instance_set_capacity = capacity;
  return true;
}

static bool s_renderer_instance_matrices_are_valid(
    const Mat4 *instances, u32 instance_count)
{
  if (instances == NULL || instance_count == 0u ||
      instance_count > UINT32_MAX / sizeof(Mat4))
  {
    return false;
  }

  for (u32 i = 0u; i < instance_count; ++i)
  {
    for (u32 element = 0u; element < 16u; ++element)
    {
      if (!isfinite(instances[i].m[element]))
      {
        return false;
      }
    }
  }
  return true;
}

static bool s_renderer_instance_set_buffers_create(LDKRenderer *renderer,
    LDKRendererInstanceSetResource *resource, const Mat4 *instances,
    u32 instance_count, u32 capacity)
{
  if (capacity < instance_count || capacity > UINT32_MAX / sizeof(Mat4) ||
      capacity > UINT32_MAX / sizeof(LDKRHIColor) ||
      capacity > UINT32_MAX / sizeof(Vec4))
  {
    return false;
  }

  LDKRHIColor *colors = (LDKRHIColor *)LDK_RENDERER_ALLOC(
      (size_t)capacity * sizeof(LDKRHIColor));
  if (colors == NULL)
  {
    return false;
  }
  for (u32 i = 0u; i < capacity; ++i)
  {
    colors[i] = ldk_renderer_color_from_rgba32(0xffffffffu);
  }

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = capacity * (u32)sizeof(Mat4);
  desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  desc.stats_class = LDK_RHI_BUFFER_STATS_INSTANCE;
  LDKRHIBuffer transform_buffer =
      ldk_rhi_buffer_create(renderer->rhi, &desc);

  desc.size = capacity * (u32)sizeof(LDKRHIColor);
  desc.initial_data = colors;
  LDKRHIBuffer color_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  LDK_RENDERER_FREE(colors);

  if (transform_buffer == LDK_RHI_INVALID_RESOURCE ||
      color_buffer == LDK_RHI_INVALID_RESOURCE ||
      !ldk_rhi_buffer_update(renderer->rhi, transform_buffer, 0,
          instance_count * (u32)sizeof(Mat4), instances))
  {
    ldk_rhi_buffer_destroy(renderer->rhi, transform_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, color_buffer);
    return false;
  }

  ldk_rhi_buffer_destroy(renderer->rhi, resource->transform_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->color_buffer);
  resource->transform_buffer = transform_buffer;
  resource->color_buffer = color_buffer;
  resource->instance_count = instance_count;
  resource->instance_capacity = capacity;
  return true;
}

LDKResourceInstanceSet ldk_renderer_instance_set_create(
    LDKRenderer *renderer, const Mat4 *instances, u32 instance_count)
{
  LDKResourceInstanceSet invalid = ldk_renderer_instance_set_null();
  if (renderer == NULL || !renderer->is_initialized ||
      renderer->rhi->functions.draw_indexed_instanced == NULL ||
      !s_renderer_instance_matrices_are_valid(instances, instance_count))
  {
    return invalid;
  }

  if (renderer->instance_set_count == renderer->instance_set_capacity &&
      !s_renderer_grow_instance_set_cache(renderer))
  {
    return invalid;
  }

  u32 capacity = 256u;
  while (capacity < instance_count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = instance_count;
      break;
    }
    capacity *= 2u;
  }

  u32 index = renderer->instance_set_count;
  LDKRendererInstanceSetResource *resource = &renderer->instance_sets[index];
  memset(resource, 0, sizeof(*resource));
  if (!s_renderer_instance_set_buffers_create(
          renderer, resource, instances, instance_count, capacity))
  {
    return invalid;
  }

  resource->alive = true;
  ++renderer->instance_set_count;
  LDKResourceInstanceSet result = {(LDKRHIResource)(index + 1u)};
  return result;
}

bool ldk_renderer_instance_set_update(LDKRenderer *renderer,
    LDKResourceInstanceSet instance_set, const Mat4 *instances,
    u32 instance_count)
{
  LDKRendererInstanceSetResource *resource =
      s_renderer_instance_set_get_resource(renderer, instance_set);
  if (resource == NULL)
  {
    return false;
  }
  if (instance_count == 0u)
  {
    resource->instance_count = 0u;
    return true;
  }
  if (!s_renderer_instance_matrices_are_valid(instances, instance_count))
  {
    return false;
  }

  if (instance_count > resource->instance_capacity)
  {
    u32 capacity = resource->instance_capacity;
    while (capacity < instance_count)
    {
      if (capacity > UINT32_MAX / 2u)
      {
        capacity = instance_count;
        break;
      }
      capacity *= 2u;
    }
    return s_renderer_instance_set_buffers_create(
        renderer, resource, instances, instance_count, capacity);
  }

  if (!ldk_rhi_buffer_update(renderer->rhi, resource->transform_buffer, 0,
          instance_count * (u32)sizeof(Mat4), instances))
  {
    return false;
  }
  resource->instance_count = instance_count;
  return true;
}

void ldk_renderer_instance_set_destroy(
    LDKRenderer *renderer, LDKResourceInstanceSet instance_set)
{
  LDKRendererInstanceSetResource *resource =
      s_renderer_instance_set_get_resource(renderer, instance_set);
  if (resource == NULL || renderer->rhi == NULL)
  {
    return;
  }

  ldk_rhi_buffer_destroy(renderer->rhi, resource->transform_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->color_buffer);
  memset(resource, 0, sizeof(*resource));
}

static void s_renderer_destroy_instance_set_resources(LDKRenderer *renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  for (u32 i = 0u; i < renderer->instance_set_count; ++i)
  {
    LDKResourceInstanceSet instance_set = {(LDKRHIResource)(i + 1u)};
    ldk_renderer_instance_set_destroy(renderer, instance_set);
  }
  LDK_RENDERER_FREE(renderer->instance_sets);
  renderer->instance_sets = NULL;
  renderer->instance_set_count = 0u;
  renderer->instance_set_capacity = 0u;
}

static bool s_renderer_grow_mesh_submit_queue(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->submitted_mesh_capacity == 0
      ? 256
      : renderer->submitted_mesh_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMeshSubmit);
  LDKRendererMeshSubmit* new_submits = renderer->submitted_meshes == NULL
      ? (LDKRendererMeshSubmit*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererMeshSubmit*)LDK_RENDERER_REALLOC(
            renderer->submitted_meshes, new_size);

  if (new_submits == NULL)
  {
    return false;
  }

  renderer->submitted_meshes = new_submits;
  renderer->submitted_mesh_capacity = new_capacity;
  return true;
}

/* Low 40 bits are the sortable state key: mesh 16, material 16, pipeline 8.
 * High 24 bits carry the original submitted_meshes index and are not sorted. */
#define LDK_RENDERER_MESH_SORT_MESH_SHIFT 0u
#define LDK_RENDERER_MESH_SORT_MATERIAL_SHIFT 16u
#define LDK_RENDERER_MESH_SORT_PIPELINE_SHIFT 32u
#define LDK_RENDERER_MESH_SORT_SUBMIT_SHIFT 40u
#define LDK_RENDERER_MESH_SORT_RADIX_PASSES 5u
#define LDK_RENDERER_MESH_SORT_COMPACT_ID_MAX UINT16_MAX
#define LDK_RENDERER_MESH_SORT_SUBMIT_INDEX_MAX 0x00ffffffu

LDK_STATIC_ASSERT(LDK_RENDERER_MATERIAL_SELECTION_VEGETATION <= UINT8_MAX,
    mesh_sort_pipeline_id_bits);

static bool s_renderer_mesh_sort_storage_ensure(
    LDKRenderer* renderer, u32 item_count)
{
  if (item_count <= renderer->mesh_sort_capacity)
  {
    return true;
  }

  u32 capacity =
      renderer->mesh_sort_capacity == 0 ? 256u : renderer->mesh_sort_capacity;
  while (capacity < item_count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = item_count;
      break;
    }
    capacity *= 2u;
  }

  size_t size = (size_t)capacity * sizeof(u64);
  if (capacity < item_count || size / sizeof(u64) != capacity)
  {
    return false;
  }

  u64* items = (u64*)LDK_RENDERER_ALLOC(size);
  u64* scratch = (u64*)LDK_RENDERER_ALLOC(size);
  if (items == NULL || scratch == NULL)
  {
    LDK_RENDERER_FREE(items);
    LDK_RENDERER_FREE(scratch);
    return false;
  }

  LDK_RENDERER_FREE(renderer->mesh_sort_items);
  LDK_RENDERER_FREE(renderer->mesh_sort_scratch);
  renderer->mesh_sort_items = items;
  renderer->mesh_sort_scratch = scratch;
  renderer->mesh_sort_capacity = capacity;
  return true;
}

static bool s_renderer_mesh_sort_id_map_ensure(
    u32** map, u32* map_capacity, u32 item_count)
{
  if (item_count <= *map_capacity)
  {
    return true;
  }

  u32 capacity = *map_capacity == 0 ? 256u : *map_capacity;
  while (capacity < item_count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = item_count;
      break;
    }
    capacity *= 2u;
  }

  size_t size = (size_t)capacity * sizeof(u32);
  if (capacity < item_count || size / sizeof(u32) != capacity)
  {
    return false;
  }

  u32* new_map = *map == NULL
      ? (u32*)LDK_RENDERER_ALLOC(size)
      : (u32*)LDK_RENDERER_REALLOC(*map, size);
  if (new_map == NULL)
  {
    return false;
  }

  *map = new_map;
  *map_capacity = capacity;
  return true;
}

static bool s_renderer_mesh_sort_resources_ensure(LDKRenderer* renderer)
{
  return s_renderer_mesh_sort_storage_ensure(
             renderer, renderer->submitted_mesh_count) &&
         s_renderer_mesh_sort_id_map_ensure(&renderer->mesh_sort_mesh_ids,
             &renderer->mesh_sort_mesh_id_capacity, renderer->mesh_count) &&
         s_renderer_mesh_sort_id_map_ensure(&renderer->mesh_sort_material_ids,
             &renderer->mesh_sort_material_id_capacity,
             renderer->material_count);
}

static u64 s_renderer_mesh_sort_item_make(u32 submit_index, u32 pipeline_id,
    u32 material_id, u32 mesh_id)
{
  return ((u64)submit_index << LDK_RENDERER_MESH_SORT_SUBMIT_SHIFT) |
         ((u64)pipeline_id << LDK_RENDERER_MESH_SORT_PIPELINE_SHIFT) |
         ((u64)material_id << LDK_RENDERER_MESH_SORT_MATERIAL_SHIFT) |
         ((u64)mesh_id << LDK_RENDERER_MESH_SORT_MESH_SHIFT);
}

static u32 s_renderer_mesh_sort_item_submit_index(u64 item)
{
  return (u32)(item >> LDK_RENDERER_MESH_SORT_SUBMIT_SHIFT);
}

static bool s_renderer_material_is_blended(
    const LDKRendererMaterialResource *material)
{
  return material &&
      (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_BLEND ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT_BLEND);
}

/* Textured alpha-blended materials cast binary shadows using their
 * opacity, without changing their color-pass blending behavior. */
static bool s_renderer_shadow_material_uses_alpha_test(
    const LDKRendererMaterialResource *material)
{
  return material &&
      (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT ||
          material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND);
}

static bool s_renderer_mesh_material_excluded_from_queue(
    const LDKRendererMaterialResource *material, bool shadow_queue)
{
  return s_renderer_material_is_blended(material) &&
      (!shadow_queue || !s_renderer_shadow_material_uses_alpha_test(material));
}

static u64* s_renderer_mesh_sort_radix(
    u64* items, u64* scratch, u32 item_count)
{
  if (item_count < 2)
  {
    return items;
  }

  u64* source = items;
  u64* destination = scratch;
  for (u32 pass = 0; pass < LDK_RENDERER_MESH_SORT_RADIX_PASSES; ++pass)
  {
    u32 buckets[256] = {0};
    u32 shift = pass * 8u;
    for (u32 i = 0; i < item_count; ++i)
    {
      u32 bucket = (u32)((source[i] >> shift) & 0xffu);
      ++buckets[bucket];
    }

    u32 offset = 0;
    for (u32 bucket = 0; bucket < 256u; ++bucket)
    {
      u32 count = buckets[bucket];
      buckets[bucket] = offset;
      offset += count;
    }

    for (u32 i = 0; i < item_count; ++i)
    {
      u32 bucket = (u32)((source[i] >> shift) & 0xffu);
      destination[buckets[bucket]++] = source[i];
    }

    u64* swap = source;
    source = destination;
    destination = swap;
  }

  return source;
}

static bool s_renderer_mesh_sort_queue_build(LDKRenderer* renderer,
    LDKRendererView const* view, u32 required_flags, u64** out_items,
    u32* out_item_count)
{
  if (renderer == NULL || view == NULL || out_items == NULL ||
      out_item_count == NULL)
  {
    return false;
  }

  *out_items = NULL;
  *out_item_count = 0;
  if (renderer->submitted_mesh_count == 0)
  {
    return true;
  }

  if (!s_renderer_mesh_sort_resources_ensure(renderer))
  {
    return false;
  }

  if (renderer->mesh_count > 0)
  {
    memset(renderer->mesh_sort_mesh_ids, 0xff,
        (size_t)renderer->mesh_count * sizeof(u32));
  }
  if (renderer->material_count > 0)
  {
    memset(renderer->mesh_sort_material_ids, 0xff,
        (size_t)renderer->material_count * sizeof(u32));
  }

  u32 next_mesh_id = 0;
  u32 next_material_id = 0;
  u32 item_count = 0;
  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[i];
    if ((submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0 ||
        (submit->flags & required_flags) != required_flags ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    LDKRendererMeshResource* mesh =
        s_renderer_mesh_get_resource(renderer, submit->mesh);
    LDKRendererMaterialResource* material =
        s_renderer_material_get_resource(renderer, submit->material);
    if (mesh == NULL || mesh->index_count == 0 || material == NULL ||
        material->selection == LDK_RENDERER_MATERIAL_SELECTION_INVALID ||
        s_renderer_mesh_material_excluded_from_queue(material,
            (required_flags & LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS) != 0))
    {
      continue;
    }

    if (i > LDK_RENDERER_MESH_SORT_SUBMIT_INDEX_MAX ||
        material->render_key > UINT8_MAX)
    {
      return false;
    }

    u32 mesh_index = (u32)(submit->mesh.id - 1u);
    u32 material_index = (u32)(submit->material.id - 1u);
    u32 mesh_id = renderer->mesh_sort_mesh_ids[mesh_index];
    if (mesh_id == UINT32_MAX)
    {
      if (next_mesh_id > LDK_RENDERER_MESH_SORT_COMPACT_ID_MAX)
      {
        return false;
      }
      mesh_id = next_mesh_id++;
      renderer->mesh_sort_mesh_ids[mesh_index] = mesh_id;
    }

    u32 material_id = renderer->mesh_sort_material_ids[material_index];
    if (material_id == UINT32_MAX)
    {
      if (next_material_id > LDK_RENDERER_MESH_SORT_COMPACT_ID_MAX)
      {
        return false;
      }
      material_id = next_material_id++;
      renderer->mesh_sort_material_ids[material_index] = material_id;
    }

    renderer->mesh_sort_items[item_count++] = s_renderer_mesh_sort_item_make(
        i, (u32)material->render_key, material_id, mesh_id);
  }

  *out_items = s_renderer_mesh_sort_radix(renderer->mesh_sort_items,
      renderer->mesh_sort_scratch, item_count);
  *out_item_count = item_count;
  return true;
}

static bool s_renderer_mesh_submit_same_batch(
    const LDKRendererMeshSubmit* a, const LDKRendererMeshSubmit* b)
{
  if (a == NULL || b == NULL || a->instance_count || b->instance_count ||
      a->instance_set.id != LDK_RHI_INVALID_RESOURCE ||
      b->instance_set.id != LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  /* The opaque queue already fixes the view/pass. CAST_SHADOWS belongs to
   * the shadow pass and must not split the visible batch. */
  return a->mesh.id == b->mesh.id && a->material.id == b->material.id &&
    a->first_index == b->first_index &&
          a->index_count == b->index_count &&
          ((a->flags ^ b->flags) &
              (LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD |
                  LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE)) == 0u;
}

static u32 s_renderer_mesh_submit_instance_count(
    LDKRenderer *renderer, const LDKRendererMeshSubmit *submit)
{
  if (submit == NULL)
  {
    return 0u;
  }
  if (submit->instance_count != 0u)
  {
    return submit->instance_count;
  }

  LDKRendererInstanceSetResource *resource =
      s_renderer_instance_set_get_resource(renderer, submit->instance_set);
  return resource ? resource->instance_count : 0u;
}

typedef struct LDKRendererMeshCameraParams
{
  Mat4 view;
  Mat4 projection;
  float camera_position[4];
} LDKRendererMeshCameraParams;

typedef struct LDKRendererMeshObjectParams
{
  Mat4 world;
  LDKRHIColor instance_color;
  float options[4];
  float uv_rect[4];
} LDKRendererMeshObjectParams;

typedef struct LDKRendererMeshMaterialParams
{
  LDKRHIColor color;
  /* GLSL u_surface: specular, shininess, emission, alpha cutoff. */
  float specular;
  float shininess;
  float emission;
  float alpha_cutoff;
  /* Curvature, wind strength, wind speed, wind direction radians. */
  float vegetation[4];
  /* UV scale.xy, offset.xy. */
  float uv_transform[LDK_MATERIAL_TEXTURE_SLOT_COUNT][4];
  /* Interaction origin.xz, inverse world size, recovery time seconds. */
  float vegetation_interaction[4];
  /* Terrain surface count in x. */
  float terrain_info[4];
  /* min height, blend width, texture weight, unused. */
  float terrain_range[LDK_RENDERER_TERRAIN_SURFACE_COUNT][4];
  float terrain_color[LDK_RENDERER_TERRAIN_SURFACE_COUNT][4];
  /* u0, v0, u1, v1. */
  float terrain_atlas_rect[LDK_RENDERER_TERRAIN_SURFACE_COUNT][4];
  /* uv scale xy. */
  float terrain_uv_scale[LDK_RENDERER_TERRAIN_SURFACE_COUNT][4];
} LDKRendererMeshMaterialParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererMeshCameraParams) == 144,
    mesh_camera_std140_size);
LDK_STATIC_ASSERT(sizeof(LDKRendererMeshObjectParams) == 112,
    mesh_object_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshObjectParams, instance_color) == 64,
    mesh_object_color_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshObjectParams, options) == 80,
    mesh_object_options_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshCameraParams, camera_position) == 128,
    mesh_camera_position_std140_offset);
LDK_STATIC_ASSERT(sizeof(LDKRendererMeshMaterialParams) ==
        96 + (LDK_MATERIAL_TEXTURE_SLOT_COUNT - 1u) * 16 +
            LDK_RENDERER_TERRAIN_SURFACE_COUNT * 64,
    mesh_material_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshMaterialParams, specular) == 16,
    mesh_material_surface_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshMaterialParams, shininess) == 20,
    mesh_material_shininess_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshMaterialParams, emission) == 24,
    mesh_material_emission_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererMeshMaterialParams, alpha_cutoff) == 28,
    mesh_material_alpha_cutoff_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshMaterialParams, vegetation) == 32,
    mesh_material_vegetation_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererMeshMaterialParams, uv_transform) == 48,
    mesh_material_uv_transform_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererMeshMaterialParams, vegetation_interaction) ==
        64 + (LDK_MATERIAL_TEXTURE_SLOT_COUNT - 1u) * 16,
    mesh_material_interaction_std140_offset);

typedef struct LDKRendererShadowCameraParams
{
  Mat4 view_projection;
  float animation[4];
} LDKRendererShadowCameraParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererShadowCameraParams) == 80,
    shadow_camera_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererShadowCameraParams, animation) == 64,
    shadow_camera_animation_std140_offset);

typedef struct LDKRendererShadowMaterialParams
{
  float color_alpha;
  float alpha_cutoff;
  float padding[2];
  float vegetation[4];
  float uv_transform[4];
  float vegetation_interaction[4];
} LDKRendererShadowMaterialParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererShadowMaterialParams) == 64,
    shadow_material_std140_size);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererShadowMaterialParams, alpha_cutoff) == 4,
    shadow_material_alpha_cutoff_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererShadowMaterialParams, vegetation) == 16,
    shadow_material_vegetation_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererShadowMaterialParams, uv_transform) == 32,
    shadow_material_uv_transform_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererShadowMaterialParams, vegetation_interaction) == 48,
    shadow_material_interaction_std140_offset);

typedef struct LDKRendererLightParams
{
  float position_type[4];
  float direction_range[4];
  float color_intensity[4];
  float cone[4];
} LDKRendererLightParams;

typedef struct LDKRendererLightingParams
{
  i32 count[4];
  float ambient_sky[4];
  float ambient_ground[4];
  LDKRendererLightParams lights[LDK_RENDERER_MAX_LIGHTS_PER_VIEW];
  Mat4 shadow_view_projection;
  float shadow_params[4]; // depth bias, slope bias, reserved, reserved
  float fog_color[4]; // Linear RGB, alpha ignored.
  float fog_params[4]; // start, effective density, reserved, reserved.
} LDKRendererLightingParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererLightParams) == 64, light_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererLightingParams, lights) == 48,
    lighting_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererLightingParams, shadow_view_projection) == 1072,
    shadow_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererLightingParams, fog_color) == 1152,
    fog_color_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererLightingParams, fog_params) == 1168,
    fog_params_std140_offset);
LDK_STATIC_ASSERT(
    sizeof(LDKRendererLightingParams) == 1184, lighting_std140_size);

typedef struct LDKRendererViewLighting
{
  LDKRendererLightingParams params;
  const LDKRendererLightSubmit *shadow_light;
} LDKRendererViewLighting;

static bool s_renderer_shadow_settings_valid(u32 resolution, float distance)
{
  return resolution >= 256 && resolution <= 8192 &&
         (resolution & (resolution - 1)) == 0 && isfinite(distance) &&
         distance > 0;
}
#define LDK_RENDERER_SHADOW_CASTER_MARGIN 60.0f

bool ldk_renderer_ambient_light_set(LDKRenderer *renderer,
    u32 sky_color, u32 ground_color, float intensity)
{
  if (!renderer || !renderer->is_initialized ||
      !isfinite(intensity) || intensity < 0.0f)
  {
    return false;
  }

  renderer->ambient_light.sky_color = sky_color;
  renderer->ambient_light.ground_color = ground_color;
  renderer->ambient_light.intensity = intensity;
  return true;
}

bool ldk_renderer_fog_set(LDKRenderer *renderer,
    bool enabled, u32 color, float density, float start)
{
  if (!renderer || !renderer->is_initialized || !isfinite(density) ||
      density < 0.0f || !isfinite(start) || start < 0.0f)
  {
    return false;
  }

  renderer->fog.enabled = enabled;
  renderer->fog.color = color;
  renderer->fog.density = density;
  renderer->fog.start = start;
  return true;
}

void ldk_renderer_vegetation_interaction_clear(LDKRenderer *renderer)
{
  if (!renderer)
  {
    return;
  }

  if (renderer->rhi)
  {
    ldk_rhi_bindings_destroy(
        renderer->rhi, renderer->mesh_pass.vegetation_bindings);
    ldk_rhi_bindings_destroy(
        renderer->rhi, renderer->shadow_pass.vegetation_bindings);
  }

  renderer->mesh_pass.vegetation_bindings = LDK_RHI_INVALID_RESOURCE;
  renderer->shadow_pass.vegetation_bindings = LDK_RHI_INVALID_RESOURCE;
  renderer->vegetation_interaction_texture = ldk_renderer_texture_null();
  renderer->vegetation_interaction_origin = vec2_make(0.0f, 0.0f);
  renderer->vegetation_interaction_inv_world_size = 0.0f;
  renderer->vegetation_interaction_enabled = false;
}

bool ldk_renderer_vegetation_interaction_set(LDKRenderer *renderer,
    LDKResourceTexture texture, Vec2 world_origin, float world_size)
{
  LDKRendererTextureResource *resource;
  LDKRHIBindings mesh_bindings = LDK_RHI_INVALID_RESOURCE;
  LDKRHIBindings shadow_bindings = LDK_RHI_INVALID_RESOURCE;

  if (!renderer || !renderer->is_initialized || !renderer->rhi ||
      !isfinite(world_origin.x) || !isfinite(world_origin.y) ||
      !isfinite(world_size) || world_size <= 0.0f)
  {
    return false;
  }

  resource = s_renderer_texture_get_resource(renderer, texture);
  if (!resource)
  {
    return false;
  }

  if (renderer->vegetation_interaction_texture.id != texture.id ||
      renderer->mesh_pass.vegetation_bindings == LDK_RHI_INVALID_RESOURCE ||
      renderer->shadow_pass.vegetation_bindings == LDK_RHI_INVALID_RESOURCE)
  {
    LDKRHIBindingsDesc desc = {0};
    ldk_rhi_bindings_desc_defaults(&desc);
    desc.layout = renderer->mesh_pass.bindings_layout;
    desc.binding_count = 6;
    desc.bindings[0].slot = 0;
    desc.bindings[0].buffer = renderer->mesh_pass.camera_buffer;
    desc.bindings[0].buffer_size = sizeof(LDKRendererMeshCameraParams);
    desc.bindings[1].slot = 1;
    desc.bindings[1].buffer = renderer->mesh_pass.object_buffer;
    desc.bindings[1].buffer_size = sizeof(LDKRendererMeshObjectParams);
    desc.bindings[2].slot = 2;
    desc.bindings[2].buffer = renderer->mesh_pass.material_buffer;
    desc.bindings[2].buffer_size = sizeof(LDKRendererMeshMaterialParams);
    desc.bindings[3].slot = 4;
    desc.bindings[3].buffer = renderer->mesh_pass.lighting_buffer;
    desc.bindings[3].buffer_size = sizeof(LDKRendererLightingParams);
    desc.bindings[4].slot = 5;
    desc.bindings[4].texture = renderer->mesh_pass.shadow_texture;
    desc.bindings[4].sampler = renderer->mesh_pass.shadow_sampler;
    desc.bindings[5].slot = 8;
    desc.bindings[5].texture = resource->texture;
    desc.bindings[5].sampler = resource->sampler;
    mesh_bindings = ldk_rhi_bindings_create(renderer->rhi, &desc);
    if (mesh_bindings == LDK_RHI_INVALID_RESOURCE)
    {
      return false;
    }

    ldk_rhi_bindings_desc_defaults(&desc);
    desc.layout = renderer->shadow_pass.bindings_layout;
    desc.binding_count = 4;
    desc.bindings[0].slot = 0;
    desc.bindings[0].buffer = renderer->shadow_pass.camera_buffer;
    desc.bindings[0].buffer_size = sizeof(LDKRendererShadowCameraParams);
    desc.bindings[1].slot = 1;
    desc.bindings[1].buffer = renderer->shadow_pass.object_buffer;
    desc.bindings[1].buffer_size = sizeof(Mat4);
    desc.bindings[2].slot = 2;
    desc.bindings[2].buffer = renderer->shadow_pass.material_buffer;
    desc.bindings[2].buffer_size = sizeof(LDKRendererShadowMaterialParams);
    desc.bindings[3].slot = 4;
    desc.bindings[3].texture = resource->texture;
    desc.bindings[3].sampler = resource->sampler;
    shadow_bindings = ldk_rhi_bindings_create(renderer->rhi, &desc);
    if (shadow_bindings == LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_bindings_destroy(renderer->rhi, mesh_bindings);
      return false;
    }

    ldk_rhi_bindings_destroy(
        renderer->rhi, renderer->mesh_pass.vegetation_bindings);
    ldk_rhi_bindings_destroy(
        renderer->rhi, renderer->shadow_pass.vegetation_bindings);
    renderer->mesh_pass.vegetation_bindings = mesh_bindings;
    renderer->shadow_pass.vegetation_bindings = shadow_bindings;
  }

  renderer->vegetation_interaction_texture = texture;
  renderer->vegetation_interaction_origin = world_origin;
  renderer->vegetation_interaction_inv_world_size = 1.0f / world_size;
  renderer->vegetation_interaction_enabled = true;
  return true;
}

bool ldk_renderer_submit_light(LDKRenderer *renderer,
    const LDKRendererLightSubmit *light)
{
  if (!renderer || !renderer->is_initialized || !light ||
      light->view_id == LDK_RENDERER_VIEW_INVALID ||
      light->type < LDK_RENDERER_LIGHT_POINT ||
      light->type > LDK_RENDERER_LIGHT_DIRECTIONAL ||
      !isfinite(light->intensity) || light->intensity < 0.0f)
  {
    return false;
  }
  LDKRendererLightSubmit submit = *light;
  if (light->type != LDK_RENDERER_LIGHT_DIRECTIONAL &&
      (!isfinite(light->range) || light->range <= 0.0f ||
          !isfinite(light->position.x) || !isfinite(light->position.y) ||
          !isfinite(light->position.z)))
  {
    return false;
  }
  if (light->type != LDK_RENDERER_LIGHT_POINT)
  {
    float length_squared = vec3_dot(light->direction, light->direction);
    if (!isfinite(length_squared) || length_squared < 1e-12f)
    {
      return false;
    }
    submit.direction = vec3_mul(light->direction,
        1.0f / sqrtf(length_squared));
  }
  if (light->type == LDK_RENDERER_LIGHT_SPOT &&
      (!isfinite(light->inner_angle) || !isfinite(light->outer_angle) ||
          light->inner_angle < 0.0f ||
          light->outer_angle <= 0.0f ||
          light->outer_angle > STDXM_PI * 0.5f ||
          light->inner_angle > light->outer_angle))
  {
    return false;
  }
  if (renderer->submitted_light_count == renderer->submitted_light_capacity)
  {
    u32 capacity = renderer->submitted_light_capacity == 0
        ? 16 : renderer->submitted_light_capacity * 2;
    if (capacity <= renderer->submitted_light_capacity)
    {
      return false;
    }
    LDKRendererLightSubmit *lights = LDK_RENDERER_REALLOC(
        renderer->submitted_lights, (size_t)capacity * sizeof(*lights));
    if (!lights)
    {
      return false;
    }
    renderer->submitted_lights = lights;
    renderer->submitted_light_capacity = capacity;
  }
  renderer->submitted_lights[renderer->submitted_light_count++] = submit;
  return true;
}

static void s_renderer_lighting_select(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKRendererViewLighting *lighting)
{
  LDKRendererLightingParams params = {0};
  memset(lighting, 0, sizeof(*lighting));
  params.count[1] = -1;
  LDKRHIColor sky =
      ldk_renderer_color_from_rgba32(renderer->ambient_light.sky_color);
  LDKRHIColor ground =
      ldk_renderer_color_from_rgba32(renderer->ambient_light.ground_color);
  params.ambient_sky[0] = sky.r;
  params.ambient_sky[1] = sky.g;
  params.ambient_sky[2] = sky.b;
  params.ambient_sky[3] = renderer->ambient_light.intensity;
  params.ambient_ground[0] = ground.r;
  params.ambient_ground[1] = ground.g;
  params.ambient_ground[2] = ground.b;
  params.ambient_ground[3] = renderer->ambient_light.intensity;
  LDKRHIColor fog_color = ldk_renderer_color_from_rgba32(renderer->fog.color);
  params.fog_color[0] = fog_color.r;
  params.fog_color[1] = fog_color.g;
  params.fog_color[2] = fog_color.b;
  params.fog_color[3] = 1.0f;
  params.fog_params[0] = renderer->fog.start;
  params.fog_params[1] = renderer->fog.enabled ? renderer->fog.density : 0.0f;

  for (u32 i = 0; i < renderer->submitted_light_count; i++)
  {
    const LDKRendererLightSubmit *light = &renderer->submitted_lights[i];
    if (light->view_id != LDK_RENDERER_VIEW_ALL && light->view_id != view_id)
    {
      continue;
    }
    if (light->intensity == 0.0f)
    {
      continue;
    }
    if (params.count[0] == LDK_RENDERER_MAX_LIGHTS_PER_VIEW)
    {
      if (!renderer->light_limit_reported)
      {
        ldk_log_warning("Renderer light limit exceeded: first %u lights per view are used.",
            (u32)LDK_RENDERER_MAX_LIGHTS_PER_VIEW);
        renderer->light_limit_reported = true;
      }
      break;
    }
    if (!lighting->shadow_light && light->casts_shadows &&
        light->type == LDK_RENDERER_LIGHT_DIRECTIONAL)
    {
      lighting->shadow_light = light;
      params.count[1] = params.count[0];
    }
    LDKRendererLightParams *out = &params.lights[params.count[0]++];
    out->position_type[0] = light->position.x;
    out->position_type[1] = light->position.y;
    out->position_type[2] = light->position.z;
    out->position_type[3] = (float)light->type;
    out->direction_range[0] = light->direction.x;
    out->direction_range[1] = light->direction.y;
    out->direction_range[2] = light->direction.z;
    out->direction_range[3] = light->range;
    LDKRHIColor color = ldk_renderer_color_from_rgba32(light->color);
    out->color_intensity[0] = color.r;
    out->color_intensity[1] = color.g;
    out->color_intensity[2] = color.b;
    out->color_intensity[3] = light->intensity;
    out->cone[0] = cosf(light->inner_angle);
    out->cone[1] = cosf(light->outer_angle);
  }
  lighting->params = params;
}

#include "ldk_renderer_pass_shadow.inl"
#include "ldk_renderer_pass_mesh.inl"
#include "ldk_renderer_pass_skybox.inl"
#include "ldk_renderer_pass_grid.inl"
#include "ldk_renderer_pass_water.inl"
static bool s_renderer_view_requires_hdr(LDKRendererView const* view)
{
  return view != NULL && view->post_processing.enabled &&
      (view->post_processing.tonemapping_enabled ||
          (view->post_processing.bloom_enabled &&
              isfinite(view->post_processing.bloom_intensity) &&
              view->post_processing.bloom_intensity > 0.0f));
}

static bool s_renderer_view_pass(LDKRenderer* renderer,
    LDKRendererView* view, LDKRendererFrameDesc const* frame_desc)
{
  if (renderer == NULL || view == NULL || !view->submitted ||
      frame_desc == NULL || view->width == 0 || view->height == 0)
  {
    return false;
  }

  LDKRHIFormat scene_color_format = s_renderer_view_requires_hdr(view)
      ? LDK_RHI_FORMAT_RGBA16_FLOAT
      : LDK_RHI_FORMAT_RGBA8_UNORM;

  if (view->target.color_texture != LDK_RHI_INVALID_RESOURCE &&
      view->target.color_format != scene_color_format)
  {
    s_renderer_post_process_bindings_destroy(renderer, view);
    s_renderer_post_process_blur_bindings_destroy(renderer, view);
  }

  if (!s_renderer_target_ensure(renderer, &view->target,
      (i32)view->width, (i32)view->height, scene_color_format))
  {
    return false;
  }

  LDKRendererViewLighting lighting;
  s_renderer_lighting_select(renderer, view->id, &lighting);
  s_renderer_shadow_pass_draw(renderer, view, &lighting);
  ldk_rhi_buffer_update(renderer->rhi, renderer->mesh_pass.lighting_buffer, 0,
      sizeof(lighting.params), &lighting.params);

  LDKRHIPassDesc pass_desc;
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = view->target.color_texture;
  pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.color_attachments[0].clear_color =
      ldk_renderer_color_from_rgba32(
          view->clear_color_set ? view->clear_color : frame_desc->clear_color);
  if (lighting.params.fog_params[1] > 0.0f &&
      !s_renderer_skybox_get_resource(renderer, view->skybox))
  {
    pass_desc.color_attachments[0].clear_color =
        ldk_renderer_color_from_rgba32(renderer->fog.color);
    pass_desc.color_attachments[0].clear_color.a = 1.0f;
  }
  pass_desc.depth_attachment.valid = true;
  pass_desc.depth_attachment.texture = view->target.depth_texture;
  pass_desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.depth_attachment.depth_store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.depth_attachment.clear_depth = 1.0f;
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)view->width;
  pass_desc.viewport.height = (float)view->height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  s_renderer_skybox_pass_draw(renderer, &renderer->skybox_pass, view);
  s_renderer_mesh_pass_draw(renderer, &renderer->mesh_pass, view,
      LDK_RENDERER_MESH_SUBMIT_FLAG_NONE);
  s_renderer_grid_pass_draw(renderer, &renderer->grid_pass, view);
  if (renderer->submitted_water_count)
  {
    ldk_rhi_pass_end(renderer->rhi);
    if (!s_renderer_water_pass_draw(renderer, view, &lighting))
    {
      ldk_log_error("Unable to render water for the current view.\n");
    }
    pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_LOAD;
    pass_desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_LOAD;
    ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  }
  s_renderer_mesh_pass_draw_translucent(
      renderer, &renderer->mesh_pass, view);
  s_renderer_text_pass_draw(renderer, &renderer->text_pass, view);
  s_renderer_wireframes_draw(renderer, &renderer->mesh_pass, view);
  // The remaining mesh submissions are editor/game overlays, not atmosphere.
  if (lighting.params.fog_params[1] > 0.0f)
  {
    lighting.params.fog_params[1] = 0.0f;
    ldk_rhi_buffer_update(renderer->rhi, renderer->mesh_pass.lighting_buffer, 0,
        sizeof(lighting.params), &lighting.params);
  }
  if (view->separate_overlay)
  {
    ldk_rhi_pass_end(renderer->rhi);
    pass_desc.color_attachments[0].texture = view->overlay_target.color_texture;
    pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
    pass_desc.color_attachments[0].clear_color =
        ldk_renderer_color_from_rgba32(0x00000000u);
    pass_desc.depth_attachment.texture = view->overlay_target.depth_texture;
    pass_desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
    ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  }
  s_renderer_mesh_pass_draw(renderer, &renderer->mesh_pass, view,
      LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY);
  s_renderer_mesh_pass_draw(renderer, &renderer->mesh_pass, view,
      LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY |
          LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE);
  ldk_rhi_pass_end(renderer->rhi);
  if (view->separate_overlay &&
      !s_renderer_overlay_display_draw(renderer, view))
  {
    return false;
  }
  return true;
}
#include "ldk_renderer_pass_text.inl"
#include "ldk_renderer_pass_ui.inl"
#include "ldk_renderer_pass_postprocess.inl"
// ---------------------------------------------------------------------------
// Internal renderer resources: Skybox
// ---------------------------------------------------------------------------

static LDKRendererSkyboxResource *s_renderer_skybox_get_resource(
    LDKRenderer *renderer, LDKResourceSkybox skybox)
{
  if (!renderer || skybox.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)(skybox.id - 1u);
  if (index >= renderer->skybox_count)
  {
    return NULL;
  }

  LDKRendererSkyboxResource *resource = &renderer->skyboxes[index];
  return resource->alive ? resource : NULL;
}

static bool s_renderer_grow_skybox_cache(LDKRenderer *renderer)
{
  u32 new_capacity = renderer->skybox_capacity == 0
      ? 16u : renderer->skybox_capacity * 2u;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererSkyboxResource);
  LDKRendererSkyboxResource *new_skyboxes = renderer->skyboxes == NULL
      ? (LDKRendererSkyboxResource *)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererSkyboxResource *)LDK_RENDERER_REALLOC(
            renderer->skyboxes, new_size);
  if (!new_skyboxes)
  {
    return false;
  }

  memset(new_skyboxes + renderer->skybox_capacity, 0,
      (size_t)(new_capacity - renderer->skybox_capacity) *
          sizeof(LDKRendererSkyboxResource));
  renderer->skyboxes = new_skyboxes;
  renderer->skybox_capacity = new_capacity;
  return true;
}

static bool s_renderer_skybox_faces_get(LDKAssetManager *assets,
    const LDKSkyboxDesc *desc, LDKImageInfo out_faces[LDK_SKYBOX_FACE_COUNT])
{
  if (!assets || !desc || !ldk_skybox_desc_is_valid(desc))
  {
    return false;
  }

  u32 size = 0;
  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    const LDKAssetImageData *data =
        ldk_asset_manager_image_get_const(assets, desc->faces[i]);
    if (!data || data->is_missing || !data->image ||
        !ldk_image_get_info(data->image, &out_faces[i]) ||
        out_faces[i].width == 0 ||
        out_faces[i].width != out_faces[i].height ||
        out_faces[i].channel_count != 4 ||
        out_faces[i].byte_count > UINT32_MAX)
    {
      return false;
    }

    if (i == 0)
    {
      size = out_faces[i].width;
    }
    else if (out_faces[i].width != size)
    {
      return false;
    }
  }

  return true;
}

LDKResourceSkybox ldk_renderer_skybox_null(void)
{
  return LDK_RESOURCE_SKYBOX_INVALID;
}

bool ldk_renderer_skybox_is_valid(
    LDKRenderer *renderer, LDKResourceSkybox skybox)
{
  return s_renderer_skybox_get_resource(renderer, skybox) != NULL;
}

LDKResourceSkybox ldk_renderer_skybox_create(LDKRenderer *renderer,
    LDKAssetManager *assets, const LDKSkyboxDesc *desc)
{
  LDKResourceSkybox invalid = ldk_renderer_skybox_null();
  LDKImageInfo faces[LDK_SKYBOX_FACE_COUNT] = {0};

  if (!renderer || !renderer->is_initialized ||
      !s_renderer_skybox_faces_get(assets, desc, faces))
  {
    return invalid;
  }

  if (renderer->skybox_count == renderer->skybox_capacity &&
      !s_renderer_grow_skybox_cache(renderer))
  {
    return invalid;
  }

  LDKRendererSkyboxResource resource = {0};
  LDKRHITextureDesc texture_desc = {0};
  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_CUBE;
  texture_desc.format = LDK_RHI_FORMAT_RGBA8_SRGB;
  texture_desc.width = faces[0].width;
  texture_desc.height = faces[0].height;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = LDK_SKYBOX_FACE_COUNT;
  texture_desc.usage =
      LDK_RHI_TEXTURE_USAGE_SAMPLED | LDK_RHI_TEXTURE_USAGE_TRANSFER_DST;
  resource.texture = ldk_rhi_texture_create(renderer->rhi, &texture_desc);
  if (resource.texture == LDK_RHI_INVALID_RESOURCE)
  {
    return invalid;
  }

  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    if (!ldk_rhi_texture_update(renderer->rhi, resource.texture, 0, i,
            faces[i].pixels, (u32)faces[i].byte_count))
    {
      ldk_rhi_texture_destroy(renderer->rhi, resource.texture);
      return invalid;
    }
  }

  LDKRHISamplerDesc sampler_desc = {0};
  sampler_desc.min_filter = LDK_RHI_FILTER_LINEAR;
  sampler_desc.mag_filter = LDK_RHI_FILTER_LINEAR;
  sampler_desc.mip_filter = LDK_RHI_FILTER_LINEAR;
  sampler_desc.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler_desc.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler_desc.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  resource.sampler = ldk_rhi_sampler_create(renderer->rhi, &sampler_desc);
  if (resource.sampler == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, resource.texture);
    return invalid;
  }

  LDKRHIBindingsDesc bindings_desc = {0};
  ldk_rhi_bindings_desc_defaults(&bindings_desc);
  bindings_desc.layout = renderer->skybox_pass.bindings_layout;
  bindings_desc.binding_count = 2;
  bindings_desc.bindings[0].slot = 0;
  bindings_desc.bindings[0].buffer = renderer->skybox_pass.params_buffer;
  bindings_desc.bindings[0].buffer_size = sizeof(LDKRendererSkyboxParams);
  bindings_desc.bindings[1].slot = 1;
  bindings_desc.bindings[1].texture = resource.texture;
  bindings_desc.bindings[1].sampler = resource.sampler;
  resource.bindings = ldk_rhi_bindings_create(renderer->rhi, &bindings_desc);
  if (resource.bindings == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_sampler_destroy(renderer->rhi, resource.sampler);
    ldk_rhi_texture_destroy(renderer->rhi, resource.texture);
    return invalid;
  }

  resource.size = faces[0].width;
  resource.alive = true;
  u32 index = renderer->skybox_count++;
  renderer->skyboxes[index] = resource;

  LDKResourceSkybox skybox = {(LDKRHIResource)(index + 1u)};
  return skybox;
}

void ldk_renderer_skybox_destroy(
    LDKRenderer *renderer, LDKResourceSkybox skybox)
{
  LDKRendererSkyboxResource *resource =
      s_renderer_skybox_get_resource(renderer, skybox);
  if (!resource)
  {
    return;
  }

  ldk_rhi_bindings_destroy(renderer->rhi, resource->bindings);
  ldk_rhi_sampler_destroy(renderer->rhi, resource->sampler);
  ldk_rhi_texture_destroy(renderer->rhi, resource->texture);
  memset(resource, 0, sizeof(*resource));
}

static void s_renderer_destroy_skybox_resources(LDKRenderer *renderer)
{
  if (!renderer)
  {
    return;
  }

  for (u32 i = 0; i < renderer->skybox_count; ++i)
  {
    LDKRendererSkyboxResource *resource = &renderer->skyboxes[i];
    if (!resource->alive)
    {
      continue;
    }
    ldk_rhi_bindings_destroy(renderer->rhi, resource->bindings);
    ldk_rhi_sampler_destroy(renderer->rhi, resource->sampler);
    ldk_rhi_texture_destroy(renderer->rhi, resource->texture);
  }

  LDK_RENDERER_FREE(renderer->skyboxes);
  renderer->skyboxes = NULL;
  renderer->skybox_count = 0;
  renderer->skybox_capacity = 0;
}

// Internal renderer resources: Texture
// ---------------------------------------------------------------------------

static LDKRendererTextureResource* s_renderer_texture_get_resource(
    LDKRenderer* renderer, LDKResourceTexture texture)
{
  if (renderer == NULL || texture.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)(texture.id - 1u);
  if (index >= renderer->texture_count)
  {
    return NULL;
  }

  LDKRendererTextureResource* resource = &renderer->textures[index];
  if (!resource->alive)
  {
    return NULL;
  }

  return resource;
}

static bool s_renderer_grow_texture_cache(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->texture_capacity == 0
      ? 64 : renderer->texture_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererTextureResource);
  LDKRendererTextureResource* new_textures = renderer->textures == NULL
      ? (LDKRendererTextureResource*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererTextureResource*)LDK_RENDERER_REALLOC(
            renderer->textures, new_size);

  if (new_textures == NULL)
  {
    return false;
  }

  memset(new_textures + renderer->texture_capacity, 0,
      (size_t)(new_capacity - renderer->texture_capacity) *
          sizeof(LDKRendererTextureResource));

  renderer->textures = new_textures;
  renderer->texture_capacity = new_capacity;
  return true;
}

void ldk_renderer_texture_options_defaults(LDKRendererTextureOptions* options)
{
  if (options == NULL)
  {
    return;
  }

  memset(options, 0, sizeof(*options));
  options->min_filter = LDK_RHI_FILTER_NEAREST;
  options->mag_filter = LDK_RHI_FILTER_NEAREST;
  options->mip_filter = LDK_RHI_FILTER_NEAREST;
  options->wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  options->wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  options->wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
}

static u32 s_renderer_texture_mip_count(u32 width, u32 height)
{
  u32 size = width > height ? width : height;
  u32 mip_count = 1;

  while (size > 1)
  {
    size >>= 1;
    mip_count += 1;
  }

  return mip_count;
}

static LDKRendererTextureOptions s_renderer_texture_options(
    LDKRendererTextureDesc const* desc)
{
  LDKRendererTextureOptions options = {0};
  ldk_renderer_texture_options_defaults(&options);
  options.flags = desc->flags;

  if (desc->options != NULL)
  {
    options = *desc->options;
    options.flags |= desc->flags;
  }

  return options;
}

static LDKRHIFormat s_renderer_texture_format_from_desc(
    LDKRendererTextureDesc const* desc)
{
  if (desc == NULL)
  {
    return LDK_RHI_FORMAT_INVALID;
  }

  if (desc->format != LDK_RHI_FORMAT_INVALID)
  {
    return desc->format;
  }

  if (desc->channel_count == 1)
  {
    return LDK_RHI_FORMAT_R8_UNORM;
  }

  if (desc->channel_count == 2)
  {
    return LDK_RHI_FORMAT_RG8_UNORM;
  }

  if (desc->channel_count == 4)
  {
    LDKRendererTextureOptions options = s_renderer_texture_options(desc);
    if ((options.flags & LDK_RENDERER_TEXTURE_FLAG_SRGB) != 0)
    {
      return LDK_RHI_FORMAT_RGBA8_SRGB;
    }

    return LDK_RHI_FORMAT_RGBA8_UNORM;
  }

  return LDK_RHI_FORMAT_INVALID;
}

static bool s_renderer_texture_desc_is_valid(
    LDKRendererTextureDesc const* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->width == 0 || desc->height == 0)
  {
    return false;
  }

  if (desc->pixels == NULL && desc->byte_count != 0)
  {
    return false;
  }

  if (desc->byte_count > UINT32_MAX)
  {
    return false;
  }

  LDKRendererTextureOptions options = s_renderer_texture_options(desc);
  LDKRHISamplerDesc sampler_desc = {0};
  sampler_desc.min_filter = options.min_filter;
  sampler_desc.mag_filter = options.mag_filter;
  sampler_desc.mip_filter = options.mip_filter;
  sampler_desc.wrap_u = options.wrap_u;
  sampler_desc.wrap_v = options.wrap_v;
  sampler_desc.wrap_w = options.wrap_w;

  if (!ldk_rhi_is_valid_sampler_desc(&sampler_desc))
  {
    return false;
  }

  return s_renderer_texture_format_from_desc(desc) != LDK_RHI_FORMAT_INVALID;
}

static bool s_renderer_texture_resource_create_rhi_texture(
    LDKRenderer* renderer, LDKRendererTextureResource* resource,
    LDKRendererTextureDesc const* desc)
{
  LDKRHIFormat format = s_renderer_texture_format_from_desc(desc);
  if (format == LDK_RHI_FORMAT_INVALID)
  {
    return false;
  }

  LDKRendererTextureOptions options = s_renderer_texture_options(desc);

  LDKRHITextureDesc rhi_desc = {0};
  ldk_rhi_texture_desc_defaults(&rhi_desc);
  rhi_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  rhi_desc.format = format;
  rhi_desc.width = desc->width;
  rhi_desc.height = desc->height;
  rhi_desc.depth = 1;
  rhi_desc.mip_count = options.generate_mipmaps
      ? s_renderer_texture_mip_count(desc->width, desc->height) : 1;
  rhi_desc.layer_count = 1;
  rhi_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  rhi_desc.initial_data = desc->pixels;
  rhi_desc.initial_data_size = (u32)desc->byte_count;

  if ((options.flags & LDK_RENDERER_TEXTURE_FLAG_RENDERABLE) != 0)
  {
    rhi_desc.usage |= LDK_RHI_TEXTURE_USAGE_RENDER_TARGET;
  }

  resource->texture = ldk_rhi_texture_create(renderer->rhi, &rhi_desc);
  if (resource->texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHISamplerDesc sampler_desc = {0};
  sampler_desc.min_filter = options.min_filter;
  sampler_desc.mag_filter = options.mag_filter;
  sampler_desc.mip_filter = options.mip_filter;
  sampler_desc.wrap_u = options.wrap_u;
  sampler_desc.wrap_v = options.wrap_v;
  sampler_desc.wrap_w = options.wrap_w;

  resource->sampler = ldk_rhi_sampler_create(renderer->rhi, &sampler_desc);
  if (resource->sampler == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, resource->texture);
    resource->texture = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  resource->width = desc->width;
  resource->height = desc->height;
  resource->channel_count = desc->channel_count;
  resource->format = format;
  resource->flags = options.flags;
  return true;
}

LDKResourceTexture ldk_renderer_texture_null(void)
{
  return LDK_RESOURCE_TEXTURE_INVALID;
}

bool ldk_renderer_texture_is_valid(
    LDKRenderer* renderer, LDKResourceTexture texture)
{
  return s_renderer_texture_get_resource(renderer, texture) != NULL;
}

LDKUITextureHandle ldk_renderer_texture_ui_handle(
    LDKRenderer* renderer, LDKResourceTexture texture)
{
  LDKRendererTextureResource* resource =
      s_renderer_texture_get_resource(renderer, texture);
  if (resource == NULL)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  return (LDKUITextureHandle)resource->texture;
}

static LDKRHISampler s_renderer_texture_sampler_from_rhi_texture(
    LDKRenderer* renderer, LDKRHITexture texture)
{
  if (renderer == NULL || texture == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  for (u32 i = 0; i < renderer->texture_count; i++)
  {
    LDKRendererTextureResource* resource = &renderer->textures[i];
    if (resource->alive && resource->texture == texture)
    {
      return resource->sampler;
    }
  }

  return LDK_RHI_INVALID_RESOURCE;
}

LDKResourceTexture ldk_renderer_texture_create(
    LDKRenderer* renderer, LDKRendererTextureDesc const* desc)
{
  LDKResourceTexture invalid = ldk_renderer_texture_null();

  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_texture_desc_is_valid(desc))
  {
    return invalid;
  }

  if (renderer->texture_count == renderer->texture_capacity)
  {
    if (!s_renderer_grow_texture_cache(renderer))
    {
      return invalid;
    }
  }

  u32 index = renderer->texture_count;
  LDKRendererTextureResource* resource = &renderer->textures[index];
  memset(resource, 0, sizeof(*resource));

  if (!s_renderer_texture_resource_create_rhi_texture(
          renderer, resource, desc))
  {
    memset(resource, 0, sizeof(*resource));
    return invalid;
  }

  resource->alive = true;
  renderer->texture_count += 1;

  LDKResourceTexture texture = {0};
  texture.id = (LDKRHITexture)(index + 1u);
  return texture;
}

bool ldk_renderer_texture_update(LDKRenderer* renderer,
    LDKResourceTexture texture, void const* pixels, u64 byte_count)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return false;
  }

  if (pixels == NULL || byte_count == 0 || byte_count > UINT32_MAX)
  {
    return false;
  }

  LDKRendererTextureResource* resource =
      s_renderer_texture_get_resource(renderer, texture);
  if (resource == NULL)
  {
    return false;
  }

  return ldk_rhi_texture_update(renderer->rhi, resource->texture, 0, 0,
      pixels, (u32)byte_count);
}

LDKResourceTexture ldk_renderer_texture_create_from_image(
    LDKRenderer* renderer, LDKImage const* image,
    LDKRendererTextureOptions const* options)
{
  LDKImageInfo info = {0};

  if (!ldk_image_get_info(image, &info))
  {
    return ldk_renderer_texture_null();
  }

  LDKRendererTextureDesc desc = {0};
  desc.width = info.width;
  desc.height = info.height;
  desc.channel_count = info.channel_count;
  desc.format = info.format == LDK_IMAGE_FORMAT_RGBA16_UNORM
      ? LDK_RHI_FORMAT_RGBA16_UNORM
      : LDK_RHI_FORMAT_INVALID;
  desc.pixels = info.pixels;
  desc.byte_count = info.byte_count;
  desc.options = options;

  return ldk_renderer_texture_create(renderer, &desc);
}

void ldk_renderer_texture_destroy(
    LDKRenderer* renderer, LDKResourceTexture texture)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return;
  }

  LDKRendererTextureResource* resource =
      s_renderer_texture_get_resource(renderer, texture);
  if (resource == NULL)
  {
    return;
  }

  if (resource->image_references != 0)
  {
    return;
  }

  if (renderer->vegetation_interaction_texture.id == texture.id)
  {
    ldk_renderer_vegetation_interaction_clear(renderer);
  }

  s_renderer_mesh_pass_remove_texture_bindings(
      &renderer->mesh_pass, resource->texture);
  s_renderer_shadow_pass_remove_texture_bindings(
      &renderer->shadow_pass, resource->texture);
  s_renderer_ui_pass_remove_texture_bindings(
      &renderer->ui_pass, resource->texture);
  ldk_rhi_sampler_destroy(renderer->rhi, resource->sampler);
  ldk_rhi_texture_destroy(renderer->rhi, resource->texture);
  memset(resource, 0, sizeof(*resource));
}

// ---------------------------------------------------------------------------
// Material resources
// ---------------------------------------------------------------------------

static LDKResourceTexture s_renderer_image_acquire(LDKRenderer* renderer,
    LDKAssetManager* assets, LDKAssetImage image, bool srgb)
{
  if (!renderer || !renderer->is_initialized || !assets)
  {
    return ldk_renderer_texture_null();
  }
  const LDKAssetImageData* data =
      ldk_asset_manager_image_get_const(assets, image);
  if (!data || !data->image)
  {
    return ldk_renderer_texture_null();
  }
  for (u32 i = 0; i < renderer->texture_count; i++)
  {
    LDKRendererTextureResource* entry = &renderer->textures[i];
    if (entry->alive && entry->image_references != 0 &&
        entry->asset_manager == assets &&
        entry->image_asset.h.index == image.h.index &&
        entry->image_asset.h.version == image.h.version &&
        ((entry->flags & LDK_RENDERER_TEXTURE_FLAG_SRGB) != 0) == srgb)
    {
      if (entry->image_references == UINT32_MAX)
      {
        return ldk_renderer_texture_null();
      }
      entry->image_references++;
      LDKResourceTexture result = {i + 1u};
      return result;
    }
  }
  LDKRendererTextureOptions options = {0};
  ldk_renderer_texture_options_defaults(&options);
  options.generate_mipmaps = true;
  options.mip_filter = LDK_RHI_FILTER_NONE;
  if (srgb)
  {
    options.flags |= LDK_RENDERER_TEXTURE_FLAG_SRGB;
  }
  LDKResourceTexture result =
      ldk_renderer_texture_create_from_image(renderer, data->image, &options);
  LDKRendererTextureResource* entry =
      s_renderer_texture_get_resource(renderer, result);
  if (entry)
  {
    entry->asset_manager = assets;
    entry->image_asset = image;
    entry->image_references = 1;
  }
  return result;
}

LDKResourceTexture ldk_renderer_image_acquire(LDKRenderer* renderer,
    LDKAssetManager* assets, LDKAssetImage image)
{
  /* Generic assets default to linear data. Color slots opt into sRGB. */
  return s_renderer_image_acquire(renderer, assets, image, false);
}

void ldk_renderer_image_release(
    LDKRenderer* renderer, LDKResourceTexture texture)
{
  LDKRendererTextureResource* entry =
      s_renderer_texture_get_resource(renderer, texture);
  if (!entry || entry->image_references == 0)
  {
    return;
  }
  entry->image_references--;
  if (entry->image_references == 0)
  {
    ldk_renderer_texture_destroy(renderer, texture);
  }
}

static LDKResourceTexture s_renderer_material_map_acquire(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKAssetImage image, const char *label)
{
  if (x_handle_is_null(image.h))
  {
    return ldk_renderer_texture_null();
  }

  const LDKAssetImageData *data =
      ldk_asset_manager_image_get_const(assets, image);
  if (!data || !data->image || data->is_missing)
  {
    ldk_log_error("Material %s unavailable; using neutral fallback.\n", label);
    return ldk_renderer_texture_null();
  }

  LDKResourceTexture texture =
      s_renderer_image_acquire(renderer, assets, image, false);
  if (!ldk_renderer_texture_is_valid(renderer, texture))
  {
    ldk_log_error(
        "Material %s upload failed; using neutral fallback.\n", label);
    return ldk_renderer_texture_null();
  }
  return texture;
}

static LDKAssetImage s_renderer_material_source_image(
    LDKAssetManager *assets, const LDKMaterialDesc *material, u32 slot)
{
  LDKAssetTextureAtlas atlas = material->texture_atlases[slot];
  if (assets && !x_handle_is_null(atlas.h))
  {
    const LDKAssetTextureAtlasData *data =
        ldk_asset_manager_texture_atlas_get_const(assets, atlas);
    return data ? data->image : ldk_asset_image_null();
  }
  return ldk_material_texture_slot_get(material, slot);
}

static bool s_renderer_material_atlas_rect(LDKAssetManager *assets,
    LDKAssetTextureAtlas atlas, u64 region_id, float uv[4])
{
  uv[0] = 0.0f;
  uv[1] = 0.0f;
  uv[2] = 1.0f;
  uv[3] = 1.0f;
  if (!region_id || x_handle_is_null(atlas.h))
    return true;
  const LDKAssetTextureAtlasData *data =
      ldk_asset_manager_texture_atlas_get_const(assets, atlas);
  const LDKTextureAtlasRegion *region = data
      ? ldk_texture_atlas_region_find_const(&data->atlas, region_id) : NULL;
  const LDKAssetImageData *image = data
      ? ldk_asset_manager_image_get_const(assets, data->image) : NULL;
  if (!region || !image || !image->image)
    return false;
  return ldk_texture_atlas_region_uv(region,
      ldk_image_get_width(image->image), ldk_image_get_height(image->image), uv);
}

/* The albedo region is evaluated per submission, not part of GPU
 * material state. Two authored materials that differ only in their default
 * frame may therefore still share the same renderer material and batch. */
bool ldk_renderer_material_atlases_current(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKResourceMaterial material)
{
  LDKRendererMaterialResource *resource =
      s_renderer_material_get_resource(renderer, material);
  if (!resource || !assets)
    return false;
  if (!resource->has_source_desc)
    return true;
  for (u32 i = 0; i < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
  {
    LDKAssetTextureAtlas atlas = resource->source_desc.texture_atlases[i];
    if (x_handle_is_null(atlas.h))
      continue;
    const LDKAssetTextureAtlasData *data =
        ldk_asset_manager_texture_atlas_get_const(assets, atlas);
    if (!data || data->revision != resource->source_atlas_revisions[i])
      return false;
  }
  return true;
}

static bool s_renderer_material_gpu_source_equal(
    const LDKMaterialDesc *a, const LDKMaterialDesc *b)
{
  LDKMaterialDesc x = *a;
  LDKMaterialDesc y = *b;
  x.texture_regions[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO] = 0u;
  y.texture_regions[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO] = 0u;
  return ldk_material_desc_equal(&x, &y);
}

static bool s_renderer_material_resolve_internal(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
    LDKResourceMaterial *renderer_material,
    LDKResourceTexture *renderer_texture,
    LDKResourceTexture *renderer_normal_map,
    LDKResourceTexture *renderer_specular_map, bool allow_intern)
{
  LDKRendererMaterialDesc desc = {0};
  LDKResourceMaterial material;
  LDKResourceTexture normal_map = ldk_renderer_texture_null();
  LDKResourceTexture specular_map = ldk_renderer_texture_null();
  bool lit;

  if (!renderer || !material_desc || !renderer_material || !renderer_texture ||
      !renderer_normal_map || !renderer_specular_map ||
      !ldk_material_desc_is_valid(material_desc))
  {
    return false;
  }

  desc.type = material_desc->type;
  desc.texture = ldk_renderer_texture_null();
  desc.normal_map = ldk_renderer_texture_null();
  desc.specular_map = ldk_renderer_texture_null();
  for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
  {
    desc.additional_textures[i] = ldk_renderer_texture_null();
  }
  desc.alpha_mode = material_desc->alpha_mode;
  desc.alpha_cutoff = material_desc->alpha_cutoff;
  lit = material_desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      material_desc->type == LDK_MATERIAL_TYPE_VERTEX_COLOR;

  switch (material_desc->type)
  {
  case LDK_MATERIAL_TYPE_VERTEX_COLOR:
  case LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT:
    desc.color = material_desc->args.vertex_color.color;
    break;
  case LDK_MATERIAL_TYPE_TEXTURED:
  case LDK_MATERIAL_TYPE_TEXTURED_UNLIT:
    if (!assets)
    {
      return false;
    }
    desc.color = material_desc->args.textured.color;
    desc.texture = s_renderer_image_acquire(
        renderer, assets, s_renderer_material_source_image(
            assets, material_desc, LDK_MATERIAL_TEXTURE_SLOT_ALBEDO), true);
    if (!ldk_renderer_texture_is_valid(renderer, desc.texture))
    {
      ldk_log_error("Material image unavailable; using missing texture.\n");
      LDKAssetImage fallback = ldk_asset_manager_image_missing(assets, NULL);
      desc.texture = s_renderer_image_acquire(renderer, assets, fallback, true);
      if (!ldk_renderer_texture_is_valid(renderer, desc.texture))
      {
        return false;
      }
    }
    break;
  case LDK_MATERIAL_TYPE_INVALID:
  default:
    return false;
  }

  if (material_desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      material_desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT)
  {
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      LDKAssetImage image = s_renderer_material_source_image(
          assets, material_desc, i + 3u);
      if (x_handle_is_null(image.h))
      {
        continue;
      }
      desc.additional_textures[i] =
          ldk_renderer_image_acquire(renderer, assets, image);
      if (!ldk_renderer_texture_is_valid(
              renderer, desc.additional_textures[i]))
      {
        ldk_log_error(
            "Material texture slot %u unavailable; using missing texture.\n",
            i + 3u);
        LDKAssetImage fallback = ldk_asset_manager_image_missing(assets, NULL);
        desc.additional_textures[i] =
            ldk_renderer_image_acquire(renderer, assets, fallback);
        if (!ldk_renderer_texture_is_valid(
                renderer, desc.additional_textures[i]))
        {
          for (u32 j = 0; j < i; ++j)
          {
            ldk_renderer_image_release(renderer, desc.additional_textures[j]);
          }
          ldk_renderer_image_release(renderer, desc.texture);
          return false;
        }
      }
    }
  }

  if (lit)
  {
    if (!assets &&
        (!x_handle_is_null(material_desc->surface.normal_map.h) ||
            !x_handle_is_null(material_desc->surface.specular_map.h)))
    {
      ldk_renderer_image_release(renderer, desc.texture);
      return false;
    }
    if (assets)
    {
      normal_map = s_renderer_material_map_acquire(
          renderer, assets, s_renderer_material_source_image(
              assets, material_desc, LDK_MATERIAL_TEXTURE_SLOT_NORMAL),
          "normal map");
      specular_map = s_renderer_material_map_acquire(renderer, assets, s_renderer_material_source_image(
              assets, material_desc, LDK_MATERIAL_TEXTURE_SLOT_SPECULAR),
          "specular map");
    }
    desc.normal_map = normal_map;
    desc.specular_map = specular_map;
    desc.specular = material_desc->surface.specular;
    desc.shininess = material_desc->surface.shininess;
    desc.emission = material_desc->surface.emission;
  }

  /* Reuse a material with identical authored parameters and resolved GPU
   * textures. The caller still owns one acquisition of albedo/normal/specular;
   * optional slots 3..7 are owned once by the shared material. */
  if (allow_intern)
  for (u32 i = 0; i < renderer->material_count; ++i)
  {
    LDKRendererMaterialResource *existing = &renderer->materials[i];
    if (!existing->alive || !existing->has_source_desc ||
        existing->source_assets != assets ||
        !ldk_renderer_material_atlases_current(renderer, assets,
            (LDKResourceMaterial){i + 1u}) ||
        !s_renderer_material_gpu_source_equal(
            &existing->source_desc, material_desc) ||
        existing->desc.texture.id != desc.texture.id ||
        existing->desc.normal_map.id != desc.normal_map.id ||
        existing->desc.specular_map.id != desc.specular_map.id)
      continue;
    bool same = true;
    for (u32 j = 0; j < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++j)
    {
      if (existing->desc.additional_textures[j].id !=
          desc.additional_textures[j].id)
      {
        same = false;
        break;
      }
    }
    if (!same || existing->ref_count == UINT32_MAX)
      continue;
    for (u32 j = 0; j < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++j)
      ldk_renderer_image_release(renderer, desc.additional_textures[j]);
    ++existing->ref_count;
    material.id = (LDKRHIResource)(i + 1u);
    goto material_resolved;
  }

  material = ldk_renderer_material_create(renderer, &desc);
  if (!ldk_renderer_material_is_valid(renderer, material))
  {
    ldk_renderer_image_release(renderer, desc.texture);
    ldk_renderer_image_release(renderer, normal_map);
    ldk_renderer_image_release(renderer, specular_map);
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      ldk_renderer_image_release(renderer, desc.additional_textures[i]);
    }
    return false;
  }

  {
    LDKRendererMaterialResource *created =
        s_renderer_material_get_resource(renderer, material);
    created->source_assets = assets;
    created->source_desc = *material_desc;
    created->has_source_desc = allow_intern;
    for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
    {
      LDKAssetTextureAtlas atlas = material_desc->texture_atlases[slot];
      if (x_handle_is_null(atlas.h))
        continue;
      const LDKAssetTextureAtlasData *data =
          ldk_asset_manager_texture_atlas_get_const(assets, atlas);
      created->source_atlas_revisions[slot] = data ? data->revision : 0u;
    }
  }

  if (material_desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      material_desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      material_desc->type == LDK_MATERIAL_TYPE_VERTEX_COLOR)
  {
    LDKRendererMaterialResource *material_resource =
        s_renderer_material_get_resource(renderer, material);
    if (material_resource != NULL)
    {
      material_resource->owns_additional_textures =
          material_desc->type != LDK_MATERIAL_TYPE_VERTEX_COLOR;
    }
    for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
    {
      LDKMaterialTextureSettings settings =
          ldk_material_texture_settings_get(material_desc, slot);
      LDKRHISamplerDesc sampler_desc;
      ldk_rhi_sampler_desc_defaults(&sampler_desc);
      sampler_desc.min_filter =
          s_renderer_material_filter_to_rhi(settings.filter);
      sampler_desc.mag_filter = sampler_desc.min_filter;
      sampler_desc.mip_filter =
          s_renderer_material_mip_filter_to_rhi(settings.mip_filter);
      sampler_desc.wrap_u = s_renderer_material_wrap_to_rhi(settings.wrap_u);
      sampler_desc.wrap_v = s_renderer_material_wrap_to_rhi(settings.wrap_v);
      sampler_desc.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
      bool custom_sampling =
          settings.filter != LDK_MATERIAL_TEXTURE_FILTER_NEAREST ||
          settings.mip_filter != LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE ||
          settings.wrap_u != LDK_MATERIAL_TEXTURE_WRAP_CLAMP ||
          settings.wrap_v != LDK_MATERIAL_TEXTURE_WRAP_CLAMP;
      LDKRHISampler sampler = custom_sampling
          ? s_renderer_material_sampler_get_or_create(renderer, &sampler_desc)
          : LDK_RHI_INVALID_RESOURCE;
      if (material_resource == NULL ||
          (custom_sampling && sampler == LDK_RHI_INVALID_RESOURCE))
      {
        ldk_renderer_material_destroy(renderer, material);
        ldk_renderer_image_release(renderer, desc.texture);
        ldk_renderer_image_release(renderer, normal_map);
        ldk_renderer_image_release(renderer, specular_map);
        return false;
      }
      material_resource->texture_samplers[slot] = sampler;
      material_resource->texture_uv_scales[slot] = settings.uv_scale;
      material_resource->texture_uv_offsets[slot] = settings.uv_offset;
      /* Albedo atlas rectangles are supplied per draw/instance instead. */
      if (slot != LDK_MATERIAL_TEXTURE_SLOT_ALBEDO &&
          !x_handle_is_null(material_desc->texture_atlases[slot].h))
      {
        float uv[4];
        if (s_renderer_material_atlas_rect(assets,
                material_desc->texture_atlases[slot],
                material_desc->texture_regions[slot], uv))
        {
          float sx = uv[2] - uv[0];
          float sy = uv[3] - uv[1];
          material_resource->texture_uv_scales[slot].x *= sx;
          material_resource->texture_uv_scales[slot].y *= sy;
          material_resource->texture_uv_offsets[slot].x =
              uv[0] + settings.uv_offset.x * sx;
          material_resource->texture_uv_offsets[slot].y =
              uv[1] + settings.uv_offset.y * sy;
        }
      }
    }
    /* The shadow pass samples only albedo. */
    material_resource->texture_sampler = material_resource->texture_samplers[0];
    material_resource->uv_scale = material_resource->texture_uv_scales[0];
    material_resource->uv_offset = material_resource->texture_uv_offsets[0];
  }

material_resolved:
  ldk_renderer_material_destroy(renderer, *renderer_material);
  ldk_renderer_image_release(renderer, *renderer_texture);
  ldk_renderer_image_release(renderer, *renderer_normal_map);
  ldk_renderer_image_release(renderer, *renderer_specular_map);
  *renderer_texture = desc.texture;
  *renderer_normal_map = normal_map;
  *renderer_specular_map = specular_map;
  *renderer_material = material;
  return true;
}

bool ldk_renderer_material_resolve(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
    LDKResourceMaterial *renderer_material,
    LDKResourceTexture *renderer_texture,
    LDKResourceTexture *renderer_normal_map,
    LDKResourceTexture *renderer_specular_map)
{
  return s_renderer_material_resolve_internal(renderer, assets, material_desc,
      renderer_material, renderer_texture, renderer_normal_map,
      renderer_specular_map, true);
}

bool ldk_renderer_terrain_material_resolve(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
    const LDKRendererTerrainSurfaceDesc *surfaces, u32 surface_count,
    LDKResourceMaterial *renderer_material,
    LDKResourceTexture *renderer_texture,
    LDKResourceTexture *renderer_normal_map,
    LDKResourceTexture *renderer_specular_map)
{
  if (!surfaces || surface_count == 0u ||
      surface_count > LDK_RENDERER_TERRAIN_SURFACE_COUNT)
  {
    return false;
  }

  /* Terrain carries mutable, generated surface state and must not mutate an
   * interned material shared with ordinary MeshSources. */
  if (!s_renderer_material_resolve_internal(renderer, assets, material_desc,
          renderer_material, renderer_texture, renderer_normal_map,
          renderer_specular_map, false))
  {
    return false;
  }

  LDKRendererMaterialResource *resource =
      s_renderer_material_get_resource(renderer, *renderer_material);
  if (!resource)
  {
    return false;
  }

  resource->desc.terrain = true;
  resource->desc.terrain_surface_count = surface_count;
  memcpy(resource->desc.terrain_surfaces, surfaces,
      (size_t)surface_count * sizeof(*surfaces));
  resource->selection = LDK_RENDERER_MATERIAL_SELECTION_TERRAIN;
  resource->render_key = (LDKRendererRenderKey)resource->selection;
  return true;
}

LDKResourceMaterial ldk_renderer_material_null(void)
{
  return LDK_RESOURCE_MATERIAL_INVALID;
}

static LDKRendererMaterialResource* s_renderer_material_get_resource(
    LDKRenderer* renderer, LDKResourceMaterial material)
{
  if (renderer == NULL || material.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)(material.id - 1u);
  if (index >= renderer->material_count)
  {
    return NULL;
  }

  LDKRendererMaterialResource* resource = &renderer->materials[index];
  if (!resource->alive)
  {
    return NULL;
  }

  return resource;
}

bool ldk_renderer_material_is_valid(
    LDKRenderer* renderer, LDKResourceMaterial material)
{
  return s_renderer_material_get_resource(renderer, material) != NULL;
}

static bool s_renderer_grow_material_cache(LDKRenderer* renderer)
{
  u32 new_capacity =
      renderer->material_capacity == 0 ? 64 : renderer->material_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMaterialResource);
  LDKRendererMaterialResource* new_materials = renderer->materials == NULL
      ? (LDKRendererMaterialResource*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererMaterialResource*)LDK_RENDERER_REALLOC(
            renderer->materials, new_size);

  if (new_materials == NULL)
  {
    return false;
  }

  memset(new_materials + renderer->material_capacity, 0,
      (size_t)(new_capacity - renderer->material_capacity) *
          sizeof(LDKRendererMaterialResource));

  renderer->materials = new_materials;
  renderer->material_capacity = new_capacity;
  return true;
}

static LDKRHIFilter s_renderer_material_filter_to_rhi(
    LDKMaterialTextureFilter filter)
{
  return filter == LDK_MATERIAL_TEXTURE_FILTER_LINEAR
      ? LDK_RHI_FILTER_LINEAR
      : LDK_RHI_FILTER_NEAREST;
}

static LDKRHIFilter s_renderer_material_mip_filter_to_rhi(
    LDKMaterialTextureMipFilter filter)
{
  switch (filter)
  {
  case LDK_MATERIAL_TEXTURE_MIP_FILTER_NEAREST:
    return LDK_RHI_FILTER_NEAREST;
  case LDK_MATERIAL_TEXTURE_MIP_FILTER_LINEAR:
    return LDK_RHI_FILTER_LINEAR;
  case LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE:
  default:
    return LDK_RHI_FILTER_NONE;
  }
}

static LDKRHIWrap s_renderer_material_wrap_to_rhi(LDKMaterialTextureWrap wrap)
{
  switch (wrap)
  {
  case LDK_MATERIAL_TEXTURE_WRAP_REPEAT:
    return LDK_RHI_WRAP_REPEAT;
  case LDK_MATERIAL_TEXTURE_WRAP_MIRROR:
    return LDK_RHI_WRAP_MIRROR;
  case LDK_MATERIAL_TEXTURE_WRAP_CLAMP:
  default:
    return LDK_RHI_WRAP_CLAMP_TO_EDGE;
  }
}

static bool s_renderer_material_sampler_desc_equal(
    const LDKRHISamplerDesc *a, const LDKRHISamplerDesc *b)
{
  return a->min_filter == b->min_filter &&
         a->mag_filter == b->mag_filter &&
         a->mip_filter == b->mip_filter && a->wrap_u == b->wrap_u &&
         a->wrap_v == b->wrap_v && a->wrap_w == b->wrap_w;
}

static bool s_renderer_grow_material_sampler_cache(LDKRenderer *renderer)
{
  u32 new_capacity = renderer->material_sampler_capacity == 0
      ? 8
      : renderer->material_sampler_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMaterialSamplerResource);
  LDKRendererMaterialSamplerResource *new_samplers =
      renderer->material_samplers == NULL
      ? (LDKRendererMaterialSamplerResource *)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererMaterialSamplerResource *)LDK_RENDERER_REALLOC(
            renderer->material_samplers, new_size);

  if (new_samplers == NULL)
  {
    return false;
  }

  renderer->material_samplers = new_samplers;
  renderer->material_sampler_capacity = new_capacity;
  return true;
}

static LDKRHISampler s_renderer_material_sampler_get_or_create(
    LDKRenderer *renderer, const LDKRHISamplerDesc *desc)
{
  if (renderer == NULL || renderer->rhi == NULL || desc == NULL ||
      !ldk_rhi_is_valid_sampler_desc(desc))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  for (u32 i = 0; i < renderer->material_sampler_count; ++i)
  {
    LDKRendererMaterialSamplerResource *resource =
        &renderer->material_samplers[i];
    if (s_renderer_material_sampler_desc_equal(&resource->desc, desc))
    {
      return resource->sampler;
    }
  }

  if (renderer->material_sampler_count == renderer->material_sampler_capacity &&
      !s_renderer_grow_material_sampler_cache(renderer))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHISampler sampler = ldk_rhi_sampler_create(renderer->rhi, desc);
  if (sampler == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererMaterialSamplerResource *resource =
      &renderer->material_samplers[renderer->material_sampler_count++];
  resource->desc = *desc;
  resource->sampler = sampler;
  return sampler;
}

static bool s_renderer_material_type_is_textured(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
         type == LDK_MATERIAL_TYPE_TEXTURED;
}

static bool s_renderer_material_type_is_lit(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED ||
         type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
}

static LDKRendererMaterialSelection s_renderer_material_selection(
    const LDKRendererMaterialDesc *desc)
{
  if (desc != NULL && desc->terrain)
  {
    return LDK_RENDERER_MATERIAL_SELECTION_TERRAIN;
  }

  switch (desc->type)
  {
    case LDK_MATERIAL_TYPE_TEXTURED_UNLIT:
      if (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT)
      {
        return LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT;
      }
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_BLEND
          ? LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND
          : LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT;
    case LDK_MATERIAL_TYPE_TEXTURED:
      if (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT)
      {
        return LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT;
      }
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_BLEND
          ? LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND
          : LDK_RENDERER_MATERIAL_SELECTION_TEXTURED;
    case LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT:
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_BLEND
          ? LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT_BLEND
          : LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT;
    case LDK_MATERIAL_TYPE_VERTEX_COLOR:
      if (desc->vegetation)
      {
        return LDK_RENDERER_MATERIAL_SELECTION_VEGETATION;
      }
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_BLEND
          ? LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_BLEND
          : LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR;
    case LDK_MATERIAL_TYPE_INVALID:
    default:
      return LDK_RENDERER_MATERIAL_SELECTION_INVALID;
  }
}

static LDKRendererRenderKey s_renderer_material_render_key(
    LDKRendererMaterialSelection selection)
{
  return (LDKRendererRenderKey)selection;
}

static bool s_renderer_material_desc_is_valid(
    LDKRenderer* renderer, LDKRendererMaterialDesc const* desc)
{
  if (desc && desc->terrain)
  {
    if (desc->terrain_surface_count == 0u ||
        desc->terrain_surface_count > LDK_RENDERER_TERRAIN_SURFACE_COUNT)
    {
      return false;
    }
    float previous = -INFINITY;
    for (u32 i = 0u; i < desc->terrain_surface_count; ++i)
    {
      const LDKRendererTerrainSurfaceDesc *surface =
          &desc->terrain_surfaces[i];
      if (!isfinite(surface->min_height) ||
          !isfinite(surface->blend_width) || surface->blend_width < 0.0f ||
          !isfinite(surface->texture_weight) ||
          surface->texture_weight < 0.0f || surface->texture_weight > 1.0f ||
          surface->min_height < previous ||
          !isfinite(surface->atlas_rect.x) ||
          !isfinite(surface->atlas_rect.y) ||
          !isfinite(surface->atlas_rect.z) ||
          !isfinite(surface->atlas_rect.w) ||
          !isfinite(surface->uv_scale.x) ||
          !isfinite(surface->uv_scale.y))
      {
        return false;
      }
      previous = surface->min_height;
    }
  }
  if (desc == NULL || !ldk_material_type_is_valid(desc->type))
  {
    return false;
  }
  if (desc->vegetation &&
      (desc->type != LDK_MATERIAL_TYPE_VERTEX_COLOR ||
          desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_OPAQUE))
  {
    return false;
  }

  bool textured = s_renderer_material_type_is_textured(desc->type);
  if ((desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_OPAQUE &&
          desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_CUTOUT &&
          desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_BLEND) ||
      (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT && !textured) ||
      (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT &&
          (!isfinite(desc->alpha_cutoff) || desc->alpha_cutoff < 0.0f ||
              desc->alpha_cutoff > 1.0f)))
  {
    return false;
  }

  if (textured && !ldk_renderer_texture_is_valid(renderer, desc->texture))
  {
    return false;
  }
  if (textured)
  {
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      if (desc->additional_textures[i].id != LDK_RHI_INVALID_RESOURCE &&
          !ldk_renderer_texture_is_valid(
              renderer, desc->additional_textures[i]))
      {
        return false;
      }
    }
  }

  if (!s_renderer_material_type_is_lit(desc->type))
  {
    return isfinite(desc->vegetation_curvature) &&
           desc->vegetation_curvature >= 0.0f &&
           isfinite(desc->vegetation_wind_strength) &&
           desc->vegetation_wind_strength >= 0.0f &&
           isfinite(desc->vegetation_wind_speed) &&
           desc->vegetation_wind_speed >= 0.0f &&
           isfinite(desc->vegetation_wind_direction) &&
           isfinite(desc->vegetation_interaction_recovery_time) &&
           desc->vegetation_interaction_recovery_time >= 0.0f;
  }

  if ((desc->normal_map.id != LDK_RHI_INVALID_RESOURCE &&
          !ldk_renderer_texture_is_valid(renderer, desc->normal_map)) ||
      (desc->specular_map.id != LDK_RHI_INVALID_RESOURCE &&
          !ldk_renderer_texture_is_valid(renderer, desc->specular_map)))
  {
    return false;
  }

  return isfinite(desc->specular) && desc->specular >= 0.0f &&
         isfinite(desc->shininess) && desc->shininess >= 0.0f &&
         isfinite(desc->emission) && desc->emission >= 0.0f &&
         isfinite(desc->vegetation_curvature) &&
         desc->vegetation_curvature >= 0.0f &&
         isfinite(desc->vegetation_wind_strength) &&
         desc->vegetation_wind_strength >= 0.0f &&
         isfinite(desc->vegetation_wind_speed) &&
         desc->vegetation_wind_speed >= 0.0f &&
         isfinite(desc->vegetation_wind_direction) &&
         isfinite(desc->vegetation_interaction_recovery_time) &&
         desc->vegetation_interaction_recovery_time >= 0.0f;
}

LDKResourceMaterial ldk_renderer_material_create(
    LDKRenderer* renderer, LDKRendererMaterialDesc const* desc)
{
  LDKResourceMaterial invalid = ldk_renderer_material_null();

  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_material_desc_is_valid(renderer, desc))
  {
    return invalid;
  }

  if (renderer->material_count == renderer->material_capacity &&
      !s_renderer_grow_material_cache(renderer))
  {
    return invalid;
  }

  bool lit = s_renderer_material_type_is_lit(desc->type);
  u32 index = renderer->material_count;
  LDKRendererMaterialResource* resource = &renderer->materials[index];
  memset(resource, 0, sizeof(*resource));
  resource->desc.type = desc->type;
  resource->desc.color = desc->color;
  resource->desc.texture = s_renderer_material_type_is_textured(desc->type)
      ? desc->texture
      : ldk_renderer_texture_null();
  resource->desc.alpha_mode = desc->alpha_mode;
  resource->desc.alpha_cutoff =
      desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT
      ? desc->alpha_cutoff
      : 0.5f;
  resource->desc.normal_map =
      lit ? desc->normal_map : ldk_renderer_texture_null();
  resource->desc.specular_map =
      lit ? desc->specular_map : ldk_renderer_texture_null();
  for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
  {
    resource->desc.additional_textures[i] =
        s_renderer_material_type_is_textured(desc->type)
        ? desc->additional_textures[i]
        : ldk_renderer_texture_null();
  }
  resource->desc.specular = lit ? desc->specular : 0.0f;
  resource->desc.shininess =
      lit && desc->shininess != 0.0f ? desc->shininess : 32.0f;
  resource->desc.emission = lit ? desc->emission : 0.0f;
  resource->desc.vegetation_curvature = desc->vegetation_curvature;
  resource->desc.vegetation_wind_strength = desc->vegetation_wind_strength;
  resource->desc.vegetation_wind_speed = desc->vegetation_wind_speed;
  resource->desc.vegetation_wind_direction = desc->vegetation_wind_direction;
  resource->desc.vegetation_interaction_recovery_time =
      desc->vegetation_interaction_recovery_time;
  resource->desc.vegetation = desc->vegetation;
  resource->desc.terrain = desc->terrain;
  resource->desc.terrain_surface_count = desc->terrain_surface_count;
  if (desc->terrain_surface_count <= LDK_RENDERER_TERRAIN_SURFACE_COUNT)
  {
    memcpy(resource->desc.terrain_surfaces, desc->terrain_surfaces,
        sizeof(resource->desc.terrain_surfaces));
  }
  resource->selection = s_renderer_material_selection(&resource->desc);
  resource->render_key = s_renderer_material_render_key(resource->selection);
  resource->texture_sampler = LDK_RHI_INVALID_RESOURCE;
  for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
  {
    resource->texture_samplers[slot] = LDK_RHI_INVALID_RESOURCE;
    resource->texture_uv_scales[slot] = (Vec2){1.0f, 1.0f};
    resource->texture_uv_offsets[slot] = (Vec2){0.0f, 0.0f};
  }
  resource->owns_additional_textures = false;
  resource->ref_count = 1;
  resource->uv_scale = (Vec2){1.0f, 1.0f};
  resource->uv_offset = (Vec2){0.0f, 0.0f};
  resource->alive = true;
  renderer->material_count += 1;

  LDKResourceMaterial material = {0};
  material.id = (LDKRHIResource)(index + 1u);
  return material;
}

void ldk_renderer_material_destroy(
    LDKRenderer* renderer, LDKResourceMaterial material)
{
  LDKRendererMaterialResource* resource =
      s_renderer_material_get_resource(renderer, material);
  if (resource == NULL)
  {
    return;
  }

  if (resource->ref_count > 1)
  {
    --resource->ref_count;
    return;
  }
  if (resource->owns_additional_textures)
  {
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      ldk_renderer_image_release(
          renderer, resource->desc.additional_textures[i]);
    }
  }
  memset(resource, 0, sizeof(*resource));
}

LDKResourceMaterial ldk_renderer_material_default_get(LDKRenderer* renderer)
{
  if (renderer == NULL || !renderer->is_initialized ||
      !ldk_renderer_material_is_valid(renderer, renderer->default_material))
  {
    return ldk_renderer_material_null();
  }

  return renderer->default_material;
}

static void s_renderer_destroy_material_resources(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  for (u32 i = 0; i < renderer->material_count; i++)
  {
    LDKResourceMaterial material = {0};
    material.id = (LDKRHIResource)(i + 1u);
    renderer->materials[i].ref_count = 1;
    ldk_renderer_material_destroy(renderer, material);
  }

  LDK_RENDERER_FREE(renderer->materials);
  renderer->materials = NULL;
  renderer->material_count = 0;
  renderer->material_capacity = 0;
}

static void s_renderer_destroy_material_samplers(LDKRenderer *renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->material_sampler_count; ++i)
    {
      LDKRHISampler sampler = renderer->material_samplers[i].sampler;
      if (sampler != LDK_RHI_INVALID_RESOURCE)
      {
        ldk_rhi_sampler_destroy(renderer->rhi, sampler);
      }
    }
  }

  LDK_RENDERER_FREE(renderer->material_samplers);
  renderer->material_samplers = NULL;
  renderer->material_sampler_count = 0;
  renderer->material_sampler_capacity = 0;
}

static void s_renderer_destroy_texture_resources(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->texture_count; i++)
    {
      LDKResourceTexture texture = {0};
      texture.id = (LDKRHITexture)(i + 1u);
      renderer->textures[i].image_references = 0;
      ldk_renderer_texture_destroy(renderer, texture);
    }
  }

  LDK_RENDERER_FREE(renderer->textures);
  renderer->textures = NULL;
  renderer->texture_count = 0;
  renderer->texture_capacity = 0;
}

// ---------------------------------------------------------------------------
// Font cache
// ---------------------------------------------------------------------------

static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->font_page_count; i++)
    {
      s_renderer_ui_pass_remove_texture_bindings(
          &renderer->ui_pass, renderer->font_pages[i].texture);
      s_renderer_text_pass_remove_texture_bindings(
          &renderer->text_pass, renderer->font_pages[i].texture);
      ldk_rhi_texture_destroy(renderer->rhi, renderer->font_pages[i].texture);
    }
  }

  LDK_RENDERER_FREE(renderer->font_pages);
  renderer->font_pages = NULL;
  renderer->font_page_count = 0;
  renderer->font_page_capacity = 0;
}

static bool s_renderer_grow_font_page_cache(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->font_page_capacity == 0
      ? 32 : renderer->font_page_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererFontPageCacheEntry);
  LDKRendererFontPageCacheEntry* new_pages = renderer->font_pages == NULL
      ? (LDKRendererFontPageCacheEntry*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererFontPageCacheEntry*)LDK_RENDERER_REALLOC(
            renderer->font_pages, new_size);

  if (new_pages == NULL)
  {
    return false;
  }

  renderer->font_pages = new_pages;
  renderer->font_page_capacity = new_capacity;
  return true;
}

static LDKRHITexture s_renderer_font_page_texture_create(
    LDKRenderer *renderer, const LDKFontPageInfo *page)
{
  LDKRHITextureDesc texture_desc = {0};

  if (renderer == NULL || page == NULL || page->pixels == NULL ||
      page->width == 0 || page->height == 0)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_R8_UNORM;
  texture_desc.width = page->width;
  texture_desc.height = page->height;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = page->pixels;
  texture_desc.initial_data_size = page->width * page->height;
  texture_desc.swizzle_r = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_g = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_b = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_a = LDK_RHI_TEXTURE_SWIZZLE_R;

  return ldk_rhi_texture_create(renderer->rhi, &texture_desc);
}

LDKUITextureHandle ldk_renderer_get_font_page_texture(
    LDKRenderer *renderer, LDKFontInstance *font, u32 page_index)
{
  LDKFontPageInfo page = {0};

  if (renderer == NULL || renderer->rhi == NULL || font == NULL)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  if (!ldk_ttf_get_page_info(font, page_index, &page) ||
      page.pixels == NULL || page.width == 0 || page.height == 0)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  for (u32 i = 0; i < renderer->font_page_count; i++)
  {
    LDKRendererFontPageCacheEntry *entry = &renderer->font_pages[i];
    if (entry->font != font || entry->page_index != page_index)
    {
      continue;
    }

    if (entry->width != page.width || entry->height != page.height)
    {
      LDKRHITexture texture =
          s_renderer_font_page_texture_create(renderer, &page);
      if (texture == LDK_RHI_INVALID_RESOURCE)
      {
        return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
      }

      s_renderer_ui_pass_remove_texture_bindings(
          &renderer->ui_pass, entry->texture);
      s_renderer_text_pass_remove_texture_bindings(
          &renderer->text_pass, entry->texture);
      ldk_rhi_texture_destroy(renderer->rhi, entry->texture);
      entry->texture = texture;
      entry->width = page.width;
      entry->height = page.height;
      ldk_ttf_clear_page_dirty(font, page_index);
    }
    else if (page.dirty)
    {
      if (!ldk_rhi_texture_update(renderer->rhi, entry->texture, 0, 0,
              page.pixels, page.width * page.height))
      {
        return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
      }

      ldk_ttf_clear_page_dirty(font, page_index);
    }

    return (LDKUITextureHandle)entry->texture;
  }

  LDKRHITexture texture = s_renderer_font_page_texture_create(renderer, &page);
  if (texture == LDK_RHI_INVALID_RESOURCE)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  if (renderer->font_page_count == renderer->font_page_capacity)
  {
    if (!s_renderer_grow_font_page_cache(renderer))
    {
      ldk_rhi_texture_destroy(renderer->rhi, texture);
      return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
    }
  }

  LDKRendererFontPageCacheEntry *entry =
      &renderer->font_pages[renderer->font_page_count];
  entry->font = font;
  entry->page_index = page_index;
  entry->width = page.width;
  entry->height = page.height;
  entry->texture = texture;
  renderer->font_page_count += 1;

  ldk_ttf_clear_page_dirty(font, page_index);
  return (LDKUITextureHandle)texture;
}

LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(
    void* user, LDKFontInstance* font, u32 page_index)
{
  return ldk_renderer_get_font_page_texture(
      (LDKRenderer*)user, font, page_index);
}

// ---------------------------------------------------------------------------
// Public renderer API
// ---------------------------------------------------------------------------

bool ldk_renderer_shadow_settings_set(
    LDKRenderer *renderer, u32 resolution, float distance)
{
  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_shadow_settings_valid(resolution, distance))
  {
    return false;
  }
  if (renderer->shadow_pass.resolution == resolution)
  {
    renderer->shadow_pass.distance = distance;
    return true;
  }

  LDKRendererShadowPass replacement;
  if (!s_renderer_shadow_pass_initialize(
          &replacement, renderer->rhi, resolution, distance))
  {
    return false;
  }
  // Build the new bindings before releasing any currently usable resources.
  LDKRendererMeshPass next_mesh = renderer->mesh_pass;
  next_mesh.shadow_texture = replacement.depth_texture;
  next_mesh.shadow_sampler = replacement.sampler;
  if (!s_renderer_mesh_pass_create_bindings(&next_mesh))
  {
    s_renderer_shadow_pass_terminate(&replacement);
    return false;
  }

  LDKRendererMeshPass *mesh = &renderer->mesh_pass;
  for (u32 i = 0; i < mesh->material_bindings_cache_count; ++i)
  {
    ldk_rhi_bindings_destroy(
        mesh->rhi, mesh->material_bindings_cache[i].bindings);
  }
  mesh->material_bindings_cache_count = 0;
  ldk_rhi_bindings_destroy(mesh->rhi, mesh->bindings);
  mesh->bindings = next_mesh.bindings;
  mesh->shadow_texture = next_mesh.shadow_texture;
  mesh->shadow_sampler = next_mesh.shadow_sampler;
  s_renderer_shadow_pass_terminate(&renderer->shadow_pass);
  renderer->shadow_pass = replacement;
  return true;
}

bool ldk_renderer_initialize(
    LDKRenderer* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL ||
      config->game_width == 0 || config->game_height == 0)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;
  renderer->game_width = config->game_width;
  renderer->game_height = config->game_height;
  renderer->present_game = config->present_game;
  renderer->ambient_light.sky_color = 0xc4dcffffu;
  renderer->ambient_light.ground_color = 0x786c60ffu;
  renderer->ambient_light.intensity = 0.3f;
  renderer->fog.enabled = false;
  renderer->fog.color = 0xb0c4d8ffu;
  renderer->fog.density = 0.01f;
  renderer->fog.start = 10.0f;
  renderer->animation_start_ticks = ldk_os_time_ticks_get();

  if (!s_renderer_shadow_pass_initialize(&renderer->shadow_pass, config->rhi,
          config->shadow_map_resolution ? config->shadow_map_resolution : 2048u,
          config->shadow_distance != 0.0f ? config->shadow_distance : 60.0f))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_mesh_pass_initialize(
          &renderer->mesh_pass, config, &renderer->shadow_pass))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_grid_pass_initialize(&renderer->grid_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_skybox_pass_initialize(&renderer->skybox_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_blur_pass_initialize(&renderer->blur_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_post_process_pass_initialize(
          &renderer->post_process_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_text_pass_initialize(&renderer->text_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_initialize(&renderer->ui_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;

  LDKRendererMaterialDesc default_material_desc = {0};
  default_material_desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  default_material_desc.texture = ldk_renderer_texture_null();
  default_material_desc.normal_map = ldk_renderer_texture_null();
  default_material_desc.specular_map = ldk_renderer_texture_null();
  default_material_desc.color = 0xffffffffu;
  default_material_desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  default_material_desc.alpha_cutoff = 0.5f;
  default_material_desc.specular = 0.0f;
  default_material_desc.shininess = 32.0f;
  default_material_desc.emission = 0.0f;
  renderer->default_material =
      ldk_renderer_material_create(renderer, &default_material_desc);
  if (!ldk_renderer_material_is_valid(
          renderer, renderer->default_material))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  return true;
}

bool ldk_renderer_game_resolution_set(
    LDKRenderer* renderer, u32 width, u32 height)
{
  if (renderer == NULL || !renderer->is_initialized ||
      width == 0 || height == 0)
  {
    return false;
  }

  if (renderer->game_width == width && renderer->game_height == height)
  {
    return true;
  }

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    LDKRendererView* view = &renderer->views[i];
    if (view->extent_override)
    {
      continue;
    }

    s_renderer_post_process_view_destroy(renderer, view);
    s_renderer_target_destroy(renderer, &view->target);
    s_renderer_target_destroy(renderer, &view->overlay_target);
    view->width = width;
    view->height = height;
  }

  renderer->game_width = width;
  renderer->game_height = height;
  return true;
}

void ldk_renderer_terminate(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  s_renderer_destroy_views(renderer);
  s_renderer_destroy_font_page_cache(renderer);
  LDK_RENDERER_FREE(renderer->submitted_texts);
  LDK_RENDERER_FREE(renderer->submitted_text_indices);
  LDK_RENDERER_FREE(renderer->submitted_text_vertices);
  s_renderer_destroy_material_resources(renderer);
  s_renderer_destroy_skybox_resources(renderer);
  s_renderer_destroy_texture_resources(renderer);
  s_renderer_destroy_instance_set_resources(renderer);
  s_renderer_destroy_mesh_resources(renderer);
  LDK_RENDERER_FREE(renderer->submitted_water);
  s_renderer_water_pass_terminate(&renderer->water_pass);
  s_renderer_post_process_pass_terminate(&renderer->post_process_pass);
  s_renderer_blur_pass_terminate(&renderer->blur_pass);
  s_renderer_ui_pass_terminate(&renderer->ui_pass);
  s_renderer_text_pass_terminate(&renderer->text_pass);
  s_renderer_skybox_pass_terminate(&renderer->skybox_pass);
  s_renderer_grid_pass_terminate(&renderer->grid_pass);
  s_renderer_mesh_pass_terminate(&renderer->mesh_pass);
  s_renderer_shadow_pass_terminate(&renderer->shadow_pass);
  s_renderer_destroy_material_samplers(renderer);
  memset(renderer, 0, sizeof(*renderer));
}

void ldk_renderer_render_frame(
    LDKRenderer* renderer, LDKRendererFrameDesc const* desc)
{
  if (renderer == NULL || !renderer->is_initialized || desc == NULL ||
      renderer->rhi == NULL)
  {
    return;
  }

  u64 frame_start_ticks = ldk_os_time_ticks_get();
  renderer->animation_time_seconds = (float)
      ldk_os_time_ticks_interval_get_seconds(
          renderer->animation_start_ticks, frame_start_ticks);

  memset(&renderer->current_frame_stats, 0,
      sizeof(renderer->current_frame_stats));
  renderer->current_frame_stats.mesh_submit_count =
      renderer->submitted_mesh_count + renderer->submitted_water_count;

  ldk_rhi_frame_begin(renderer->rhi);

  LDKRendererView* game_view =
      s_renderer_view_find(renderer, renderer->game_view);
  bool rendered_game = false;

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    LDKRendererView* view = &renderer->views[i];
    bool rendered = s_renderer_view_pass(renderer, view, desc);
    if (rendered && !s_renderer_post_process_draw(renderer, view))
    {
      view->post_processing.enabled = false;
    }
    if (rendered)
    {
      LDKRendererFrameDomainStats* stats =
          s_renderer_frame_stats_for_view(renderer, view);
      stats->rendered_view_count += 1;
    }

    if (view == game_view)
    {
      rendered_game = rendered;
    }
  }
  bool presented_game = false;
  if (rendered_game && renderer->present_game)
  {
    s_renderer_present_view_pass(renderer, game_view, desc);
    presented_game = true;
  }

  if (renderer->submitted_ui != NULL)
  {
    LDKRendererFrameDesc ui_desc = *desc;
    if (presented_game)
    {
      ui_desc.clear_color_enabled = false;
    }

    LDKRendererFrameDomainStats* ui_stats = renderer->present_game
        ? &renderer->current_frame_stats.game
        : &renderer->current_frame_stats.non_game;
    s_renderer_ui_pass(renderer, &renderer->ui_pass, ui_stats,
        renderer->submitted_ui, &ui_desc, false);
  }

  ldk_rhi_frame_end(renderer->rhi);
  renderer->current_frame_stats.rhi =
      ldk_rhi_last_frame_stats_get(renderer->rhi);

  renderer->submitted_mesh_count = 0;
  renderer->submitted_instance_count = 0;
  renderer->submitted_water_count = 0;
  renderer->submitted_line_count = 0;
  renderer->submitted_wireframe_count = 0;
  renderer->submitted_text_vertex_count = 0;
  renderer->submitted_text_index_count = 0;
  renderer->submitted_text_count = 0;
  renderer->submitted_light_count = 0;
  renderer->submitted_ui = NULL;
  s_renderer_finish_views(renderer);

  u64 frame_end_ticks = ldk_os_time_ticks_get();
  renderer->current_frame_stats.cpu_time_ms =
      ldk_os_time_ticks_interval_get_milliseconds(
          frame_start_ticks, frame_end_ticks);
  s_renderer_frame_stats_finalize(&renderer->current_frame_stats);
  renderer->last_frame_stats = renderer->current_frame_stats;
}

LDKRendererFrameStats ldk_renderer_last_frame_stats_get(
    LDKRenderer const* renderer)
{
  if (renderer == NULL)
  {
    return (LDKRendererFrameStats){0};
  }

  return renderer->last_frame_stats;
}

LDKUITextureHandle ldk_renderer_game_texture_get(
    LDKRenderer const* renderer)
{
  if (renderer == NULL)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  return ldk_renderer_view_texture_get(renderer, renderer->game_view);
}

LDKUITextureHandle ldk_renderer_view_texture_get(
    LDKRenderer const* renderer, LDKRendererViewId view_id)
{
  LDKRendererView const* view = s_renderer_view_find_const(renderer, view_id);

  if (view == NULL || !view->submitted)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHITexture texture = s_renderer_view_output_texture(view);
  if (texture == LDK_RHI_INVALID_RESOURCE)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  return (LDKUITextureHandle)texture;
}

LDKUIRect ldk_renderer_view_texture_uv_get(LDKRenderer const* renderer)
{
  (void)renderer;

  return (LDKUIRect){0.0f, 1.0f, 1.0f, -1.0f};
}

LDKUITextureHandle ldk_renderer_view_overlay_texture_request(
    LDKRenderer* renderer, LDKRendererViewId view_id)
{
  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (!view || !view->submitted || view->width == 0 || view->height == 0 ||
      !s_renderer_target_ensure(renderer, &view->overlay_target,
          (i32)view->width, (i32)view->height,
          LDK_RHI_FORMAT_RGBA8_UNORM))
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  if (!s_renderer_overlay_display_ensure(renderer, view))
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }
  view->separate_overlay = true;
  return (LDKUITextureHandle)view->overlay_display_texture;
}

static bool s_renderer_view_frustum_update(LDKRendererView *view)
{
  Mat4 matrix;
  Vec4 planes[6];

  if (view == NULL)
  {
    return false;
  }

  matrix = mat4_mul(view->projection, view->view);
  planes[0] = vec4_make(matrix.m[3] + matrix.m[0],
      matrix.m[7] + matrix.m[4], matrix.m[11] + matrix.m[8],
      matrix.m[15] + matrix.m[12]);
  planes[1] = vec4_make(matrix.m[3] - matrix.m[0],
      matrix.m[7] - matrix.m[4], matrix.m[11] - matrix.m[8],
      matrix.m[15] - matrix.m[12]);
  planes[2] = vec4_make(matrix.m[3] + matrix.m[1],
      matrix.m[7] + matrix.m[5], matrix.m[11] + matrix.m[9],
      matrix.m[15] + matrix.m[13]);
  planes[3] = vec4_make(matrix.m[3] - matrix.m[1],
      matrix.m[7] - matrix.m[5], matrix.m[11] - matrix.m[9],
      matrix.m[15] - matrix.m[13]);
  planes[4] = vec4_make(matrix.m[3] + matrix.m[2],
      matrix.m[7] + matrix.m[6], matrix.m[11] + matrix.m[10],
      matrix.m[15] + matrix.m[14]);
  planes[5] = vec4_make(matrix.m[3] - matrix.m[2],
      matrix.m[7] - matrix.m[6], matrix.m[11] - matrix.m[10],
      matrix.m[15] - matrix.m[14]);

  for (u32 i = 0u; i < 6u; ++i)
  {
    float length = sqrtf(planes[i].x * planes[i].x +
        planes[i].y * planes[i].y + planes[i].z * planes[i].z);

    if (!isfinite(length) || length <= STDXM_EPS ||
        !isfinite(planes[i].w))
    {
      return false;
    }

    view->frustum_planes[i] = vec4_make(planes[i].x / length,
        planes[i].y / length, planes[i].z / length,
        planes[i].w / length);
  }
  return true;
}

static bool s_renderer_view_bounds_visible(
    const LDKRendererView *view, Vec3 center, Vec3 extents)
{
  if (view == NULL || !view->frustum_valid)
  {
    return true;
  }

  for (u32 i = 0u; i < 6u; ++i)
  {
    const Vec4 plane = view->frustum_planes[i];
    float distance = plane.x * center.x + plane.y * center.y +
        plane.z * center.z + plane.w;
    float radius = fabsf(plane.x) * extents.x +
        fabsf(plane.y) * extents.y + fabsf(plane.z) * extents.z;

    if (distance + radius < 0.0f)
    {
      return false;
    }
  }
  return true;
}

bool ldk_renderer_view_bounds_visible(const LDKRenderer *renderer,
    LDKRendererViewId view_id, Vec3 bounds_min, Vec3 bounds_max)
{
  Vec3 center;
  Vec3 extents;

  if (renderer == NULL ||
      !isfinite(bounds_min.x) || !isfinite(bounds_min.y) ||
      !isfinite(bounds_min.z) || !isfinite(bounds_max.x) ||
      !isfinite(bounds_max.y) || !isfinite(bounds_max.z) ||
      bounds_min.x > bounds_max.x || bounds_min.y > bounds_max.y ||
      bounds_min.z > bounds_max.z)
  {
    return true;
  }

  center = vec3_mul(vec3_add(bounds_min, bounds_max), 0.5f);
  extents = vec3_mul(vec3_sub(bounds_max, bounds_min), 0.5f);

  if (view_id == LDK_RENDERER_VIEW_ALL)
  {
    bool found = false;

    for (u32 i = 0u; i < renderer->view_count; ++i)
    {
      const LDKRendererView *view = &renderer->views[i];

      if (!view->submitted)
      {
        continue;
      }
      found = true;
      if (s_renderer_view_bounds_visible(view, center, extents))
      {
        return true;
      }
    }
    return !found;
  }

  for (u32 i = 0u; i < renderer->view_count; ++i)
  {
    const LDKRendererView *view = &renderer->views[i];

    if (view->submitted && view->id == view_id)
    {
      return s_renderer_view_bounds_visible(view, center, extents);
    }
  }
  return true;
}

bool ldk_renderer_view_extent_set(LDKRenderer* renderer,
    LDKRendererViewId view_id, u32 width, u32 height)
{
  if (renderer == NULL || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL || width == 0 || height == 0)
  {
    return false;
  }

  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (view == NULL)
  {
    return false;
  }

  view->requested_width = width;
  view->requested_height = height;
  view->extent_override = true;
  return true;
}

bool ldk_renderer_view_extent_get(LDKRenderer const* renderer,
    LDKRendererViewId view_id, u32* out_width, u32* out_height)
{
  if (renderer == NULL || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL ||
      out_width == NULL || out_height == NULL ||
      renderer->game_width == 0 || renderer->game_height == 0)
  {
    return false;
  }

  LDKRendererView const* view = s_renderer_view_find_const(renderer, view_id);
  if (view != NULL && view->extent_override &&
      view->requested_width > 0 && view->requested_height > 0)
  {
    *out_width = view->requested_width;
    *out_height = view->requested_height;
  }
  else
  {
    *out_width = renderer->game_width;
    *out_height = renderer->game_height;
  }

  return true;
}

bool ldk_renderer_submit_view(LDKRenderer* renderer,
    LDKRendererViewId view_id, Mat4 view_matrix, Mat4 projection)
{
  if (renderer == NULL || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return false;
  }

  LDKRendererView* view = s_renderer_view_find(renderer, view_id);

  if (view == NULL)
  {
    if (renderer->view_count == renderer->view_capacity &&
        !s_renderer_grow_view_cache(renderer))
    {
      return false;
    }

    view = &renderer->views[renderer->view_count++];
    memset(view, 0, sizeof(*view));
    view->id = view_id;
  }

  u32 width = renderer->game_width;
  u32 height = renderer->game_height;
  if (view->extent_override &&
      view->requested_width > 0 && view->requested_height > 0)
  {
    width = view->requested_width;
    height = view->requested_height;
  }

  if (!s_renderer_view_extent_apply(renderer, view, width, height))
  {
    return false;
  }

  /* Keep editor view handles bound to the encoded display target. */
  if (renderer->post_process_pass.is_initialized &&
      !s_renderer_post_process_texture_ensure(renderer, view))
  {
    return false;
  }

  view->view = view_matrix;
  view->projection = projection;
  view->post_processing = (LDKRendererPostProcessing){0};
  view->clear_color = 0;
  view->clear_color_set = false;
  view->frustum_valid = s_renderer_view_frustum_update(view);
  view->submitted = true;
  return true;
}

bool ldk_renderer_view_clear_color_set(LDKRenderer* renderer,
    LDKRendererViewId view_id, rgba32 clear_color)
{
  if (renderer == NULL || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return false;
  }

  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (view == NULL || !view->submitted)
  {
    return false;
  }

  view->clear_color = clear_color;
  view->clear_color_set = true;
  return true;
}

bool ldk_renderer_game_view_set(
    LDKRenderer* renderer, LDKRendererViewId view_id)
{
  LDKRendererView* view = s_renderer_view_find(renderer, view_id);

  if (renderer == NULL || view == NULL || !view->submitted)
  {
    return false;
  }

  renderer->game_view = view_id;
  return true;
}

void ldk_renderer_submit_ui(
    LDKRenderer* renderer, LDKUIRenderData const* render_data)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return;
  }

  renderer->submitted_ui = render_data;
}

static bool s_renderer_submit_mesh(LDKRenderer* renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index, u32 index_count,
    Mat4 world, u32 flags)
{
  if (renderer == NULL || !renderer->is_initialized ||
      (flags & ~(LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY |
                   LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS |
                   LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD |
                   LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE)) != 0 ||
      ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE) != 0 &&
       (flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) == 0))
  {
    return false;
  }

  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL || index_count == 0 ||
      first_index > resource->index_count ||
      index_count > resource->index_count - first_index)
  {
    return false;
  }

  LDKRendererMaterialResource* material_resource =
      s_renderer_material_get_resource(renderer, material);
  if (material_resource == NULL)
  {
    return false;
  }

  if (renderer->submitted_mesh_count == renderer->submitted_mesh_capacity)
  {
    if (!s_renderer_grow_mesh_submit_queue(renderer))
    {
      return false;
    }
  }

  LDKRendererMeshSubmit* submit =
      &renderer->submitted_meshes[renderer->submitted_mesh_count];
  submit->mesh = mesh;
  submit->material = material;
  submit->first_index = first_index;
  submit->index_count = index_count;
  submit->world = world;
  submit->view_id = view_id;
  submit->flags = flags;
  submit->instance_offset = 0;
  submit->instance_count = 0;
  submit->instance_set = ldk_renderer_instance_set_null();
  submit->uv_rect = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
  renderer->submitted_mesh_count += 1;
  return true;
}

static bool s_renderer_instance_submit_storage_ensure(
    LDKRenderer *renderer, u32 count)
{
  if (count <= renderer->submitted_instance_capacity)
  {
    return true;
  }

  u32 capacity = renderer->submitted_instance_capacity;
  capacity = capacity <= UINT32_MAX / sizeof(Mat4) / 2u
      ? capacity * 2u : count;
  if (capacity < count)
  {
    capacity = count;
  }
  if (capacity == 0u)
  {
    capacity = count;
  }
  if (capacity < count || capacity > UINT32_MAX / sizeof(Mat4) ||
      capacity > UINT32_MAX / sizeof(LDKRHIColor))
  {
    return false;
  }

  Mat4 *worlds = LDK_RENDERER_ALLOC((size_t)capacity * sizeof(Mat4));
  LDKRHIColor *colors =
      LDK_RENDERER_ALLOC((size_t)capacity * sizeof(LDKRHIColor));
  if (!worlds || !colors)
  {
    LDK_RENDERER_FREE(worlds);
    LDK_RENDERER_FREE(colors);
    return false;
  }

  if (renderer->submitted_instance_count)
  {
    memcpy(worlds, renderer->submitted_instance_worlds,
        (size_t)renderer->submitted_instance_count * sizeof(Mat4));
    memcpy(colors, renderer->submitted_instance_colors,
        (size_t)renderer->submitted_instance_count * sizeof(LDKRHIColor));
  }

  LDK_RENDERER_FREE(renderer->submitted_instance_worlds);
  LDK_RENDERER_FREE(renderer->submitted_instance_colors);
  renderer->submitted_instance_worlds = worlds;
  renderer->submitted_instance_colors = colors;
  renderer->submitted_instance_capacity = capacity;
  return true;
}

bool ldk_renderer_submit_mesh_instances_colored(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index, u32 index_count,
    Mat4 parent_world, const Mat4 *instances, const u32 *instance_colors,
    u32 instance_count, u32 flags)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      (instance_count && !instances) ||
      instance_count > UINT32_MAX / sizeof(Mat4) ||
      renderer->submitted_instance_count >
          UINT32_MAX / sizeof(Mat4) - instance_count)
  {
    return false;
  }

  for (u32 i = 0; i < 16; ++i)
  {
    if (!isfinite(parent_world.m[i]))
    {
      return false;
    }
  }
  for (u32 i = 0; i < instance_count; ++i)
  {
    Mat4 world = mat4_mul(parent_world, instances[i]);
    for (u32 j = 0; j < 16; ++j)
    {
      if (!isfinite(world.m[j]))
      {
        return false;
      }
    }
  }

  // Validate and reserve one submit, then commit its instance payload.
  if (!s_renderer_submit_mesh(renderer, view_id, mesh, material,
          first_index, index_count, parent_world, flags))
  {
    return false;
  }
  LDKRendererMeshSubmit *submit =
      &renderer->submitted_meshes[--renderer->submitted_mesh_count];
  if (!instance_count)
  {
    return true;
  }

  u32 needed = renderer->submitted_instance_count + instance_count;
  if (!s_renderer_instance_submit_storage_ensure(renderer, needed))
  {
    return false;
  }

  submit->instance_offset = renderer->submitted_instance_count;
  submit->instance_count = instance_count;
  for (u32 i = 0; i < instance_count; ++i)
  {
    u32 index = submit->instance_offset + i;
    renderer->submitted_instance_worlds[index] =
        mat4_mul(parent_world, instances[i]);
    renderer->submitted_instance_colors[index] =
        ldk_renderer_color_from_rgba32(
            instance_colors ? instance_colors[i] : 0xffffffffu);
  }
  renderer->submitted_instance_count = needed;
  ++renderer->submitted_mesh_count;
  return true;
}

bool ldk_renderer_submit_mesh_instances(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index, u32 index_count,
    Mat4 parent_world, const Mat4 *instances, u32 instance_count, u32 flags)
{
  return ldk_renderer_submit_mesh_instances_colored(renderer, view_id, mesh,
      material, first_index, index_count, parent_world, instances, NULL,
      instance_count, flags);
}

bool ldk_renderer_submit_mesh_instance_set(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index, u32 index_count,
    LDKResourceInstanceSet instance_set, u32 flags)
{
  LDKRendererInstanceSetResource *resource =
      s_renderer_instance_set_get_resource(renderer, instance_set);
  if (resource == NULL || resource->instance_count == 0u ||
      view_id == LDK_RENDERER_VIEW_INVALID)
  {
    return false;
  }

  if (!s_renderer_submit_mesh(renderer, view_id, mesh, material, first_index,
          index_count, mat4_identity(), flags))
  {
    return false;
  }

  renderer->submitted_meshes[renderer->submitted_mesh_count - 1u]
      .instance_set = instance_set;
  return true;
}

bool ldk_renderer_submit_mesh_instance_set_range(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index, u32 index_count,
    LDKResourceInstanceSet instance_set, u32 first_instance,
    u32 instance_count, u32 flags)
{
  LDKRendererInstanceSetResource *resource =
      s_renderer_instance_set_get_resource(renderer, instance_set);
  LDKRendererMeshSubmit *submit;

  if (resource == NULL || instance_count == 0u ||
      first_instance > resource->instance_count ||
      instance_count > resource->instance_count - first_instance ||
      view_id == LDK_RENDERER_VIEW_INVALID)
  {
    return false;
  }

  if (!s_renderer_submit_mesh(renderer, view_id, mesh, material, first_index,
          index_count, mat4_identity(), flags))
  {
    return false;
  }

  submit = &renderer->submitted_meshes[renderer->submitted_mesh_count - 1u];
  submit->instance_offset = first_instance;
  submit->instance_count = instance_count;
  submit->instance_set = instance_set;
  return true;
}

bool ldk_renderer_submit_mesh(LDKRenderer *renderer, LDKResourceMesh mesh,
    LDKResourceMaterial material, Mat4 world)
{
  return ldk_renderer_submit_mesh_with_flags(renderer, mesh, material, world,
      LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS);
}

bool ldk_renderer_submit_mesh_with_flags(LDKRenderer *renderer,
    LDKResourceMesh mesh, LDKResourceMaterial material, Mat4 world, u32 flags)
{
  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  return s_renderer_submit_mesh(renderer, LDK_RENDERER_VIEW_ALL, mesh, material,
      0, resource->index_count, world, flags);
}

bool ldk_renderer_submit_mesh_range(
    LDKRenderer* renderer, LDKResourceMesh mesh,
    LDKResourceMaterial material, u32 first_index,
    u32 index_count, Mat4 world)
{
  return s_renderer_submit_mesh(renderer, LDK_RENDERER_VIEW_ALL, mesh, material,
      first_index, index_count, world,
      LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS);
}

bool ldk_renderer_submit_mesh_range_with_flags(LDKRenderer *renderer,
    LDKResourceMesh mesh, LDKResourceMaterial material, u32 first_index,
    u32 index_count, Mat4 world, u32 flags)
{
  return s_renderer_submit_mesh(renderer, LDK_RENDERER_VIEW_ALL, mesh, material,
      first_index, index_count, world, flags);
}

bool ldk_renderer_submit_mesh_to_view(LDKRenderer* renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, Mat4 world)
{
  if (view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return false;
  }

  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  return s_renderer_submit_mesh(renderer, view_id, mesh, material, 0,
      resource->index_count, world, LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS);
}

bool ldk_renderer_submit_skybox_to_view(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceSkybox skybox)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL ||
      !ldk_renderer_skybox_is_valid(renderer, skybox))
  {
    return false;
  }

  LDKRendererView *view = s_renderer_view_find(renderer, view_id);
  if (!view || !view->submitted)
  {
    return false;
  }

  view->skybox = skybox;
  return true;
}

bool ldk_renderer_submit_grid_to_view(LDKRenderer* renderer,
    LDKRendererViewId view_id, Vec3 grid_center,
    float grid_extent, float grid_spacing)
{
  if (renderer == NULL || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL ||
      grid_extent <= 0.0f || grid_spacing <= 0.0f)
  {
    return false;
  }

  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (view == NULL || !view->submitted)
  {
    return false;
  }

  view->grid_center = grid_center;
  view->grid_extent = grid_extent;
  view->grid_spacing = grid_spacing;
  view->grid_submitted = true;
  return true;
}

bool ldk_renderer_submit_overlay_mesh_to_view(LDKRenderer* renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    LDKResourceMaterial material, Mat4 world)
{
  if (view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return false;
  }

  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  return s_renderer_submit_mesh(renderer, view_id, mesh, material,
      0, resource->index_count, world,
      LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY);
}

bool ldk_renderer_submit_orthographic_overlay_mesh_to_view(
    LDKRenderer* renderer, LDKRendererViewId view_id,
    LDKResourceMesh mesh, LDKResourceMaterial material, Mat4 pixel_transform)
{
  if (view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL)
  {
    return false;
  }
  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }
  return s_renderer_submit_mesh(renderer, view_id, mesh, material,
      0, resource->index_count, pixel_transform,
      LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY |
          LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE);
}

bool ldk_renderer_submit_wireframe_mesh_to_view(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh, Mat4 world, u32 color,
    float line_width, u32 flags)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      view_id == LDK_RENDERER_VIEW_ALL || !isfinite(line_width) ||
      line_width <= 0.0f ||
      (flags & ~LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD) != 0 ||
      !ldk_renderer_mesh_is_valid(renderer, mesh))
  {
    return false;
  }

  if (renderer->submitted_wireframe_count ==
      renderer->submitted_wireframe_capacity)
  {
    u32 capacity = renderer->submitted_wireframe_capacity == 0
        ? 16u : renderer->submitted_wireframe_capacity * 2u;
    if (capacity <= renderer->submitted_wireframe_capacity ||
        capacity > UINT32_MAX / sizeof(LDKRendererWireframeSubmit))
    {
      return false;
    }

    size_t size = (size_t)capacity * sizeof(LDKRendererWireframeSubmit);
    LDKRendererWireframeSubmit *submits =
        LDK_RENDERER_REALLOC(renderer->submitted_wireframes, size);
    if (!submits)
    {
      return false;
    }

    renderer->submitted_wireframes = submits;
    renderer->submitted_wireframe_capacity = capacity;
  }

  LDKRendererWireframeSubmit *submit =
      &renderer->submitted_wireframes[renderer->submitted_wireframe_count++];
  submit->mesh = mesh;
  submit->world = world;
  submit->view_id = view_id;
  submit->color = color;
  submit->line_width = line_width;
  submit->flags = flags;
  return true;
}
