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
*  - the shadow pass still walks submitted_meshes[] directly;
*  - overlays preserve submission order;
*  - each view builds its own opaque render queue.
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
* Two submissions may be instanced together only when they have the same:
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
* The shadow pass evaluates CAST_SHADOWS independently while walking the
* original submissions.
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
* Compatible opaque runs with more than one item use the instanced mesh
* pipelines when possible. Per-instance data currently consists only of the
* world matrix.
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
* Locations 4..7 and location 8 must remain non-overlapping when changing
* LDKMeshVertex or the built-in mesh shaders.
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
#include <math.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer);
static void s_renderer_mesh_pass_terminate(LDKRendererMeshPass* pass);
static void s_renderer_grid_pass_terminate(LDKRendererGridPass* pass);
static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer);
static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer);
static void s_renderer_destroy_texture_resources(LDKRenderer* renderer);
static void s_renderer_destroy_material_resources(LDKRenderer* renderer);
static LDKRendererMaterialResource* s_renderer_material_get_resource(
    LDKRenderer* renderer,
    LDKResourceMaterial material);
static LDKRendererTextureResource* s_renderer_texture_get_resource(
    LDKRenderer* renderer,
    LDKResourceTexture texture);
static void s_renderer_ui_pass_remove_texture_bindings(
    LDKRendererUIPass* renderer,
    LDKRHITexture texture);
static void s_renderer_mesh_pass_remove_texture_bindings(
    LDKRendererMeshPass* pass,
    LDKRHITexture texture);
static void s_renderer_shadow_pass_remove_texture_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture);
static LDKRHISampler s_renderer_texture_sampler_from_rhi_texture(
    LDKRenderer* renderer,
    LDKRHITexture texture);

typedef struct LDKRendererUIParams
{
  float viewport_size[2];
} LDKRendererUIParams;

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(
    LDKRHIContext* rhi, u32 shader, u32 stage);

inline LDKRHIColor ldk_renderer_color_from_rgba32(u32 color)
{
  LDKRHIColor result = {0};
  result.r = (float)((color >> 24) & 0xffu) / 255.0f;
  result.g = (float)((color >> 16) & 0xffu) / 255.0f;
  result.b = (float)((color >> 8) & 0xffu) / 255.0f;
  result.a = (float)(color & 0xffu) / 255.0f;

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
    LDKRendererTarget* target, i32 width, i32 height)
{
  if (renderer == NULL || renderer->rhi == NULL || target == NULL)
  {
    return false;
  }

  if (width <= 0 || height <= 0)
  {
    return false;
  }

  target->color_format = LDK_RHI_FORMAT_RGBA8_UNORM;
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
  depth_desc.usage = LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL;

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
    LDKRendererTarget* target, i32 width, i32 height)
{
  if (target != NULL &&
      target->color_texture != LDK_RHI_INVALID_RESOURCE &&
      target->depth_texture != LDK_RHI_INVALID_RESOURCE &&
      target->width == width && target->height == height)
  {
    return true;
  }

  s_renderer_target_destroy(renderer, target);
  return s_renderer_target_create(renderer, target, width, height);
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
      view->separate_overlay = false;
      index += 1;
      continue;
    }

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

  return true;
}

static bool s_renderer_mesh_resource_create_buffers(LDKRenderer* renderer,
    LDKRendererMeshResource* resource, LDKRendererMeshDesc const* desc)
{
  LDKMeshVertex *sanitized_vertices = NULL;
  const LDKMeshVertex *upload_vertices = desc->vertices;

  if (!desc->has_tangents)
  {
    size_t vertex_bytes = (size_t)desc->vertex_count * sizeof(LDKMeshVertex);
    sanitized_vertices = (LDKMeshVertex *)LDK_RENDERER_ALLOC(vertex_bytes);
    if (!sanitized_vertices)
    {
      return false;
    }

    for (u32 i = 0; i < desc->vertex_count; ++i)
    {
      sanitized_vertices[i].position = desc->vertices[i].position;
      sanitized_vertices[i].normal = desc->vertices[i].normal;
      sanitized_vertices[i].uv = desc->vertices[i].uv;
      sanitized_vertices[i].color = desc->vertices[i].color;
      sanitized_vertices[i].tangent = vec4_make(0.0f, 0.0f, 0.0f, 0.0f);
    }
    upload_vertices = sanitized_vertices;
  }

  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = desc->vertex_count * (u32)sizeof(LDKMeshVertex);
  vertex_desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = upload_vertices;

  resource->vertex_buffer =
      ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  LDK_RENDERER_FREE(sanitized_vertices);
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
  renderer->mesh_count += 1;

  LDKResourceMesh mesh = {0};
  mesh.id = (LDKRHIResource)(index + 1u);
  return mesh;
}

bool ldk_renderer_mesh_update(LDKRenderer* renderer, LDKResourceMesh mesh,
    LDKRendererMeshDesc const* desc)
{
  if (renderer == NULL || !renderer->is_initialized ||
      !s_renderer_mesh_desc_is_valid(desc))
  {
    return false;
  }

  LDKRendererMeshResource* resource =
      s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->index_buffer);
  resource->vertex_buffer = LDK_RHI_INVALID_RESOURCE;
  resource->index_buffer = LDK_RHI_INVALID_RESOURCE;
  resource->vertex_count = 0;
  resource->index_count = 0;

  if (!s_renderer_mesh_resource_create_buffers(renderer, resource, desc))
  {
    resource->alive = false;
    return false;
  }

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

  LDK_RENDERER_FREE(renderer->submitted_lights);
  renderer->submitted_lights = NULL;
  renderer->submitted_light_count = 0;
  renderer->submitted_light_capacity = 0;

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

LDK_STATIC_ASSERT(LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT <= UINT8_MAX,
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
    LDKRendererView const* view, u64** out_items, u32* out_item_count)
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
        material->selection == LDK_RENDERER_MATERIAL_SELECTION_INVALID)
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
  if (a == NULL || b == NULL)
  {
    return false;
  }

  /* The opaque queue already fixes the view/pass. CAST_SHADOWS belongs to
   * the shadow pass and must not split the visible batch. */
  return a->mesh.id == b->mesh.id && a->material.id == b->material.id &&
         a->first_index == b->first_index &&
         a->index_count == b->index_count;
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
} LDKRendererMeshObjectParams;

typedef struct LDKRendererMeshMaterialParams
{
  LDKRHIColor color;
  /* GLSL u_surface: specular, shininess, emission, alpha cutoff. */
  float specular;
  float shininess;
  float emission;
  float alpha_cutoff;
} LDKRendererMeshMaterialParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererMeshCameraParams) == 144,
    mesh_camera_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererMeshCameraParams, camera_position) == 128,
    mesh_camera_position_std140_offset);
LDK_STATIC_ASSERT(sizeof(LDKRendererMeshMaterialParams) == 32,
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

typedef struct LDKRendererShadowMaterialParams
{
  float color_alpha;
  float alpha_cutoff;
  float padding[2];
} LDKRendererShadowMaterialParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererShadowMaterialParams) == 16,
    shadow_material_std140_size);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererShadowMaterialParams, alpha_cutoff) == 4,
    shadow_material_alpha_cutoff_std140_offset);

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
  float ambient[4];
  LDKRendererLightParams lights[LDK_RENDERER_MAX_LIGHTS_PER_VIEW];
  Mat4 shadow_view_projection;
  float shadow_params[4]; // depth bias, slope bias, reserved, reserved
} LDKRendererLightingParams;

LDK_STATIC_ASSERT(sizeof(LDKRendererLightParams) == 64, light_std140_size);
LDK_STATIC_ASSERT(offsetof(LDKRendererLightingParams, lights) == 32,
    lighting_std140_offset);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererLightingParams, shadow_view_projection) == 1056,
    shadow_std140_offset);
LDK_STATIC_ASSERT(
    sizeof(LDKRendererLightingParams) == 1136, lighting_std140_size);

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

bool ldk_renderer_ambient_light_set(
    LDKRenderer *renderer, u32 color, float intensity)
{
  if (!renderer || !renderer->is_initialized ||
      !isfinite(intensity) || intensity < 0.0f)
  {
    return false;
  }

  renderer->ambient_light.color = color;
  renderer->ambient_light.intensity = intensity;
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
  LDKRHIColor ambient =
      ldk_renderer_color_from_rgba32(renderer->ambient_light.color);
  params.ambient[0] = ambient.r;
  params.ambient[1] = ambient.g;
  params.ambient[2] = ambient.b;
  params.ambient[3] = renderer->ambient_light.intensity;

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

// One map is reused sequentially: shadow(view), meshes(view), next view.
static void s_renderer_shadow_pass_terminate(LDKRendererShadowPass *pass)
{
  if (pass->rhi)
  {
    for (u32 i = 0; i < pass->cutout_bindings_cache_count; ++i)
    {
      ldk_rhi_bindings_destroy(
          pass->rhi, pass->cutout_bindings_cache[i].bindings);
    }

    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->cutout_vertex_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    ldk_rhi_buffer_destroy(pass->rhi, pass->material_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_sampler_destroy(pass->rhi, pass->sampler);
    ldk_rhi_texture_destroy(pass->rhi, pass->depth_texture);
  }
  LDK_RENDERER_FREE(pass->cutout_bindings_cache);
  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_shadow_pass_grow_cutout_bindings_cache(
    LDKRendererShadowPass *pass)
{
  u32 new_capacity = pass->cutout_bindings_cache_capacity == 0
      ? 16
      : pass->cutout_bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererBindingsCacheEntry);
  LDKRendererBindingsCacheEntry *new_cache =
      pass->cutout_bindings_cache == NULL
      ? (LDKRendererBindingsCacheEntry *)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererBindingsCacheEntry *)LDK_RENDERER_REALLOC(
            pass->cutout_bindings_cache, new_size);

  if (!new_cache)
  {
    return false;
  }

  pass->cutout_bindings_cache = new_cache;
  pass->cutout_bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_shadow_pass_create_cutout_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture, LDKRHISampler sampler)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 4;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_size = sizeof(Mat4);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_size = sizeof(Mat4);
  desc.bindings[2].slot = 2;
  desc.bindings[2].buffer = pass->material_buffer;
  desc.bindings[2].buffer_size = sizeof(LDKRendererShadowMaterialParams);
  desc.bindings[3].slot = 3;
  desc.bindings[3].texture = texture;
  desc.bindings[3].sampler = sampler;
  return ldk_rhi_bindings_create(pass->rhi, &desc);
}

static LDKRHIBindings s_renderer_shadow_pass_get_cutout_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture, LDKRHISampler sampler)
{
  for (u32 i = 0; i < pass->cutout_bindings_cache_count; ++i)
  {
    LDKRendererBindingsCacheEntry *entry = &pass->cutout_bindings_cache[i];
    if (entry->texture == texture && entry->sampler == sampler)
    {
      return entry->bindings;
    }
  }

  if (pass->cutout_bindings_cache_count ==
          pass->cutout_bindings_cache_capacity &&
      !s_renderer_shadow_pass_grow_cutout_bindings_cache(pass))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIBindings bindings =
      s_renderer_shadow_pass_create_cutout_bindings(pass, texture, sampler);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererBindingsCacheEntry *entry =
      &pass->cutout_bindings_cache[pass->cutout_bindings_cache_count++];
  entry->texture = texture;
  entry->sampler = sampler;
  entry->bindings = bindings;
  return bindings;
}

static void s_renderer_shadow_pass_remove_texture_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture)
{
  if (!pass || !pass->rhi || texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < pass->cutout_bindings_cache_count)
  {
    LDKRendererBindingsCacheEntry *entry = &pass->cutout_bindings_cache[index];
    if (entry->texture != texture)
    {
      ++index;
      continue;
    }

    ldk_rhi_bindings_destroy(pass->rhi, entry->bindings);
    --pass->cutout_bindings_cache_count;
    if (index != pass->cutout_bindings_cache_count)
    {
      *entry = pass->cutout_bindings_cache[pass->cutout_bindings_cache_count];
    }
    memset(&pass->cutout_bindings_cache[pass->cutout_bindings_cache_count], 0,
        sizeof(*entry));
  }
}

static bool s_renderer_shadow_pass_initialize(LDKRendererShadowPass *pass,
    LDKRHIContext *rhi, u32 resolution, float distance)
{
  memset(pass, 0, sizeof(*pass));
  if (!s_renderer_shadow_settings_valid(resolution, distance))
  {
    return false;
  }
  pass->resolution = resolution;
  pass->distance = distance;
  pass->rhi = rhi;

  LDKRHITextureDesc texture;
  ldk_rhi_texture_desc_defaults(&texture);
  texture.width = pass->resolution;
  texture.height = pass->resolution;
  texture.format = LDK_RHI_FORMAT_D32_FLOAT;
  texture.usage =
      LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL | LDK_RHI_TEXTURE_USAGE_SAMPLED;
  pass->depth_texture = ldk_rhi_texture_create(rhi, &texture);

  LDKRHISamplerDesc sampler;
  ldk_rhi_sampler_desc_defaults(&sampler);
  sampler.min_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mag_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mip_filter = LDK_RHI_FILTER_NEAREST;
  sampler.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  pass->sampler = ldk_rhi_sampler_create(rhi, &sampler);

  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  pass->cutout_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS_CUTOUT, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->cutout_fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS_CUTOUT, LDK_RHI_SHADER_STAGE_FRAGMENT);

  LDKRHIBufferDesc buffer;
  ldk_rhi_buffer_desc_defaults(&buffer);
  buffer.size = sizeof(Mat4);
  buffer.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  buffer.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->camera_buffer = ldk_rhi_buffer_create(rhi, &buffer);
  pass->object_buffer = ldk_rhi_buffer_create(rhi, &buffer);
  buffer.size = sizeof(LDKRendererShadowMaterialParams);
  pass->material_buffer = ldk_rhi_buffer_create(rhi, &buffer);

  if (!pass->depth_texture || !pass->sampler || !pass->vertex_shader_module ||
      !pass->fragment_shader_module || !pass->cutout_vertex_shader_module ||
      !pass->cutout_fragment_shader_module || !pass->camera_buffer ||
      !pass->object_buffer || !pass->material_buffer)
  {
    goto fail;
  }

  LDKRHIBindingsLayoutDesc layout;
  ldk_rhi_bindings_layout_desc_defaults(&layout);
  layout.entry_count = 4;
  layout.entries[0].slot = 0;
  layout.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[0].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  layout.entries[1].slot = 1;
  layout.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  layout.entries[2].slot = 2;
  layout.entries[2].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[2].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  layout.entries[3].slot = 3;
  layout.entries[3].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  layout.entries[3].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  pass->bindings_layout = ldk_rhi_bindings_layout_create(rhi, &layout);
  if (!pass->bindings_layout)
  {
    goto fail;
  }

  LDKRHIPipelineDesc pipeline;
  ldk_rhi_pipeline_desc_defaults(&pipeline);
  pipeline.vertex_shader_module = pass->vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pipeline.bindings_layout = pass->bindings_layout;
  pipeline.vertex_layout.stride = sizeof(LDKMeshVertex);
  pipeline.vertex_layout.attribute_count = 1;
  pipeline.vertex_layout.attributes[0].location = 0;
  pipeline.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  pipeline.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKMeshVertex, position);
  pipeline.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  pipeline.color_attachment_count = 0;
  pipeline.depth_format = LDK_RHI_FORMAT_D32_FLOAT;
  pipeline.depth_state.test_enabled = true;
  pipeline.depth_state.write_enabled = true;
  pipeline.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
  pipeline.blend_state.enabled = false;
  // Two-sided casting also handles thin meshes and mirrored transforms.
  pipeline.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pipeline.raster_state.scissor_enabled = false;
  // Use the rasterized triangle's depth slope, including grazing surfaces.
  pipeline.raster_state.depth_bias_enabled = true;
  pipeline.raster_state.depth_bias_slope_factor = 1.0f;
  pipeline.raster_state.depth_bias_constant_factor = 2.0f;
  pass->pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->pipeline)
  {
    goto fail;
  }

  pipeline.vertex_shader_module = pass->cutout_vertex_shader_module;
  pipeline.fragment_shader_module = pass->cutout_fragment_shader_module;
  pipeline.vertex_layout.attribute_count = 2;
  pipeline.vertex_layout.attributes[1].location = 2;
  pipeline.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[1].offset = (u32)offsetof(LDKMeshVertex, uv);
  pass->cutout_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->cutout_pipeline)
  {
    goto fail;
  }

  LDKRHIBindingsDesc bindings;
  ldk_rhi_bindings_desc_defaults(&bindings);
  bindings.layout = pass->bindings_layout;
  bindings.binding_count = 2;
  bindings.bindings[0].slot = 0;
  bindings.bindings[0].buffer = pass->camera_buffer;
  bindings.bindings[0].buffer_size = sizeof(Mat4);
  bindings.bindings[1].slot = 1;
  bindings.bindings[1].buffer = pass->object_buffer;
  bindings.bindings[1].buffer_size = sizeof(Mat4);
  pass->bindings = ldk_rhi_bindings_create(rhi, &bindings);
  if (!pass->bindings)
  {
    goto fail;
  }
  return true;

fail:
  s_renderer_shadow_pass_terminate(pass);
  return false;
}

static bool s_renderer_shadow_camera(const LDKRendererShadowPass *pass,
    const LDKRendererView *view, Vec3 direction,
    LDKRendererLightingParams *params)
{
  const float receiver_constant_bias_texels = 0.5f;
  const float receiver_slope_bias_texels = 5.0f;

  bool invertible = false;
  Mat4 inverse =
      mat4_inverse_full(mat4_mul(view->projection, view->view), &invertible);
  if (!invertible)
  {
    return false;
  }

  Vec3 corners[8];
  Vec3 center = vec3_make(0, 0, 0);
  for (u32 i = 0; i < 4; ++i)
  {
    float x = (i & 1) ? 1.0f : -1.0f;
    float y = (i & 2) ? 1.0f : -1.0f;
    Vec3 near_corner = mat4_mul_point(inverse, vec3_make(x, y, -1));
    Vec3 far_corner = mat4_mul_point(inverse, vec3_make(x, y, 1));
    float near_depth = -mat4_mul_point(view->view, near_corner).z;
    float far_depth = -mat4_mul_point(view->view, far_corner).z;

    if (!isfinite(near_depth) || !isfinite(far_depth) ||
        near_depth >= pass->distance || far_depth <= near_depth)
    {
      return false;
    }

    float t =
        fminf(1.0f, (pass->distance - near_depth) / (far_depth - near_depth));
    corners[i] = near_corner;
    corners[i + 4] =
        vec3_add(near_corner, vec3_mul(vec3_sub(far_corner, near_corner), t));
    center = vec3_add(center, vec3_add(corners[i], corners[i + 4]));
  }

  center = vec3_mul(center, 0.125f);

  float radius = 0.0f;
  for (u32 i = 0; i < 8; ++i)
  {
    Vec3 delta = vec3_sub(corners[i], center);
    radius = fmaxf(radius, sqrtf(vec3_dot(delta, delta)));
  }

  if (!isfinite(radius) || radius < 1e-4f)
  {
    return false;
  }

  // Rounded sphere fit keeps the map extent stable under camera rotation.
  radius = ceilf(radius * 16.0f) / 16.0f;
  radius *= (float)pass->resolution / (float)(pass->resolution - 2u);

  float texel = 2.0f * radius / (float)pass->resolution;

  Vec3 up =
      fabsf(direction.y) > 0.99f ? vec3_make(0, 0, 1) : vec3_make(0, 1, 0);

  Mat4 light_view = mat4_look_at_rh(vec3_make(0, 0, 0), direction, up);
  Vec3 light_center = mat4_mul_point(light_view, center);

  light_center.x = floorf(light_center.x / texel + 0.5f) * texel;
  light_center.y = floorf(light_center.y / texel + 0.5f) * texel;

  float min_z = light_center.z - radius;
  float max_z =
      light_center.z + radius + LDK_RENDERER_SHADOW_CASTER_MARGIN;

  // Include upstream, off-camera casters. This bounded first pass has no CSM.
  Mat4 projection =
      mat4_orthographic_rh_no(light_center.x - radius,
          light_center.x + radius,
          light_center.y - radius,
          light_center.y + radius,
          -max_z,
          -min_z);

  params->shadow_view_projection = mat4_mul(projection, light_view);

  float depth_range = max_z - min_z;

  // Bias is expressed in shadow-map texels and converted to depth space.
  params->shadow_params[0] =
      receiver_constant_bias_texels * texel / depth_range;
  params->shadow_params[1] =
      receiver_slope_bias_texels * texel / depth_range;

  return true;
}

static bool s_renderer_shadow_material_is_cutout(
    const LDKRendererMaterialResource *material)
{
  return material &&
      (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
          material->selection ==
              LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT);
}

static void s_renderer_shadow_pass_draw(LDKRenderer *renderer,
    const LDKRendererView *view, LDKRendererViewLighting *lighting)
{
  LDKRendererShadowPass *pass = &renderer->shadow_pass;
  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  if (!lighting->shadow_light ||
      !s_renderer_shadow_camera(
          pass, view, lighting->shadow_light->direction, &lighting->params))
  {
    lighting->params.count[1] = -1;
    return;
  }

  ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0, sizeof(Mat4),
      &lighting->params.shadow_view_projection);

  LDKRHIPassDesc desc;
  ldk_rhi_pass_desc_defaults(&desc);
  desc.color_attachment_count = 0;
  desc.depth_attachment.valid = true;
  desc.depth_attachment.texture = pass->depth_texture;
  desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
  desc.depth_attachment.depth_store_op = LDK_RHI_STORE_OP_STORE;
  desc.depth_attachment.clear_depth = 1.0f;
  desc.has_viewport = true;
  desc.viewport.width = (float)pass->resolution;
  desc.viewport.height = (float)pass->resolution;
  desc.viewport.min_depth = 0.0f;
  desc.viewport.max_depth = 1.0f;
  ldk_rhi_pass_begin(pass->rhi, &desc);

  LDKRHIPipeline bound_pipeline = LDK_RHI_INVALID_RESOURCE;
  LDKRHIBindings bound_bindings = LDK_RHI_INVALID_RESOURCE;

  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    const LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[i];
    if (!(submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS) ||
        (submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    LDKRendererMeshResource *mesh =
        s_renderer_mesh_get_resource(renderer, submit->mesh);
    LDKRendererMaterialResource *material =
        s_renderer_material_get_resource(renderer, submit->material);
    if (!mesh || !mesh->index_count || !material)
    {
      continue;
    }

    LDKRHIPipeline pipeline = pass->pipeline;
    LDKRHIBindings bindings = pass->bindings;
    if (s_renderer_shadow_material_is_cutout(material))
    {
      LDKRendererTextureResource *texture =
          s_renderer_texture_get_resource(renderer, material->desc.texture);
      if (!texture)
      {
        continue;
      }

      bindings = s_renderer_shadow_pass_get_cutout_bindings(
          pass, texture->texture, texture->sampler);
      if (bindings == LDK_RHI_INVALID_RESOURCE)
      {
        continue;
      }

      LDKRendererShadowMaterialParams material_params = {0};
      material_params.color_alpha =
          ldk_renderer_color_from_rgba32(material->desc.color).a;
      material_params.alpha_cutoff = material->desc.alpha_cutoff;
      ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
          sizeof(material_params), &material_params);
      pipeline = pass->cutout_pipeline;
    }

    if (pipeline != bound_pipeline)
    {
      ldk_rhi_pipeline_bind(pass->rhi, pipeline);
      bound_pipeline = pipeline;
    }
    if (bindings != bound_bindings)
    {
      ldk_rhi_bindings_bind(pass->rhi, bindings);
      bound_bindings = bindings;
    }

    ldk_rhi_buffer_update(
        pass->rhi, pass->object_buffer, 0, sizeof(Mat4), &submit->world);
    ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
    ldk_rhi_index_buffer_bind(
        pass->rhi, mesh->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

    LDKRHIDrawIndexedDesc draw = {0};
    draw.first_index = submit->first_index;
    draw.index_count = submit->index_count;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->shadow_draw_call_count += 1;
  }

  ldk_rhi_pass_end(pass->rhi);
}

static bool s_renderer_mesh_pass_create_shaders(LDKRendererMeshPass* pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  pass->overlay_fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS_UNLIT,
      LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->overlay_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->fragment_shader_module = LDK_RHI_INVALID_RESOURCE;
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  pass->textured_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_unlit_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_UNLIT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_unlit_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_cutout_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_CUTOUT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_cutout_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_unlit_cutout_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_UNLIT_CUTOUT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_unlit_cutout_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->instanced_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS_INSTANCED, LDK_RHI_SHADER_STAGE_VERTEX);
  return pass->instanced_vertex_shader_module != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_bindings_layout(
    LDKRendererMeshPass* pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 8;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[2].slot = 2;
  desc.entries[2].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[2].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[3].slot = 3;
  desc.entries[3].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[3].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[4].slot = 4;
  desc.entries[4].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[4].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[5].slot = 5;
  desc.entries[5].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[5].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[6].slot = 6;
  desc.entries[6].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[6].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[7].slot = 7;
  desc.entries[7].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[7].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;

  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static void s_renderer_mesh_pass_instance_layout(
    LDKRHIPipelineDesc* desc)
{
  desc->vertex_buffer_layout_count = 2;
  desc->vertex_buffer_layouts[0] = desc->vertex_layout;
  desc->vertex_buffer_layouts[0].input_rate =
      LDK_RHI_VERTEX_INPUT_RATE_PER_VERTEX;

  LDKRHIVertexBufferLayoutDesc* instance = &desc->vertex_buffer_layouts[1];
  memset(instance, 0, sizeof(*instance));
  instance->stride = sizeof(Mat4);
  instance->attribute_count = 4;
  instance->input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE;
  for (u32 i = 0; i < 4; ++i)
  {
    instance->attributes[i].location = 4u + i;
    instance->attributes[i].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
    instance->attributes[i].offset = i * 4u * (u32)sizeof(float);
  }
}

static bool s_renderer_mesh_pass_create_pipeline(LDKRendererMeshPass* pass)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);

  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;
  desc.bindings_layout = pass->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = false;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_BACK;
  desc.raster_state.front_face = LDK_RHI_FRONT_FACE_CCW;
  desc.raster_state.scissor_enabled = false;

  desc.vertex_layout.stride = sizeof(LDKMeshVertex);
  desc.vertex_layout.attribute_count = 5;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKMeshVertex, position);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKMeshVertex, normal);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[2].offset =
      (u32)offsetof(LDKMeshVertex, uv);
  desc.vertex_layout.attributes[3].location = 3;
  desc.vertex_layout.attributes[3].format =
      LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[3].offset =
      (u32)offsetof(LDKMeshVertex, color);
  desc.vertex_layout.attributes[4].location = 8;
  desc.vertex_layout.attributes[4].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
  desc.vertex_layout.attributes[4].offset =
      (u32)offsetof(LDKMeshVertex, tangent);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  pass->vertex_color_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  pass->vertex_color_unlit_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    pass->vertex_color_pipeline = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->overlay_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->overlay_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    pass->vertex_color_unlit_pipeline = LDK_RHI_INVALID_RESOURCE;
    pass->vertex_color_pipeline = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->textured_overlay_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_overlay_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.fragment_shader_module = pass->textured_cutout_fragment_shader_module;
  pass->textured_cutout_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_cutout_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module =
      pass->textured_unlit_cutout_fragment_shader_module;
  pass->textured_unlit_cutout_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_cutout_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->textured_unlit_cutout_overlay_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_cutout_overlay_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.vertex_shader_module = pass->instanced_vertex_shader_module;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  s_renderer_mesh_pass_instance_layout(&desc);

  desc.fragment_shader_module = pass->fragment_shader_module;
  pass->vertex_color_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  pass->vertex_color_unlit_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_instanced_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_cutout_fragment_shader_module;
  pass->textured_cutout_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_cutout_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module =
      pass->textured_unlit_cutout_fragment_shader_module;
  pass->textured_unlit_cutout_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  return pass->textured_unlit_cutout_instanced_pipeline !=
      LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_buffers(LDKRendererMeshPass* pass)
{
  LDKRHIBufferDesc camera_desc = {0};
  ldk_rhi_buffer_desc_defaults(&camera_desc);
  camera_desc.size = sizeof(LDKRendererMeshCameraParams);
  camera_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  camera_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->camera_buffer = ldk_rhi_buffer_create(pass->rhi, &camera_desc);
  if (pass->camera_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc object_desc = {0};
  ldk_rhi_buffer_desc_defaults(&object_desc);
  object_desc.size = sizeof(LDKRendererMeshObjectParams);
  object_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  object_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->object_buffer = ldk_rhi_buffer_create(pass->rhi, &object_desc);
  if (pass->object_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc material_desc = {0};
  ldk_rhi_buffer_desc_defaults(&material_desc);
  material_desc.size = sizeof(LDKRendererMeshMaterialParams);
  material_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  material_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->material_buffer = ldk_rhi_buffer_create(pass->rhi, &material_desc);
  if (pass->material_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  LDKRHIBufferDesc lighting_desc = material_desc;
  lighting_desc.size = sizeof(LDKRendererLightingParams);
  pass->lighting_buffer = ldk_rhi_buffer_create(pass->rhi, &lighting_desc);
  return pass->lighting_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_ensure_instance_capacity(
    LDKRendererMeshPass* pass, u32 instance_count)
{
  if (instance_count <= pass->instance_capacity)
  {
    return true;
  }

  u32 capacity = pass->instance_capacity == 0 ? 256u : pass->instance_capacity;
  while (capacity < instance_count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = instance_count;
      break;
    }
    capacity *= 2u;
  }

  if (capacity < instance_count || capacity > UINT32_MAX / sizeof(Mat4))
  {
    return false;
  }

  size_t cpu_size = (size_t)capacity * sizeof(Mat4);
  Mat4* worlds = (Mat4*)LDK_RENDERER_ALLOC(cpu_size);
  if (worlds == NULL)
  {
    return false;
  }

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = (u32)cpu_size;
  desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  LDKRHIBuffer buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (buffer == LDK_RHI_INVALID_RESOURCE)
  {
    LDK_RENDERER_FREE(worlds);
    return false;
  }

  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
  LDK_RENDERER_FREE(pass->instance_worlds);
  pass->instance_buffer = buffer;
  pass->instance_worlds = worlds;
  pass->instance_capacity = capacity;
  return true;
}

static bool s_renderer_mesh_pass_create_bindings(LDKRendererMeshPass* pass)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 5;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererMeshCameraParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_offset = 0;
  desc.bindings[1].buffer_size = sizeof(LDKRendererMeshObjectParams);
  desc.bindings[2].slot = 2;
  desc.bindings[2].buffer = pass->material_buffer;
  desc.bindings[2].buffer_offset = 0;
  desc.bindings[2].buffer_size = sizeof(LDKRendererMeshMaterialParams);
  desc.bindings[3].slot = 4;
  desc.bindings[3].buffer = pass->lighting_buffer;
  desc.bindings[3].buffer_size = sizeof(LDKRendererLightingParams);
  desc.bindings[4].slot = 5;
  desc.bindings[4].texture = pass->shadow_texture;
  desc.bindings[4].sampler = pass->shadow_sampler;

  pass->bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  return pass->bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_fallback_resources(
    LDKRendererMeshPass *pass)
{
  const float flat_normal[4] = {0.5f, 0.5f, 1.0f, 1.0f};
  const u8 white_specular[4] = {255u, 255u, 255u, 255u};
  LDKRHITextureDesc texture_desc = {0};
  LDKRHISamplerDesc sampler_desc = {0};

  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_RGBA32_FLOAT;
  texture_desc.width = 1;
  texture_desc.height = 1;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = flat_normal;
  texture_desc.initial_data_size = sizeof(flat_normal);
  pass->flat_normal_texture = ldk_rhi_texture_create(pass->rhi, &texture_desc);
  if (pass->flat_normal_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  texture_desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  texture_desc.initial_data = white_specular;
  texture_desc.initial_data_size = sizeof(white_specular);
  pass->white_specular_texture =
      ldk_rhi_texture_create(pass->rhi, &texture_desc);
  if (pass->white_specular_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  ldk_rhi_sampler_desc_defaults(&sampler_desc);
  sampler_desc.min_filter = LDK_RHI_FILTER_NEAREST;
  sampler_desc.mag_filter = LDK_RHI_FILTER_NEAREST;
  sampler_desc.mip_filter = LDK_RHI_FILTER_NEAREST;
  pass->fallback_sampler = ldk_rhi_sampler_create(pass->rhi, &sampler_desc);
  return pass->fallback_sampler != LDK_RHI_INVALID_RESOURCE;
}

static LDKRHIBindings s_renderer_mesh_pass_create_material_bindings(
    LDKRendererMeshPass *pass, LDKRHITexture albedo_texture,
    LDKRHISampler albedo_sampler, LDKRHITexture normal_texture,
    LDKRHISampler normal_sampler, LDKRHITexture specular_texture,
    LDKRHISampler specular_sampler)
{
  LDKRHIBindingsDesc desc = {0};
  u32 binding = 0;

  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;

  desc.bindings[binding].slot = 0;
  desc.bindings[binding].buffer = pass->camera_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshCameraParams);
  binding++;
  desc.bindings[binding].slot = 1;
  desc.bindings[binding].buffer = pass->object_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshObjectParams);
  binding++;
  desc.bindings[binding].slot = 2;
  desc.bindings[binding].buffer = pass->material_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshMaterialParams);
  binding++;

  if (albedo_texture != LDK_RHI_INVALID_RESOURCE)
  {
    desc.bindings[binding].slot = 3;
    desc.bindings[binding].texture = albedo_texture;
    desc.bindings[binding].sampler = albedo_sampler;
    binding++;
  }

  desc.bindings[binding].slot = 4;
  desc.bindings[binding].buffer = pass->lighting_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererLightingParams);
  binding++;
  desc.bindings[binding].slot = 5;
  desc.bindings[binding].texture = pass->shadow_texture;
  desc.bindings[binding].sampler = pass->shadow_sampler;
  binding++;
  if (normal_texture != LDK_RHI_INVALID_RESOURCE)
  {
    desc.bindings[binding].slot = 6;
    desc.bindings[binding].texture = normal_texture;
    desc.bindings[binding].sampler = normal_sampler;
    binding++;
  }
  if (specular_texture != LDK_RHI_INVALID_RESOURCE)
  {
    desc.bindings[binding].slot = 7;
    desc.bindings[binding].texture = specular_texture;
    desc.bindings[binding].sampler = specular_sampler;
    binding++;
  }
  desc.binding_count = binding;

  return ldk_rhi_bindings_create(pass->rhi, &desc);
}

static bool s_renderer_mesh_pass_grow_material_bindings_cache(
    LDKRendererMeshPass *pass)
{
  u32 new_capacity = pass->material_bindings_cache_capacity == 0
                         ? 16
                         : pass->material_bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMeshBindingsCacheEntry);
  LDKRendererMeshBindingsCacheEntry *new_cache =
      pass->material_bindings_cache == NULL
          ? (LDKRendererMeshBindingsCacheEntry *)LDK_RENDERER_ALLOC(new_size)
          : (LDKRendererMeshBindingsCacheEntry *)LDK_RENDERER_REALLOC(
                pass->material_bindings_cache, new_size);

  if (!new_cache)
  {
    return false;
  }

  pass->material_bindings_cache = new_cache;
  pass->material_bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_mesh_pass_get_material_bindings(
    LDKRendererMeshPass *pass, LDKRHITexture albedo_texture,
    LDKRHISampler albedo_sampler, LDKRHITexture normal_texture,
    LDKRHISampler normal_sampler, LDKRHITexture specular_texture,
    LDKRHISampler specular_sampler)
{
  for (u32 i = 0; i < pass->material_bindings_cache_count; i++)
  {
    LDKRendererMeshBindingsCacheEntry *entry =
        &pass->material_bindings_cache[i];
    if (entry->albedo_texture == albedo_texture &&
        entry->albedo_sampler == albedo_sampler &&
        entry->normal_texture == normal_texture &&
        entry->normal_sampler == normal_sampler &&
        entry->specular_texture == specular_texture &&
        entry->specular_sampler == specular_sampler)
    {
      return entry->bindings;
    }
  }

  if (pass->material_bindings_cache_count ==
          pass->material_bindings_cache_capacity &&
      !s_renderer_mesh_pass_grow_material_bindings_cache(pass))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIBindings bindings = s_renderer_mesh_pass_create_material_bindings(pass,
      albedo_texture, albedo_sampler, normal_texture, normal_sampler,
      specular_texture, specular_sampler);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererMeshBindingsCacheEntry *entry =
      &pass->material_bindings_cache[pass->material_bindings_cache_count++];
  entry->albedo_texture = albedo_texture;
  entry->albedo_sampler = albedo_sampler;
  entry->normal_texture = normal_texture;
  entry->normal_sampler = normal_sampler;
  entry->specular_texture = specular_texture;
  entry->specular_sampler = specular_sampler;
  entry->bindings = bindings;
  return bindings;
}

static void s_renderer_mesh_pass_remove_texture_bindings(
    LDKRendererMeshPass* pass, LDKRHITexture texture)
{
  if (!pass || !pass->rhi || texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < pass->material_bindings_cache_count)
  {
    LDKRendererMeshBindingsCacheEntry *entry =
        &pass->material_bindings_cache[index];
    if (entry->albedo_texture != texture && entry->normal_texture != texture &&
        entry->specular_texture != texture)
    {
      index++;
      continue;
    }

    ldk_rhi_bindings_destroy(pass->rhi, entry->bindings);
    pass->material_bindings_cache_count--;
    if (index != pass->material_bindings_cache_count)
    {
      *entry =
          pass->material_bindings_cache[pass->material_bindings_cache_count];
    }
    memset(&pass->material_bindings_cache[pass->material_bindings_cache_count],
        0, sizeof(*entry));
  }
}

static bool s_renderer_mesh_pass_initialize(LDKRendererMeshPass *pass,
    LDKRendererConfig const *config, const LDKRendererShadowPass *shadow)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }
  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  pass->shadow_texture = shadow->depth_texture;
  pass->shadow_sampler = shadow->sampler;

  if (!s_renderer_mesh_pass_create_shaders(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_bindings_layout(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_pipeline(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_buffers(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_fallback_resources(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_bindings(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_mesh_pass_terminate(LDKRendererMeshPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    for (u32 i = 0; i < pass->material_bindings_cache_count; i++)
    {
      ldk_rhi_bindings_destroy(
          pass->rhi, pass->material_bindings_cache[i].bindings);
    }

    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_sampler_destroy(pass->rhi, pass->fallback_sampler);
    ldk_rhi_texture_destroy(pass->rhi, pass->white_specular_texture);
    ldk_rhi_texture_destroy(pass->rhi, pass->flat_normal_texture);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->lighting_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->material_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_cutout_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_cutout_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->vertex_color_unlit_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_cutout_overlay_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_overlay_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->overlay_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_unlit_cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_unlit_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->overlay_fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  LDK_RENDERER_FREE(pass->instance_worlds);
  LDK_RENDERER_FREE(pass->material_bindings_cache);
  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_mesh_pass_material_is_textured(
    const LDKRendererMaterialResource* material)
{
  return material != NULL &&
         (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT);
}

static bool s_renderer_mesh_pass_material_is_lit(
    const LDKRendererMaterialResource *material)
{
  return material != NULL &&
         (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR);
}

static LDKRHIPipeline s_renderer_mesh_pass_pipeline(
    const LDKRendererMeshPass* pass,
    LDKRendererMaterialSelection selection, u32 flags, bool instanced)
{
  bool overlay = (flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0;
  if (overlay)
  {
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT)
    {
      return pass->textured_unlit_cutout_overlay_pipeline;
    }
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT)
    {
      return pass->textured_overlay_pipeline;
    }
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT)
    {
      return pass->overlay_pipeline;
    }
    return LDK_RHI_INVALID_RESOURCE;
  }

  switch (selection)
  {
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR:
      return instanced ? pass->vertex_color_instanced_pipeline
                       : pass->vertex_color_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT:
      return instanced ? pass->vertex_color_unlit_instanced_pipeline
                       : pass->vertex_color_unlit_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED:
      return instanced ? pass->textured_instanced_pipeline
                       : pass->textured_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT:
      return instanced ? pass->textured_unlit_instanced_pipeline
                       : pass->textured_unlit_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT:
      return instanced ? pass->textured_cutout_instanced_pipeline
                       : pass->textured_cutout_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT:
      return instanced ? pass->textured_unlit_cutout_instanced_pipeline
                       : pass->textured_unlit_cutout_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_INVALID:
    default:
      return LDK_RHI_INVALID_RESOURCE;
  }
}

static LDKRendererMeshSubmit* s_renderer_mesh_pass_run_submit(
    LDKRenderer* renderer, LDKRendererMeshSubmit* first_submit,
    const u64* sort_items, u32 index)
{
  if (sort_items == NULL)
  {
    return index == 0 ? first_submit : NULL;
  }

  u32 submit_index = s_renderer_mesh_sort_item_submit_index(sort_items[index]);
  if (submit_index >= renderer->submitted_mesh_count)
  {
    return NULL;
  }
  return &renderer->submitted_meshes[submit_index];
}

static void s_renderer_mesh_pass_draw_run(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererFrameDomainStats* stats,
    LDKRendererMeshSubmit* first_submit, const u64* sort_items,
    u32 submit_count)
{
  if (submit_count == 0 || first_submit == NULL)
  {
    return;
  }

  LDKRendererMeshSubmit* first = first_submit;
  LDKRendererMeshResource* mesh =
      s_renderer_mesh_get_resource(renderer, first->mesh);
  LDKRendererMaterialResource* material =
      s_renderer_material_get_resource(renderer, first->material);
  if (mesh == NULL || mesh->index_count == 0 || material == NULL)
  {
    return;
  }

  bool overlay =
      (first->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0;
  bool textured = s_renderer_mesh_pass_material_is_textured(material);
  bool lit = !overlay && s_renderer_mesh_pass_material_is_lit(material);

  LDKRHIBindings bindings = pass->bindings;
  if (textured || lit)
  {
    LDKRHITexture albedo_texture = LDK_RHI_INVALID_RESOURCE;
    LDKRHISampler albedo_sampler = LDK_RHI_INVALID_RESOURCE;
    LDKRHITexture normal_texture = LDK_RHI_INVALID_RESOURCE;
    LDKRHISampler normal_sampler = LDK_RHI_INVALID_RESOURCE;
    LDKRHITexture specular_texture = LDK_RHI_INVALID_RESOURCE;
    LDKRHISampler specular_sampler = LDK_RHI_INVALID_RESOURCE;

    if (textured)
    {
      LDKRendererTextureResource* texture =
          s_renderer_texture_get_resource(renderer, material->desc.texture);
      if (texture == NULL)
      {
        return;
      }
      albedo_texture = texture->texture;
      albedo_sampler = texture->sampler;
    }

    if (lit)
    {
      normal_texture = pass->flat_normal_texture;
      normal_sampler = pass->fallback_sampler;
      specular_texture = pass->white_specular_texture;
      specular_sampler = pass->fallback_sampler;

      if (material->desc.normal_map.id != LDK_RHI_INVALID_RESOURCE)
      {
        LDKRendererTextureResource* texture = s_renderer_texture_get_resource(
            renderer, material->desc.normal_map);
        if (texture != NULL)
        {
          normal_texture = texture->texture;
          normal_sampler = texture->sampler;
        }
      }
      if (material->desc.specular_map.id != LDK_RHI_INVALID_RESOURCE)
      {
        LDKRendererTextureResource* texture = s_renderer_texture_get_resource(
            renderer, material->desc.specular_map);
        if (texture != NULL)
        {
          specular_texture = texture->texture;
          specular_sampler = texture->sampler;
        }
      }
    }

    bindings = s_renderer_mesh_pass_get_material_bindings(pass,
        albedo_texture, albedo_sampler, normal_texture, normal_sampler,
        specular_texture, specular_sampler);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      return;
    }
  }

  LDKRendererMeshMaterialParams material_params = {0};
  material_params.color =
      ldk_renderer_color_from_rgba32(material->desc.color);
  material_params.alpha_cutoff = material->desc.alpha_cutoff;
  if (lit)
  {
    material_params.specular = material->desc.specular;
    material_params.shininess = material->desc.shininess;
    material_params.emission = material->desc.emission;
  }
  ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
      sizeof(material_params), &material_params);

  if (!overlay && submit_count > 1 &&
      s_renderer_mesh_pass_ensure_instance_capacity(pass, submit_count))
  {
    bool valid_run = true;
    for (u32 i = 0; i < submit_count; ++i)
    {
      LDKRendererMeshSubmit* submit = s_renderer_mesh_pass_run_submit(
          renderer, first, sort_items, i);
      if (submit == NULL)
      {
        valid_run = false;
        break;
      }
      pass->instance_worlds[i] = submit->world;
    }

    u32 byte_count = submit_count * (u32)sizeof(Mat4);
    if (valid_run &&
        ldk_rhi_buffer_update(pass->rhi, pass->instance_buffer, 0, byte_count,
            pass->instance_worlds))
    {
      LDKRHIPipeline pipeline = s_renderer_mesh_pass_pipeline(
          pass, material->selection, first->flags, true);
      if (pipeline != LDK_RHI_INVALID_RESOURCE)
      {
        ldk_rhi_pipeline_bind(pass->rhi, pipeline);
        ldk_rhi_bindings_bind(pass->rhi, bindings);
        ldk_rhi_vertex_buffer_bind_at(
            pass->rhi, 0, mesh->vertex_buffer, 0);
        ldk_rhi_vertex_buffer_bind_at(
            pass->rhi, 1, pass->instance_buffer, 0);
        ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
            LDK_RHI_INDEX_TYPE_UINT32);

        LDKRHIDrawIndexedInstancedDesc draw = {0};
        draw.index_count = first->index_count;
        draw.instance_count = submit_count;
        draw.first_index = first->first_index;
        draw.vertex_offset = 0;
        draw.first_instance = 0;
        ldk_rhi_draw_indexed_instanced(pass->rhi, &draw);
        stats->draw_call_count += 1;
        stats->opaque_mesh_draw_call_count += 1;
        stats->instanced_batch_count += 1;
        stats->instanced_instance_count += submit_count;
        return;
      }
    }
  }

  LDKRHIPipeline pipeline = s_renderer_mesh_pass_pipeline(
      pass, material->selection, first->flags, false);
  if (pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  ldk_rhi_pipeline_bind(pass->rhi, pipeline);
  ldk_rhi_bindings_bind(pass->rhi, bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
      LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < submit_count; ++i)
  {
    LDKRendererMeshSubmit* submit =
        s_renderer_mesh_pass_run_submit(renderer, first, sort_items, i);
    if (submit == NULL)
    {
      return;
    }

    LDKRendererMeshObjectParams object = {0};
    object.world = submit->world;
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);

    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = first->index_count;
    draw.first_index = first->first_index;
    draw.vertex_offset = 0;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    if (overlay)
    {
      stats->overlay_mesh_draw_call_count += 1;
    }
    else
    {
      stats->opaque_mesh_draw_call_count += 1;
    }
  }
}

static void s_renderer_mesh_pass_draw_unsorted_submissions(
    LDKRenderer* renderer, LDKRendererMeshPass* pass,
    LDKRendererView const* view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[i];
    if ((submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != flags ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    if (flags == LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY)
    {
      stats->overlay_mesh_render_count += 1;
    }
    else
    {
      stats->opaque_mesh_render_count += 1;
      stats->batch_count += 1;
      if (stats->max_batch_size < 1)
      {
        stats->max_batch_size = 1;
      }
    }

    s_renderer_mesh_pass_draw_run(
        renderer, pass, stats, submit, NULL, 1);
  }
}

static void s_renderer_mesh_pass_draw_submissions(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererView const* view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if (flags == LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY)
  {
    s_renderer_mesh_pass_draw_unsorted_submissions(
        renderer, pass, view, flags);
    return;
  }

  u64* sort_items = NULL;
  u32 sort_item_count = 0;
  if (!s_renderer_mesh_sort_queue_build(
          renderer, view, &sort_items, &sort_item_count))
  {
    s_renderer_mesh_pass_draw_unsorted_submissions(
        renderer, pass, view, flags);
    return;
  }

  stats->opaque_mesh_render_count += sort_item_count;

  u32 i = 0;
  while (i < sort_item_count)
  {
    u32 submit_index = s_renderer_mesh_sort_item_submit_index(sort_items[i]);
    if (submit_index >= renderer->submitted_mesh_count)
    {
      ++i;
      continue;
    }

    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[submit_index];
    u32 end = i + 1;
    while (end < sort_item_count)
    {
      u32 next_index =
          s_renderer_mesh_sort_item_submit_index(sort_items[end]);
      if (next_index >= renderer->submitted_mesh_count ||
          !s_renderer_mesh_submit_same_batch(
              submit, &renderer->submitted_meshes[next_index]))
      {
        break;
      }
      ++end;
    }

    u32 batch_size = end - i;
    stats->batch_count += 1;
    if (batch_size > stats->max_batch_size)
    {
      stats->max_batch_size = batch_size;
    }

    s_renderer_mesh_pass_draw_run(
        renderer, pass, stats, submit, &sort_items[i], batch_size);
    i = end;
  }
}

static bool s_renderer_line_mesh_create(LDKRenderer *renderer)
{
  if (ldk_renderer_mesh_is_valid(renderer, renderer->line_mesh))
  {
    return true;
  }
  // Radius 1/2, along +Z from 0 to 1. Outward CCW faces and end caps.
  LDKMeshVertex vertices[6] = {0};
  const u32 indices[] = {0, 2, 1, 3, 4, 5,
      0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 2, 0, 3, 2, 3, 5};
  for (u32 i = 0; i < 6; ++i)
  {
    float angle = (float)(i % 3) * (2.0f * STDXM_PI / 3.0f);
    vertices[i].position = vec3_make(0.5f * cosf(angle),
        0.5f * sinf(angle), i < 3 ? 0.0f : 1.0f);
    vertices[i].normal = vec3_make(cosf(angle), sinf(angle), 0.0f);
    vertices[i].color = 0xffffffffu;
  }
  LDKRendererMeshDesc desc = {0};
  desc.vertices = vertices;
  desc.vertex_count = 6;
  desc.indices = indices;
  desc.index_count = 24;
  renderer->line_mesh = ldk_renderer_mesh_create(renderer, &desc);
  return ldk_renderer_mesh_is_valid(renderer, renderer->line_mesh);
}

bool ldk_renderer_draw_line(LDKRenderer *renderer, LDKRendererViewId view_id,
    Vec3 start, Vec3 end, float thickness, u32 color, bool depth_test)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      !isfinite(thickness) || thickness <= 0.0f ||
      !isfinite(start.x) || !isfinite(start.y) || !isfinite(start.z) ||
      !isfinite(end.x) || !isfinite(end.y) || !isfinite(end.z))
  {
    return false;
  }
  Vec3 delta = vec3_sub(end, start);
  float length = hypotf(hypotf(delta.x, delta.y), delta.z);
  if (!isfinite(length))
  {
    return false;
  }
  if (length == 0.0f)
  {
    return true;
  }
  Vec3 forward = vec3_make(delta.x / length, delta.y / length,
      delta.z / length);
  Vec3 reference = fabsf(forward.y) < 0.9f
      ? vec3_make(0, 1, 0) : vec3_make(1, 0, 0);
  Vec3 right = vec3_norm(vec3_cross(reference, forward));
  Vec3 up = vec3_cross(forward, right);
  Mat4 world = mat4_identity();
  for (u32 i = 0; i < 3; ++i)
  {
    const float x[] = {right.x, right.y, right.z};
    const float y[] = {up.x, up.y, up.z};
    const float z[] = {delta.x, delta.y, delta.z};
    world.m[i] = x[i] * thickness;
    world.m[4 + i] = y[i] * thickness;
    world.m[8 + i] = z[i];
  }
  world.m[12] = start.x;
  world.m[13] = start.y;
  world.m[14] = start.z;
  if (renderer->submitted_line_count == renderer->submitted_line_capacity)
  {
    u32 capacity = renderer->submitted_line_capacity == 0
        ? 128 : renderer->submitted_line_capacity * 2;
    size_t size = (size_t)capacity * sizeof(LDKRendererLineSubmit);
    if (capacity <= renderer->submitted_line_capacity ||
        size / sizeof(LDKRendererLineSubmit) != capacity)
    {
      return false;
    }
    LDKRendererLineSubmit *lines = LDK_RENDERER_REALLOC(
        renderer->submitted_lines, size);
    if (!lines)
    {
      return false;
    }
    renderer->submitted_lines = lines;
    renderer->submitted_line_capacity = capacity;
  }
  if (!s_renderer_line_mesh_create(renderer))
  {
    return false;
  }
  LDKRendererLineSubmit *line =
      &renderer->submitted_lines[renderer->submitted_line_count++];
  line->world = world;
  line->view_id = view_id;
  line->color = color;
  line->depth_test = depth_test;
  return true;
}

static void s_renderer_lines_draw(LDKRenderer *renderer,
    LDKRendererMeshPass *pass, const LDKRendererView *view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if (!renderer->submitted_line_count)
  {
    return;
  }
  LDKRendererMeshResource *mesh =
      s_renderer_mesh_get_resource(renderer, renderer->line_mesh);
  if (!mesh)
  {
    return;
  }
  bool overlay = flags == LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY;
  bool bound = false;
  for (u32 i = 0; i < renderer->submitted_line_count; ++i)
  {
    const LDKRendererLineSubmit *line = &renderer->submitted_lines[i];
    if (line->depth_test == overlay ||
        (line->view_id != LDK_RENDERER_VIEW_ALL && line->view_id != view->id))
    {
      continue;
    }
    if (!bound)
    {
      ldk_rhi_pipeline_bind(pass->rhi, overlay
          ? pass->overlay_pipeline : pass->vertex_color_unlit_pipeline);
      ldk_rhi_bindings_bind(pass->rhi, pass->bindings);
      ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
      ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
          LDK_RHI_INDEX_TYPE_UINT32);
      bound = true;
    }
    LDKRendererMeshObjectParams object = {0};
    object.world = line->world;
    LDKRendererMeshMaterialParams material = {0};
    material.color = ldk_renderer_color_from_rgba32(line->color);
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);
    ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
        sizeof(material), &material);
    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = mesh->index_count;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->line_draw_call_count += 1;
  }
}

static void s_renderer_mesh_pass_draw(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererView const* view, u32 flags)
{
  if (renderer == NULL || pass == NULL || !pass->is_initialized ||
      view == NULL || !view->submitted)
  {
    return;
  }

  LDKRendererMeshCameraParams camera_params = {0};
  Mat4 camera_world = mat4_inverse_affine(view->view);
  Vec3 camera_position =
      mat4_mul_point(camera_world, vec3_make(0.0f, 0.0f, 0.0f));
  camera_params.view = view->view;
  camera_params.projection = view->projection;
  camera_params.camera_position[0] = camera_position.x;
  camera_params.camera_position[1] = camera_position.y;
  camera_params.camera_position[2] = camera_position.z;
  camera_params.camera_position[3] = 1.0f;
  ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0,
      sizeof(camera_params), &camera_params);

  s_renderer_mesh_pass_draw_submissions(renderer, pass, view, flags);
  s_renderer_lines_draw(renderer, pass, view, flags);
}
// ---------------------------------------------------------------------------
// Internal pass: Procedural grid
// ---------------------------------------------------------------------------

#define LDK_RENDERER_GRID_VERTEX_COUNT 6u

typedef struct LDKRendererGridVertex
{
  float x;
  float y;
  float z;
} LDKRendererGridVertex;

typedef struct LDKRendererGridParams
{
  Mat4 view;
  Mat4 projection;
  float center_extent[4];
  float settings[4];
} LDKRendererGridParams;

static bool s_renderer_grid_pass_create_shaders(LDKRendererGridPass* pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_GRID_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_GRID_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_grid_pass_create_bindings_layout(
    LDKRendererGridPass* pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 1;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;

  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_pipeline(LDKRendererGridPass* pass)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);
  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;
  desc.bindings_layout = pass->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = false;
  desc.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.raster_state.scissor_enabled = false;
  desc.vertex_layout.stride = sizeof(LDKRendererGridVertex);
  desc.vertex_layout.attribute_count = 1;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset = 0;
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  pass->pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  return pass->pipeline != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_buffers(LDKRendererGridPass* pass)
{
  static const LDKRendererGridVertex vertices[LDK_RENDERER_GRID_VERTEX_COUNT] = {
      {-1.0f, 0.0f, -1.0f},
      {-1.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, 1.0f},
      {-1.0f, 0.0f, -1.0f},
      {1.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, -1.0f},
  };
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = sizeof(vertices);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = vertices;

  pass->vertex_buffer = ldk_rhi_buffer_create(pass->rhi, &vertex_desc);
  if (pass->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererGridParams);
  params_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->params_buffer = ldk_rhi_buffer_create(pass->rhi, &params_desc);
  return pass->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_bindings(LDKRendererGridPass* pass)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 1;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererGridParams);

  pass->bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  return pass->bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_initialize(
    LDKRendererGridPass* pass, LDKRendererConfig const* config)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  if (!s_renderer_grid_pass_create_shaders(pass) ||
      !s_renderer_grid_pass_create_bindings_layout(pass) ||
      !s_renderer_grid_pass_create_pipeline(pass) ||
      !s_renderer_grid_pass_create_buffers(pass) ||
      !s_renderer_grid_pass_create_bindings(pass))
  {
    s_renderer_grid_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_grid_pass_terminate(LDKRendererGridPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  memset(pass, 0, sizeof(*pass));
}

static void s_renderer_grid_pass_draw(LDKRenderer* renderer,
    LDKRendererGridPass* pass, LDKRendererView const* view)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if (pass == NULL || !pass->is_initialized || view == NULL ||
      !view->submitted || !view->grid_submitted)
  {
    return;
  }

  LDKRendererGridParams params = {0};
  params.view = view->view;
  params.projection = view->projection;
  params.center_extent[0] = view->grid_center.x;
  params.center_extent[1] = view->grid_center.y;
  params.center_extent[2] = view->grid_center.z;
  params.center_extent[3] = view->grid_extent;
  params.settings[0] = view->grid_spacing;
  ldk_rhi_buffer_update(pass->rhi, pass->params_buffer, 0,
      sizeof(params), &params);

  ldk_rhi_pipeline_bind(pass->rhi, pass->pipeline);
  ldk_rhi_bindings_bind(pass->rhi, pass->bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, pass->vertex_buffer, 0);

  LDKRHIDrawDesc draw_desc = {0};
  draw_desc.vertex_count = LDK_RENDERER_GRID_VERTEX_COUNT;
  draw_desc.first_vertex = 0;
  ldk_rhi_draw(pass->rhi, &draw_desc);
  stats->draw_call_count += 1;
  stats->grid_draw_call_count += 1;
}

static bool s_renderer_view_pass(LDKRenderer* renderer,
    LDKRendererView* view, LDKRendererFrameDesc const* frame_desc)
{
  if (renderer == NULL || view == NULL || !view->submitted ||
      frame_desc == NULL || renderer->game_width == 0 ||
      renderer->game_height == 0)
  {
    return false;
  }

  if (!s_renderer_target_ensure(renderer, &view->target,
      (i32)renderer->game_width, (i32)renderer->game_height))
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
      ldk_renderer_color_from_rgba32(frame_desc->clear_color);
  pass_desc.depth_attachment.valid = true;
  pass_desc.depth_attachment.texture = view->target.depth_texture;
  pass_desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.depth_attachment.depth_store_op = LDK_RHI_STORE_OP_DONT_CARE;
  pass_desc.depth_attachment.clear_depth = 1.0f;
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)renderer->game_width;
  pass_desc.viewport.height = (float)renderer->game_height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  s_renderer_mesh_pass_draw(renderer, &renderer->mesh_pass, view,
      LDK_RENDERER_MESH_SUBMIT_FLAG_NONE);
  s_renderer_grid_pass_draw(renderer, &renderer->grid_pass, view);
  if (view->separate_overlay)
  {
    ldk_rhi_pass_end(renderer->rhi);
    pass_desc.color_attachments[0].texture = view->overlay_target.color_texture;
    pass_desc.color_attachments[0].clear_color =
        ldk_renderer_color_from_rgba32(0x00000000u);
    pass_desc.depth_attachment.texture = view->overlay_target.depth_texture;
    ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  }
  s_renderer_mesh_pass_draw(renderer, &renderer->mesh_pass, view,
      LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY);
  ldk_rhi_pass_end(renderer->rhi);
  return true;
}
// ---------------------------------------------------------------------------
// Internal pass: UI / present
// ---------------------------------------------------------------------------

static bool s_renderer_ui_pass_create_shaders(LDKRendererUIPass* renderer)
{
  renderer->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (renderer->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  renderer->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (renderer->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->vertex_shader_module);
    renderer->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_ui_pass_bindings_create_layout(
    LDKRendererUIPass* renderer)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 2;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;

  renderer->bindings_layout =
      ldk_rhi_bindings_layout_create(renderer->rhi, &desc);
  return renderer->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_create_pipeline(LDKRendererUIPass* renderer)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);
  desc.vertex_shader_module = renderer->vertex_shader_module;
  desc.fragment_shader_module = renderer->fragment_shader_module;
  desc.bindings_layout = renderer->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.raster_state.scissor_enabled = true;
  desc.vertex_layout.stride = sizeof(LDKUIVertex);
  desc.vertex_layout.attribute_count = 3;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[0].offset = (u32)offsetof(LDKUIVertex, x);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[1].offset = (u32)offsetof(LDKUIVertex, u);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[2].offset = (u32)offsetof(LDKUIVertex, color);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_INVALID;

  renderer->pipeline = ldk_rhi_pipeline_create(renderer->rhi, &desc);
  return renderer->pipeline != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_create_white_texture(LDKRendererUIPass* renderer)
{
  u32 pixel = 0xffffffffu;
  LDKRHITextureDesc texture_desc = {0};
  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  texture_desc.width = 1;
  texture_desc.height = 1;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = &pixel;
  texture_desc.initial_data_size = sizeof(pixel);

  renderer->white_texture =
      ldk_rhi_texture_create(renderer->rhi, &texture_desc);
  return renderer->white_texture != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_create_sampler(LDKRendererUIPass* renderer)
{
  LDKRHISamplerDesc desc = {0};
  ldk_rhi_sampler_desc_defaults(&desc);

  renderer->sampler = ldk_rhi_sampler_create(renderer->rhi, &desc);
  return renderer->sampler != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_buffer_creates(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  vertex_desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (renderer->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = renderer->index_capacity * (u32)sizeof(u32);
  index_desc.usage =
      LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (renderer->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererUIParams);
  params_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->params_buffer = ldk_rhi_buffer_create(renderer->rhi, &params_desc);
  return renderer->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_recreate_vertex_buffer(
    LDKRendererUIPass* renderer)
{
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
  renderer->vertex_buffer = LDK_RHI_INVALID_RESOURCE;

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->vertex_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_recreate_index_buffer(
    LDKRendererUIPass* renderer)
{
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
  renderer->index_buffer = LDK_RHI_INVALID_RESOURCE;

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->index_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_ensure_vertex_capacity(
    LDKRendererUIPass* renderer, u32 vertex_count)
{
  if (vertex_count <= renderer->vertex_capacity)
  {
    return true;
  }

  while (renderer->vertex_capacity < vertex_count)
  {
    renderer->vertex_capacity *= 2;
  }

  return s_renderer_ui_pass_recreate_vertex_buffer(renderer);
}

static bool s_renderer_ui_pass_ensure_index_capacity(
    LDKRendererUIPass* renderer, u32 index_count)
{
  if (index_count <= renderer->index_capacity)
  {
    return true;
  }

  while (renderer->index_capacity < index_count)
  {
    renderer->index_capacity *= 2;
  }

  return s_renderer_ui_pass_recreate_index_buffer(renderer);
}

static bool s_renderer_ui_pass_initialize(
    LDKRendererUIPass* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;
  renderer->vertex_capacity = config->initial_ui_vertex_capacity > 0
      ? config->initial_ui_vertex_capacity : 1024;
  renderer->index_capacity = config->initial_ui_index_capacity > 0
      ? config->initial_ui_index_capacity : 2048;

  if (!s_renderer_ui_pass_create_shaders(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_bindings_create_layout(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_pipeline(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_white_texture(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_sampler(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_buffer_creates(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;
  return true;
}

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
    {
      ldk_rhi_bindings_destroy(
          renderer->rhi, renderer->bindings_cache[i].bindings);
    }

    ldk_rhi_buffer_destroy(renderer->rhi, renderer->params_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
    ldk_rhi_sampler_destroy(renderer->rhi, renderer->sampler);
    ldk_rhi_texture_destroy(renderer->rhi, renderer->white_texture);
    ldk_rhi_pipeline_destroy(renderer->rhi, renderer->pipeline);
    ldk_rhi_bindings_layout_destroy(renderer->rhi, renderer->bindings_layout);
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->vertex_shader_module);
  }

  LDK_RENDERER_FREE(renderer->bindings_cache);
  memset(renderer, 0, sizeof(*renderer));
}

static LDKRHIBindings s_renderer_ui_pass_create_draw_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = renderer->bindings_layout;
  desc.binding_count = 2;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = renderer->params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererUIParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].texture = texture;
  desc.bindings[1].sampler = sampler;
  return ldk_rhi_bindings_create(renderer->rhi, &desc);
}

static LDKRHIBindings s_renderer_ui_pass_find_cached_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
{
  for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
  {
    if (renderer->bindings_cache[i].texture == texture &&
        renderer->bindings_cache[i].sampler == sampler)
    {
      return renderer->bindings_cache[i].bindings;
    }
  }

  return LDK_RHI_INVALID_RESOURCE;
}

static void s_renderer_ui_pass_remove_texture_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture)
{
  if (renderer == NULL || renderer->rhi == NULL ||
      texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < renderer->bindings_cache_count)
  {
    LDKRendererBindingsCacheEntry* entry = &renderer->bindings_cache[index];
    if (entry->texture != texture)
    {
      index += 1;
      continue;
    }

    ldk_rhi_bindings_destroy(renderer->rhi, entry->bindings);
    renderer->bindings_cache_count -= 1;

    if (index != renderer->bindings_cache_count)
    {
      *entry = renderer->bindings_cache[renderer->bindings_cache_count];
    }

    memset(&renderer->bindings_cache[renderer->bindings_cache_count],
        0, sizeof(*entry));
  }
}

static bool s_renderer_ui_pass_grow_bindings_cache(
    LDKRendererUIPass* renderer)
{
  u32 new_capacity = renderer->bindings_cache_capacity == 0
      ? 16 : renderer->bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererBindingsCacheEntry);
  LDKRendererBindingsCacheEntry* new_cache = renderer->bindings_cache == NULL
      ? (LDKRendererBindingsCacheEntry*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererBindingsCacheEntry*)LDK_RENDERER_REALLOC(
            renderer->bindings_cache, new_size);

  if (new_cache == NULL)
  {
    return false;
  }

  renderer->bindings_cache = new_cache;
  renderer->bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_ui_pass_get_draw_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
{
  LDKRHIBindings cached_bindings =
      s_renderer_ui_pass_find_cached_bindings(renderer, texture, sampler);
  if (cached_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    return cached_bindings;
  }

  if (renderer->bindings_cache_count == renderer->bindings_cache_capacity)
  {
    if (!s_renderer_ui_pass_grow_bindings_cache(renderer))
    {
      return LDK_RHI_INVALID_RESOURCE;
    }
  }

  LDKRHIBindings bindings =
      s_renderer_ui_pass_create_draw_bindings(renderer, texture, sampler);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererBindingsCacheEntry* entry =
      &renderer->bindings_cache[renderer->bindings_cache_count];
  entry->texture = texture;
  entry->sampler = sampler;
  entry->bindings = bindings;
  renderer->bindings_cache_count += 1;
  return bindings;
}

static void s_renderer_ui_pass(LDKRenderer* owner,
    LDKRendererUIPass* renderer, LDKRendererFrameDomainStats* stats,
    LDKUIRenderData const* render_data,
    const LDKRendererFrameDesc* frame_desc, bool present)
{
  if (renderer == NULL || !renderer->is_initialized || render_data == NULL ||
      frame_desc == NULL)
  {
    return;
  }

  i32 framebuffer_width = frame_desc->framebuffer_width;
  i32 framebuffer_height = frame_desc->framebuffer_height;

  if (framebuffer_width <= 0 || framebuffer_height <= 0)
  {
    return;
  }

  if (render_data->vertex_count == 0 || render_data->index_count == 0 ||
      render_data->command_count == 0)
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_vertex_capacity(
          renderer, render_data->vertex_count))
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_index_capacity(
          renderer, render_data->index_count))
  {
    return;
  }

  LDKRHIPassDesc pass_desc;
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = LDK_RHI_INVALID_RESOURCE;
  pass_desc.color_attachments[0].load_op = frame_desc->clear_color_enabled
      ? LDK_RHI_LOAD_OP_CLEAR : LDK_RHI_LOAD_OP_LOAD;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.color_attachments[0].clear_color =
      ldk_renderer_color_from_rgba32(frame_desc->clear_color);
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)framebuffer_width;
  pass_desc.viewport.height = (float)framebuffer_height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);

  LDKRendererUIParams params = {0};
  params.viewport_size[0] = (float)framebuffer_width;
  params.viewport_size[1] = (float)framebuffer_height;

  ldk_rhi_buffer_update(renderer->rhi, renderer->params_buffer, 0,
      sizeof(params), &params);
  ldk_rhi_buffer_update(renderer->rhi, renderer->vertex_buffer, 0,
      render_data->vertex_count * (u32)sizeof(LDKUIVertex),
      render_data->vertices);
  ldk_rhi_buffer_update(renderer->rhi, renderer->index_buffer, 0,
      render_data->index_count * (u32)sizeof(u32), render_data->indices);

  ldk_rhi_pipeline_bind(renderer->rhi, renderer->pipeline);
  ldk_rhi_vertex_buffer_bind(renderer->rhi, renderer->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(renderer->rhi, renderer->index_buffer, 0,
      LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < render_data->command_count; i++)
  {
    const LDKUIDrawCmd* cmd = &render_data->commands[i];
    LDKRHITexture texture = (LDKRHITexture)cmd->texture;
    if (texture == LDK_RHI_INVALID_RESOURCE)
    {
      texture = renderer->white_texture;
    }

    LDKRHISampler sampler =
        s_renderer_texture_sampler_from_rhi_texture(owner, texture);
    if (sampler == LDK_RHI_INVALID_RESOURCE)
    {
      sampler = renderer->sampler;
    }

    LDKRHIBindings bindings =
        s_renderer_ui_pass_get_draw_bindings(renderer, texture, sampler);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }

    LDKRHIRect scissor = {0};
    float x0 = floorf(cmd->clip_rect.x);
    float y0 = floorf(cmd->clip_rect.y);
    float x1 = ceilf(cmd->clip_rect.x + cmd->clip_rect.w);
    float y1 = ceilf(cmd->clip_rect.y + cmd->clip_rect.h);

    scissor.x = (i32)x0;
    scissor.y = (i32)y0;
    scissor.width = (i32)(x1 - x0);
    scissor.height = (i32)(y1 - y0);

    ldk_rhi_scissor_set(renderer->rhi, &scissor);

    if (scissor.width <= 0 || scissor.height <= 0)
    {
      continue;
    }

    ldk_rhi_bindings_bind(renderer->rhi, bindings);
    ldk_rhi_scissor_set(renderer->rhi, &scissor);

    LDKRHIDrawIndexedDesc draw_desc = {0};
    draw_desc.index_count = cmd->index_count;
    draw_desc.first_index = cmd->index_offset;
    draw_desc.vertex_offset = 0;
    ldk_rhi_draw_indexed(renderer->rhi, &draw_desc);
    stats->draw_call_count += 1;
    if (present)
    {
      stats->present_draw_call_count += 1;
    }
    else
    {
      stats->ui_draw_call_count += 1;
    }
  }

  ldk_rhi_pass_end(renderer->rhi);
}

static void s_renderer_present_view_pass(LDKRenderer* renderer,
    LDKRendererView const* view, const LDKRendererFrameDesc* frame_desc)
{
  if (renderer == NULL || view == NULL || frame_desc == NULL)
  {
    return;
  }

  if (view->target.color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  i32 width = frame_desc->framebuffer_width;
  i32 height = frame_desc->framebuffer_height;

  if (width <= 0 || height <= 0)
  {
    return;
  }

  float game_aspect = (float)renderer->game_width / (float)renderer->game_height;
  float framebuffer_aspect = (float)width / (float)height;
  float present_width = (float)width;
  float present_height = (float)height;

  if (framebuffer_aspect > game_aspect)
  {
    present_width = present_height * game_aspect;
  }
  else
  {
    present_height = present_width / game_aspect;
  }

  float present_x = ((float)width - present_width) * 0.5f;
  float present_y = ((float)height - present_height) * 0.5f;

  LDKUIVertex vertices[4] =
  {
    {present_x,                 present_y,                  0.0f, 0.0f, 0xffffffffu},
    {present_x + present_width, present_y,                  1.0f, 0.0f, 0xffffffffu},
    {present_x + present_width, present_y + present_height, 1.0f, 1.0f, 0xffffffffu},
    {present_x,                 present_y + present_height, 0.0f, 1.0f, 0xffffffffu},
  };

  u32 indices[6] =
  {
    0, 1, 2,
    0, 2, 3,
  };

  LDKUIDrawCmd command = {0};
  command.texture = (LDKUITextureHandle)view->target.color_texture;
  command.clip_rect.x = 0.0f;
  command.clip_rect.y = 0.0f;
  command.clip_rect.w = (float)width;
  command.clip_rect.h = (float)height;
  command.index_offset = 0;
  command.index_count = 6;

  LDKUIRenderData render_data = {0};
  render_data.vertices = vertices;
  render_data.vertex_count = 4;
  render_data.indices = indices;
  render_data.index_count = 6;
  render_data.commands = &command;
  render_data.command_count = 1;

  LDKRendererFrameDesc present_desc = *frame_desc;
  present_desc.clear_color = 0x000000FFu;
  present_desc.clear_color_enabled = true;

  s_renderer_ui_pass(renderer, &renderer->ui_pass,
      &renderer->current_frame_stats.game, &render_data, &present_desc, true);
}

// ---------------------------------------------------------------------------
// Internal renderer resources: Mesh
// ---------------------------------------------------------------------------
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

LDKResourceTexture ldk_renderer_image_acquire(LDKRenderer* renderer,
    LDKAssetManager* assets, LDKAssetImage image)
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
        entry->image_asset.h.version == image.h.version)
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
  LDKResourceTexture result =
      ldk_renderer_texture_create_from_image(renderer, data->image, NULL);
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
      ldk_renderer_image_acquire(renderer, assets, image);
  if (!ldk_renderer_texture_is_valid(renderer, texture))
  {
    ldk_log_error(
        "Material %s upload failed; using neutral fallback.\n", label);
    return ldk_renderer_texture_null();
  }
  return texture;
}

bool ldk_renderer_material_resolve(LDKRenderer *renderer,
    LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
    LDKResourceMaterial *renderer_material,
    LDKResourceTexture *renderer_texture,
    LDKResourceTexture *renderer_normal_map,
    LDKResourceTexture *renderer_specular_map)
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
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  desc.alpha_cutoff = 0.5f;
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
    desc.alpha_mode = material_desc->args.textured.alpha_mode;
    desc.alpha_cutoff = material_desc->args.textured.alpha_cutoff;
    desc.texture = ldk_renderer_image_acquire(
        renderer, assets, material_desc->args.textured.texture);
    if (!ldk_renderer_texture_is_valid(renderer, desc.texture))
    {
      ldk_log_error("Material image unavailable; using missing texture.\n");
      LDKAssetImage fallback = ldk_asset_manager_image_missing(assets, NULL);
      desc.texture = ldk_renderer_image_acquire(renderer, assets, fallback);
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
          renderer, assets, material_desc->surface.normal_map, "normal map");
      specular_map = s_renderer_material_map_acquire(renderer, assets,
          material_desc->surface.specular_map, "specular map");
    }
    desc.normal_map = normal_map;
    desc.specular_map = specular_map;
    desc.specular = material_desc->surface.specular;
    desc.shininess = material_desc->surface.shininess;
    desc.emission = material_desc->surface.emission;
  }

  material = ldk_renderer_material_create(renderer, &desc);
  if (!ldk_renderer_material_is_valid(renderer, material))
  {
    ldk_renderer_image_release(renderer, desc.texture);
    ldk_renderer_image_release(renderer, normal_map);
    ldk_renderer_image_release(renderer, specular_map);
    return false;
  }

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
  switch (desc->type)
  {
    case LDK_MATERIAL_TYPE_TEXTURED_UNLIT:
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT
          ? LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT
          : LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT;
    case LDK_MATERIAL_TYPE_TEXTURED:
      return desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT
          ? LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT
          : LDK_RENDERER_MATERIAL_SELECTION_TEXTURED;
    case LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT:
      return LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT;
    case LDK_MATERIAL_TYPE_VERTEX_COLOR:
      return LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR;
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
  if (desc == NULL || !ldk_material_type_is_valid(desc->type))
  {
    return false;
  }

  if (s_renderer_material_type_is_textured(desc->type))
  {
    if (!ldk_renderer_texture_is_valid(renderer, desc->texture) ||
        (desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_OPAQUE &&
            desc->alpha_mode != LDK_MATERIAL_ALPHA_MODE_CUTOUT) ||
        (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT &&
            (!isfinite(desc->alpha_cutoff) || desc->alpha_cutoff < 0.0f ||
                desc->alpha_cutoff > 1.0f)))
    {
      return false;
    }
  }

  if (!s_renderer_material_type_is_lit(desc->type))
  {
    return true;
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
         isfinite(desc->emission) && desc->emission >= 0.0f;
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
  resource->desc.alpha_mode = s_renderer_material_type_is_textured(desc->type)
      ? desc->alpha_mode
      : LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  resource->desc.alpha_cutoff =
      s_renderer_material_type_is_textured(desc->type)
      ? desc->alpha_cutoff
      : 0.5f;
  resource->desc.normal_map =
      lit ? desc->normal_map : ldk_renderer_texture_null();
  resource->desc.specular_map =
      lit ? desc->specular_map : ldk_renderer_texture_null();
  resource->desc.specular = lit ? desc->specular : 0.0f;
  resource->desc.shininess =
      lit && desc->shininess != 0.0f ? desc->shininess : 32.0f;
  resource->desc.emission = lit ? desc->emission : 0.0f;
  resource->selection = s_renderer_material_selection(&resource->desc);
  resource->render_key = s_renderer_material_render_key(resource->selection);
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
    ldk_renderer_material_destroy(renderer, material);
  }

  LDK_RENDERER_FREE(renderer->materials);
  renderer->materials = NULL;
  renderer->material_count = 0;
  renderer->material_capacity = 0;
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

LDKUITextureHandle ldk_renderer_get_font_page_texture(
    LDKRenderer* renderer, LDKFontInstance* font, u32 page_index)
{
  if (renderer == NULL || renderer->rhi == NULL || font == NULL)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  for (u32 i = 0; i < renderer->font_page_count; i++)
  {
    LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[i];
    if (entry->font == font && entry->page_index == page_index)
    {
      return (LDKUITextureHandle)entry->texture;
    }
  }

  LDKFontPageInfo page = {0};
  if (!ldk_ttf_get_page_info(font, page_index, &page))
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  if (page.pixels == NULL || page.width == 0 || page.height == 0)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHITextureDesc texture_desc = {0};
  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_R8_UNORM;
  texture_desc.width = page.width;
  texture_desc.height = page.height;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = page.pixels;
  texture_desc.initial_data_size = page.width * page.height;
  texture_desc.swizzle_r = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_g = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_b = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_a = LDK_RHI_TEXTURE_SWIZZLE_R;

  LDKRHITexture texture = ldk_rhi_texture_create(renderer->rhi, &texture_desc);
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

  LDKRendererFontPageCacheEntry* entry =
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
  renderer->ambient_light.color = 0xffffffffu;
  renderer->ambient_light.intensity = 0.0f;

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
    s_renderer_target_destroy(renderer, &renderer->views[i].target);
    s_renderer_target_destroy(renderer, &renderer->views[i].overlay_target);
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
  s_renderer_destroy_material_resources(renderer);
  s_renderer_destroy_texture_resources(renderer);
  s_renderer_destroy_mesh_resources(renderer);
  s_renderer_ui_pass_terminate(&renderer->ui_pass);
  s_renderer_grid_pass_terminate(&renderer->grid_pass);
  s_renderer_mesh_pass_terminate(&renderer->mesh_pass);
  s_renderer_shadow_pass_terminate(&renderer->shadow_pass);
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

  memset(&renderer->current_frame_stats, 0,
      sizeof(renderer->current_frame_stats));
  renderer->current_frame_stats.mesh_submit_count =
      renderer->submitted_mesh_count;

  ldk_rhi_frame_begin(renderer->rhi);

  LDKRendererView* game_view =
      s_renderer_view_find(renderer, renderer->game_view);
  bool rendered_game = false;

  for (u32 i = 0; i < renderer->view_count; i++)
  {
    LDKRendererView* view = &renderer->views[i];
    bool rendered = s_renderer_view_pass(renderer, view, desc);
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

  renderer->submitted_mesh_count = 0;
  renderer->submitted_line_count = 0;
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

  if (view == NULL || !view->submitted ||
      view->target.color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  return (LDKUITextureHandle)view->target.color_texture;
}

LDKUITextureHandle ldk_renderer_view_overlay_texture_request(
    LDKRenderer* renderer, LDKRendererViewId view_id)
{
  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (!view || !view->submitted ||
      !s_renderer_target_ensure(renderer, &view->overlay_target,
          (i32)renderer->game_width, (i32)renderer->game_height))
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  view->separate_overlay = true;
  return (LDKUITextureHandle)view->overlay_target.color_texture;
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

  view->view = view_matrix;
  view->projection = projection;
  view->submitted = true;
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
                   LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS)) != 0)
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
  renderer->submitted_mesh_count += 1;
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
