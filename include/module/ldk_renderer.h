/**
 * @file ldk_renderer.h
 * @brief LDK engine renderer.
 *
 * API agnostic renderer built on top of ldk_rhi.
 */

#ifndef LDK_RENDERER_H
#define LDK_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_material.h>
#include <ldk_mesh.h>
#include <ldk_resource.h>
#include <ldk_ttf.h>
#include <ldk_image.h>
#include <module/ldk_rhi.h>
#include <module/ldk_ui.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef LDK_RENDERER_ALLOC
#define LDK_RENDERER_ALLOC(size) malloc(size)
#endif

#ifndef LDK_RENDERER_FREE
#define LDK_RENDERER_FREE(ptr) free(ptr)
#endif

#ifndef LDK_RENDERER_REALLOC
#define LDK_RENDERER_REALLOC(ptr, size) realloc(ptr, size)
#endif

  typedef enum LDKShader
  {
    LDK_SHADER_INVALID = 0,
    LDK_SHADER_UI_PASS,
    LDK_SHADER_MESH_PASS,
    LDK_SHADER_PRESENT_PASS,
    LDK_SHADER_MESH_PASS_INSTANCED,
    LDK_SHADER_GRID_PASS,
    LDK_SHADER_MESH_PASS_UNLIT,
    LDK_SHADER_MESH_PASS_TEXTURED,
    LDK_SHADER_MESH_PASS_TEXTURED_UNLIT,
    LDK_SHADER_SHADOW_PASS,
    LDK_SHADER_MESH_PASS_TEXTURED_CUTOUT,
    LDK_SHADER_MESH_PASS_TEXTURED_UNLIT_CUTOUT,
    LDK_SHADER_SHADOW_PASS_CUTOUT
  } LDKShader;

  typedef struct LDKRendererMeshDesc
  {
    const LDKMeshVertex* vertices;
    u32 vertex_count;
    const u32* indices;
    u32 index_count;
    /* True when vertex tangent fields contain authored/initialized data. */
    bool has_tangents;
  } LDKRendererMeshDesc;

  typedef struct LDKRendererMeshResource
  {
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    u32 vertex_count;
    u32 index_count;
    bool alive;
  } LDKRendererMeshResource;

  typedef u64 LDKRendererViewId;

#define LDK_RENDERER_VIEW_INVALID ((LDKRendererViewId)0)
#define LDK_RENDERER_VIEW_ALL ((LDKRendererViewId)UINT64_MAX)

#define LDK_RENDERER_MAX_LIGHTS_PER_VIEW 16

  typedef enum LDKRendererLightType
  {
    LDK_RENDERER_LIGHT_POINT = 0,
    LDK_RENDERER_LIGHT_SPOT,
    LDK_RENDERER_LIGHT_DIRECTIONAL
  } LDKRendererLightType;

  typedef struct LDKRendererLightSubmit
  {
    LDKRendererLightType type;
    LDKRendererViewId view_id;
    Vec3 position;
    Vec3 direction;
    u32 color; // 0xRRGGBBAA; alpha is ignored.
    float intensity;
    float range;
    float inner_angle; // Half-cone angle, radians.
    float outer_angle; // Half-cone angle, radians.
    bool casts_shadows; // Directional only; first eligible light per view.
  } LDKRendererLightSubmit;

  typedef struct LDKRendererAmbientLight
  {
    u32 color; // 0xRRGGBBAA; alpha is ignored.
    float intensity;
  } LDKRendererAmbientLight;

  typedef enum LDKRendererMeshSubmitFlag
  {
    LDK_RENDERER_MESH_SUBMIT_FLAG_NONE = 0,
    LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY = 1 << 0,
    LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS = 1 << 1
  } LDKRendererMeshSubmitFlag;

  typedef struct LDKRendererMeshSubmit
  {
    LDKResourceMesh mesh;
    LDKResourceMaterial material;
    u32 first_index;
    u32 index_count;
    Mat4 world;
    LDKRendererViewId view_id;
    u32 flags;
  } LDKRendererMeshSubmit;

  /* Internal frame data for the shared triangular line prism. */
  typedef struct LDKRendererLineSubmit
  {
    Mat4 world;
    LDKRendererViewId view_id;
    u32 color;
    bool depth_test;
  } LDKRendererLineSubmit;

  typedef struct LDKRendererConfig
  {
    LDKRHIContext* rhi;
    u32 initial_ui_vertex_capacity;
    u32 initial_ui_index_capacity;
    u32 game_width;
    u32 game_height;
    bool present_game;
    // Zero selects the default (2048 texels, 60 world units).
    u32 shadow_map_resolution;
    float shadow_distance;
  } LDKRendererConfig;

  typedef struct LDKRendererFrameDesc
  {
    i32 framebuffer_width;
    i32 framebuffer_height;
    rgba32 clear_color;
    bool clear_color_enabled;
  } LDKRendererFrameDesc;

  typedef struct LDKRendererFrameDomainStats
  {
    u32 rendered_view_count;
    u32 opaque_mesh_render_count;
    u32 overlay_mesh_render_count;

    u32 batch_count;
    u32 instanced_batch_count;
    u32 instanced_instance_count;
    u32 max_batch_size;

    u32 draw_call_count;
    u32 opaque_mesh_draw_call_count;
    u32 overlay_mesh_draw_call_count;
    u32 shadow_draw_call_count;
    u32 line_draw_call_count;
    u32 grid_draw_call_count;
    u32 ui_draw_call_count;
    u32 present_draw_call_count;
  } LDKRendererFrameDomainStats;

  typedef struct LDKRendererFrameStats
  {
    double cpu_time_ms;

    // Submission count is global because a VIEW_ALL submission may be rendered
    // into both game and non-game views. The remaining counters describe actual
    // rendered work and are also available split by destination below.
    u32 rendered_view_count;
    u32 mesh_submit_count;
    u32 opaque_mesh_render_count;
    u32 overlay_mesh_render_count;

    u32 batch_count;
    u32 instanced_batch_count;
    u32 instanced_instance_count;
    u32 max_batch_size;

    u32 draw_call_count;
    u32 opaque_mesh_draw_call_count;
    u32 overlay_mesh_draw_call_count;
    u32 shadow_draw_call_count;
    u32 line_draw_call_count;
    u32 grid_draw_call_count;
    u32 ui_draw_call_count;
    u32 present_draw_call_count;

    LDKRendererFrameDomainStats game;
    LDKRendererFrameDomainStats non_game;
  } LDKRendererFrameStats;

  typedef struct LDKRendererBindingsCacheEntry
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    LDKRHIBindings bindings;
  } LDKRendererBindingsCacheEntry;

  typedef struct LDKRendererMeshBindingsCacheEntry
  {
    LDKRHITexture albedo_texture;
    LDKRHISampler albedo_sampler;
    LDKRHITexture normal_texture;
    LDKRHISampler normal_sampler;
    LDKRHITexture specular_texture;
    LDKRHISampler specular_sampler;
    LDKRHIBindings bindings;
  } LDKRendererMeshBindingsCacheEntry;

  typedef struct LDKRendererUIPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHITexture white_texture;
    LDKRHISampler sampler;
    LDKRendererBindingsCacheEntry* bindings_cache;
    u32 bindings_cache_count;
    u32 bindings_cache_capacity;
    u32 vertex_capacity;
    u32 index_capacity;
    bool is_initialized;
  } LDKRendererUIPass;

  typedef struct LDKRendererShadowPass
  {
    u32 resolution;
    float distance;
    LDKRHIContext *rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIShaderModule cutout_vertex_shader_module;
    LDKRHIShaderModule cutout_fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIPipeline cutout_pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBuffer material_buffer;
    LDKRHIBindings bindings;
    LDKRendererBindingsCacheEntry *cutout_bindings_cache;
    u32 cutout_bindings_cache_count;
    u32 cutout_bindings_cache_capacity;
    LDKRHITexture depth_texture;
    LDKRHISampler sampler;
  } LDKRendererShadowPass;

  typedef struct LDKRendererMeshPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule instanced_vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIShaderModule overlay_fragment_shader_module;
    LDKRHIShaderModule textured_fragment_shader_module;
    LDKRHIShaderModule textured_unlit_fragment_shader_module;
    LDKRHIShaderModule textured_cutout_fragment_shader_module;
    LDKRHIShaderModule textured_unlit_cutout_fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline vertex_color_pipeline;
    LDKRHIPipeline vertex_color_unlit_pipeline;
    LDKRHIPipeline overlay_pipeline;
    LDKRHIPipeline textured_pipeline;
    LDKRHIPipeline textured_unlit_pipeline;
    LDKRHIPipeline textured_overlay_pipeline;
    LDKRHIPipeline textured_cutout_pipeline;
    LDKRHIPipeline textured_unlit_cutout_pipeline;
    LDKRHIPipeline textured_unlit_cutout_overlay_pipeline;
    LDKRHIPipeline vertex_color_instanced_pipeline;
    LDKRHIPipeline vertex_color_unlit_instanced_pipeline;
    LDKRHIPipeline textured_instanced_pipeline;
    LDKRHIPipeline textured_unlit_instanced_pipeline;
    LDKRHIPipeline textured_cutout_instanced_pipeline;
    LDKRHIPipeline textured_unlit_cutout_instanced_pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBuffer material_buffer;
    LDKRHIBuffer lighting_buffer;
    LDKRHIBuffer instance_buffer;
    Mat4* instance_worlds;
    u32 instance_capacity;
    // Borrowed from the renderer-owned shadow pass.
    LDKRHITexture shadow_texture;
    LDKRHISampler shadow_sampler;
    // Renderer-owned neutral resources for optional lit material maps.
    LDKRHITexture flat_normal_texture;
    LDKRHITexture white_specular_texture;
    LDKRHISampler fallback_sampler;
    LDKRHIBindings bindings;
    LDKRendererMeshBindingsCacheEntry *material_bindings_cache;
    u32 material_bindings_cache_count;
    u32 material_bindings_cache_capacity;
    bool is_initialized;
  } LDKRendererMeshPass;

  typedef struct LDKRendererGridPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHIBindings bindings;
    bool is_initialized;
  } LDKRendererGridPass;

  typedef struct LDKRendererFontPageCacheEntry
  {
    LDKFontInstance* font;
    u32 page_index;
    u32 width;
    u32 height;
    LDKRHITexture texture;
  } LDKRendererFontPageCacheEntry;

  typedef struct LDKRendererFontPage
  {
    LDKFontInstance* font;
    u32 page_index;
    u32 width;
    u32 height;
    LDKRHITexture texture;
  } LDKRendererFontPage;

  typedef struct LDKRendererTarget
  {
    LDKRHITexture color_texture;
    LDKRHITexture depth_texture;
    i32 width;
    i32 height;
    LDKRHIFormat color_format;
    LDKRHIFormat depth_format;
  } LDKRendererTarget;

  typedef struct LDKRendererView
  {
    LDKRendererViewId id;
    Mat4 view;
    Mat4 projection;
    LDKRendererTarget target;
    LDKRendererTarget overlay_target;
    bool separate_overlay;
    Vec3 grid_center;
    float grid_extent;
    float grid_spacing;
    bool grid_submitted;
    bool submitted;
  } LDKRendererView;

  typedef enum LDKRendererTextureFlag
  {
    LDK_RENDERER_TEXTURE_FLAG_NONE       = 0,
    LDK_RENDERER_TEXTURE_FLAG_SRGB       = 1 << 0,
    LDK_RENDERER_TEXTURE_FLAG_RENDERABLE = 1 << 1
  } LDKRendererTextureFlag;

  typedef struct LDKRendererTextureOptions
  {
    u32 flags;
    bool generate_mipmaps;
    LDKRHIFilter min_filter;
    LDKRHIFilter mag_filter;
    LDKRHIFilter mip_filter;
    LDKRHIWrap wrap_u;
    LDKRHIWrap wrap_v;
    LDKRHIWrap wrap_w;
  } LDKRendererTextureOptions;

  typedef struct LDKRendererTextureDesc
  {
    u32 width;
    u32 height;
    u32 channel_count;
    u32 flags;
    void const* pixels;
    u64 byte_count;
    LDKRendererTextureOptions const* options;
  } LDKRendererTextureDesc;

  typedef struct LDKRendererTextureResource
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    u32 width;
    u32 height;
    u32 channel_count;
    LDKRHIFormat format;
    u32 flags;
    bool alive;
    struct LDKAssetManager* asset_manager;
    LDKAssetImage image_asset;
    u32 image_references;
  } LDKRendererTextureResource;

  typedef struct LDKRendererMaterialDesc
  {
    LDKMaterialType type;
    LDKResourceTexture texture;
    /* Optional borrowed resources for lit materials. */
    LDKResourceTexture normal_map;
    LDKResourceTexture specular_map;
    rgba32 color;
    LDKMaterialAlphaMode alpha_mode;
    float alpha_cutoff;
    float specular;
    float shininess;
    float emission;
  } LDKRendererMaterialDesc;

  typedef enum LDKRendererMaterialSelection
  {
    LDK_RENDERER_MATERIAL_SELECTION_INVALID = 0,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT
  } LDKRendererMaterialSelection;

  typedef u64 LDKRendererRenderKey;

  typedef struct LDKRendererMaterialResource
  {
    LDKRendererMaterialDesc desc;
    LDKRendererMaterialSelection selection;
    LDKRendererRenderKey render_key;
    bool alive;
  } LDKRendererMaterialResource;

  typedef struct LDKRenderer
  {
    LDKRHIContext* rhi;
    LDKRendererUIPass ui_pass;
    LDKRendererMeshPass mesh_pass;
    LDKRendererShadowPass shadow_pass;
    LDKRendererGridPass grid_pass;
    LDKUIRenderData const* submitted_ui;
    u32 game_width;
    u32 game_height;
    bool present_game;

    // Render views
    LDKRendererView* views;
    u32 view_count;
    u32 view_capacity;
    LDKRendererViewId game_view;

    // Mesh cache
    LDKRendererMeshResource* meshes;
    u32 mesh_count;
    u32 mesh_capacity;

    // Texture cache
    LDKRendererTextureResource* textures;
    u32 texture_count;
    u32 texture_capacity;

    // Material cache
    LDKRendererMaterialResource* materials;
    u32 material_count;
    u32 material_capacity;
    LDKResourceMaterial default_material;

    // Font atlas cache
    LDKRendererFontPageCacheEntry* font_pages;
    u32 font_page_count;
    u32 font_page_capacity;

    LDKRendererAmbientLight ambient_light;
    LDKRendererLightSubmit *submitted_lights;
    u32 submitted_light_count;
    u32 submitted_light_capacity;
    bool light_limit_reported;

    // Shared line geometry and transient submissions, owned by the renderer.
    LDKResourceMesh line_mesh;
    LDKRendererLineSubmit *submitted_lines;
    u32 submitted_line_count;
    u32 submitted_line_capacity;

    // Submitted meshes
    LDKRendererMeshSubmit* submitted_meshes;
    u32 submitted_mesh_count;
    u32 submitted_mesh_capacity;

    // Per-view opaque mesh sort scratch. Sort items pack a 40-bit state key
    // and a 24-bit index into submitted_meshes.
    u64* mesh_sort_items;
    u64* mesh_sort_scratch;
    u32 mesh_sort_capacity;
    u32* mesh_sort_mesh_ids;
    u32 mesh_sort_mesh_id_capacity;
    u32* mesh_sort_material_ids;
    u32 mesh_sort_material_id_capacity;

    // Frame statistics. last_frame_stats always describes the last fully
    // completed renderer frame; current_frame_stats is internal accumulation.
    LDKRendererFrameStats current_frame_stats;
    LDKRendererFrameStats last_frame_stats;

    bool is_initialized;
  } LDKRenderer;

  /** Set constant ambient light applied to lit materials.
   * Color uses 0xRRGGBBAA; alpha is ignored. Intensity must be finite and
   * non-negative. Ambient light does not count toward the per-view light limit.
   */
  LDK_API bool ldk_renderer_ambient_light_set(
      LDKRenderer *renderer, u32 color, float intensity);

  /** Submit an unlit, capped triangular prism from start to end.
   * Thickness is the circumdiameter of its cross-section, in world units.
   * Depth test also enables depth writes; false uses the overlay path.
   * View may be ALL or an ID not yet submitted. Call on the render thread
   * before render_frame; the queue is cleared after that frame is rendered.
   * Zero-length segments succeed without drawing. Invalid/nonfinite data,
   * nonpositive thickness and allocation failures return false.
   */
  LDK_API bool ldk_renderer_draw_line(LDKRenderer *renderer,
      LDKRendererViewId view_id, Vec3 start, Vec3 end, float thickness,
      u32 color, bool depth_test);

  /** Submit transient world-space lighting. Views need not exist yet.
   * The first 16 matching lights illuminate each view, in submission order.
   * Returns false for invalid data or allocation failure.
   */
  LDK_API bool ldk_renderer_submit_light(LDKRenderer *renderer,
      const LDKRendererLightSubmit *light);


  /**
   * @brief Initialize the renderer.
   *
   * The renderer stores the supplied RHI context, creates its internal rendering
   * passes, and allocates the GPU resources required for built-in rendering.
   *
   * The RHI context must already be initialized and must remain alive until
   * ldk_renderer_terminate() is called.
   *
   * @param renderer Renderer instance to initialize.
   * @param config Renderer configuration.
   * @return true if the renderer was initialized successfully, false otherwise.
   */
  // Call between frames. Resolution: power of two, 256..8192;
  // distance: finite and positive. Failure preserves existing resources.
  LDK_API bool ldk_renderer_shadow_settings_set(
      LDKRenderer *renderer, u32 resolution, float distance);

  LDK_API bool ldk_renderer_initialize(
      LDKRenderer* renderer,
      LDKRendererConfig const* config);

  /**
   * @brief Change the game render-target resolution.
   *
   * The current game render target is released when its dimensions change.
   * A target with the new dimensions is created when the next scene is
   * rendered.
   *
   * @param renderer Renderer instance.
   * @param width New game render-target width in pixels.
   * @param height New game render-target height in pixels.
   * @return true when the resolution is valid and was accepted.
   */
  LDK_API bool ldk_renderer_game_resolution_set(
      LDKRenderer* renderer,
      u32 width,
      u32 height);

  /**
   * @brief Terminate the renderer and release renderer-owned resources.
   *
   * This destroys all renderer-owned GPU resources, including mesh buffers,
   * texture resources, font page textures, render targets, pass buffers,
   * bindings, pipelines, samplers, and shader modules.
   *
   * CPU-side assets are not destroyed by this function. Assets remain owned by
   * the asset manager or by whoever created them.
   *
   * The RHI context must still be alive when this function is called.
   *
   * @param renderer Renderer instance to terminate.
   */
  LDK_API void ldk_renderer_terminate(
      LDKRenderer* renderer);

  /**
   * @brief Render one frame using the currently submitted renderer data.
   *
   * The renderer begins an RHI frame, renders the submitted scene meshes when a
   * view is available, optionally presents the scene target, renders submitted
   * UI data, and ends the RHI frame.
   *
   * Submitted per-frame data is consumed by this call. After rendering, the
   * renderer clears transient submissions such as submitted meshes, submitted UI,
   * and the active camera/view state.
   *
   * @param renderer Renderer instance.
   * @param desc Frame description containing framebuffer size and clear color.
   */
  LDK_API void ldk_renderer_render_frame(
      LDKRenderer* renderer,
      LDKRendererFrameDesc const* desc);

  /**
   * @brief Return renderer statistics for the last fully completed frame.
   *
   * CPU time measures the elapsed CPU-side duration of
   * ldk_renderer_render_frame(), including time spent inside RHI calls. It is
   * not GPU time and may include waits performed by the backend.
   *
   * Rendered work is also split by destination. game contains work rendered
   * into the configured game view (and direct game presentation/UI when
   * present_game is enabled). non_game contains all other views and host UI. A
   * VIEW_ALL submission is counted independently in each destination where it
   * is actually rendered. mesh_submit_count remains global because a single
   * submission may participate in both domains.
   *
   * @param renderer Renderer instance.
   * @return A snapshot of the previous completed renderer frame. A zeroed
   *         snapshot is returned for a null renderer or before the first frame.
   */
  LDK_API LDKRendererFrameStats ldk_renderer_last_frame_stats_get(
      LDKRenderer const* renderer);

  /**
   * @brief Return the game render target color texture for UI rendering.
   *
   * The returned texture is owned by the renderer and remains valid until the
   * game render target is recreated or the renderer is terminated.
   *
   * @param renderer Renderer instance.
   * @return Game color texture handle, or an invalid handle when unavailable.
   */
  LDK_API LDKUITextureHandle ldk_renderer_game_texture_get(
      LDKRenderer const* renderer);

  /**
   * @brief Return the color texture rendered for a submitted view.
   *
   * The view identifier is supplied by the caller when the view is submitted.
   * The returned texture is owned by the renderer and remains valid until the
   * view is removed, the game resolution changes, or the renderer terminates.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of the submitted view.
   * @return View color texture handle, or an invalid handle when unavailable.
   */
  LDK_API LDKUITextureHandle ldk_renderer_view_texture_get(
      LDKRenderer const* renderer,
      LDKRendererViewId view_id);
  /* Route overlay meshes to a transparent texture for this frame. Request
   * before rendering and compose the texture above other viewer UI layers. */
  LDK_API LDKUITextureHandle ldk_renderer_view_overlay_texture_request(
      LDKRenderer* renderer, LDKRendererViewId view_id);

  // ---------------------------------------------------------------------------
  // Mesh Resource
  // ---------------------------------------------------------------------------
  /**
   * @brief Return an invalid mesh resource handle.
   *
   * This is the renderer mesh equivalent of a null handle. It can be used
   * to initialize mesh fields or to represent the absence of a mesh.
   *
   * @return Invalid mesh resource handle.
   */
  LDK_API LDKResourceMesh ldk_renderer_mesh_null(void);

  /**
   * @brief Check whether a mesh resource handle refers to a live renderer mesh.
   *
   * The handle is validated against the renderer that owns the mesh resource.
   * A non-zero handle is not enough to prove validity; the resource must still
   * exist in the renderer mesh cache.
   *
   * @param renderer Renderer that owns the mesh resource.
   * @param mesh Mesh resource handle to validate.
   * @return true if the mesh is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_mesh_is_valid(
      LDKRenderer* renderer,
      LDKResourceMesh mesh);

  /**
   * @brief Create a renderer-owned mesh resource.
   *
   * The renderer uploads the supplied CPU-side vertex and index data into
   * renderer-owned RHI buffers, stores those buffers in the renderer mesh cache,
   * and returns a renderer resource handle.
   *
   * The source vertex and index arrays are only needed during creation. After the
   * mesh is created, the returned resource does not depend on the original CPU
   * buffers.
   *
   * @param renderer Renderer that will own the mesh resource.
   * @param desc Mesh creation description containing vertices and indices.
   * @return Mesh resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceMesh ldk_renderer_mesh_create(
      LDKRenderer* renderer,
      LDKRendererMeshDesc const* desc);

  /**
   * @brief Replace the contents of an existing renderer mesh resource.
   *
   * The mesh handle is resolved through the renderer mesh cache. The old
   * renderer-owned RHI vertex and index buffers are destroyed, then new buffers
   * are created from the supplied mesh description.
   *
   * The source vertex and index arrays are only needed during the update. After
   * the update succeeds, the mesh does not depend on the original CPU buffers.
   *
   * @param renderer Renderer that owns the mesh resource.
   * @param mesh Mesh resource handle to update.
   * @param desc New mesh data description.
   * @return true if the mesh was updated, false otherwise.
   */
  LDK_API bool ldk_renderer_mesh_update(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKRendererMeshDesc const* desc);

  /**
   * @brief Destroy a renderer-owned mesh resource.
   *
   * The renderer resolves the mesh handle, destroys the underlying RHI vertex and
   * index buffers, and releases the renderer mesh cache slot.
   *
   * Destroying an invalid or already-dead mesh handle is a no-op. Mesh resources
   * are also destroyed automatically when the renderer is terminated.
   *
   * @param renderer Renderer that ownss the mesh resource.
   * @param mesh Mesh resource handle to destroy.
   */
  LDK_API void ldk_renderer_mesh_destroy(
      LDKRenderer* renderer,
      LDKResourceMesh mesh);

  // ---------------------------------------------------------------------------
  // Texture Resource
  // ---------------------------------------------------------------------------
  /**
   * @brief Initialize renderer texture options with default values.
   *
   * Defaults preserve the renderer's existing texture behavior: nearest filtering,
   * clamp-to-edge wrapping, and no generated mipmaps.
   *
   * @param options Texture options to initialize.
   */
  LDK_API void ldk_renderer_texture_options_defaults(
      LDKRendererTextureOptions* options);

  /**
   * @brief Return an invalid texture resource handle.
   *
   * This is the renderer texture equivalent of a null handle. It can be used
   * to initialize texture fields or to represent the absence of a texture.
   *
   * @return Invalid texture resource handle.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_null(void);

  /**
   * @brief Check whether a texture resource handle refers to a live renderer texture.
   *
   * The handle is validated against the renderer that owns the texture resource.
   * A non-zero handle is not enough to prove validity; the resource must still
   * exist in the renderer texture cache.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to validate.
   * @return true if the texture is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_texture_is_valid(
      LDKRenderer* renderer,
      LDKResourceTexture texture);

  /**
   * @brief Resolve a renderer texture resource to a UI draw texture handle.
   *
   * The returned handle is a frame-local draw token suitable for UI draw
   * commands. The UI system must treat it as opaque and must not store it as a
   * persistent engine resource.
   *
   * The renderer owns the original texture resource. If the resource is invalid
   * or no longer alive, this function returns an invalid UI texture handle.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to resolve.
   * @return UI texture handle for draw commands, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_texture_ui_handle(
      LDKRenderer* renderer,
      LDKResourceTexture texture);

  /**
   * @brief Create a renderer-owned texture resource.
   *
   * The renderer translates the high-level texture description into the
   * appropriate RHI texture description, creates the underlying GPU texture,
   * stores it in the renderer texture cache, and returns a renderer resource
   * handle.
   *
   * The source pixel data is only needed during creation. After the texture is
   * created, the returned resource does not depend on the original pixel buffer.
   *
   * @param renderer Renderer that will own the texture resource.
   * @param desc Texture creation description.
   * @return Texture resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_create(
      LDKRenderer* renderer,
      LDKRendererTextureDesc const* desc);

  /**
   * @brief Update the pixel contents of an existing renderer texture resource.
   *
   * The texture handle is resolved through the renderer texture cache, then the
   * underlying RHI texture is updated with the supplied pixel data.
   *
   * The texture dimensions, format, channel count, and flags are not changed by
   * this call. The supplied byte count must match the expected data size for the
   * existing texture.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to update.
   * @param pixels New pixel data.
   * @param byte_count Size of the pixel data in bytes.
   * @return true if the texture was updated, false otherwise.
   */
  LDK_API bool ldk_renderer_texture_update(
      LDKRenderer* renderer,
      LDKResourceTexture texture,
      void const* pixels,
      u64 byte_count);

  /**
   * @brief Destroy a renderer-owned texture resource.
   *
   * The renderer resolves the texture handle, destroys the underlying RHI texture,
   * and releases the renderer texture cache slot.
   *
   * Destroying an invalid or already-dead texture handle is a no-op. Texture
   * resources are also destroyed automatically when the renderer is terminated.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to destroy.
   */
  LDK_API void ldk_renderer_texture_destroy(
      LDKRenderer* renderer,
      LDKResourceTexture texture);


  /**
   * @brief Create a renderer texture resource from an image with explicit options.
   *
   * Passing NULL for options uses ldk_renderer_texture_options_defaults().
   *
   * @param renderer Renderer that will own the texture resource.
   * @param image Source CPU-side image.
   * @param options Explicit texture sampling and mipmap options, or NULL.
   * @return Texture resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_create_from_image(
      LDKRenderer* renderer,
      LDKImage const* image,
      LDKRendererTextureOptions const* options);

  // ---------------------------------------------------------------------------
  // Material Resource
  // ---------------------------------------------------------------------------

  /**
   * @brief Return an invalid material resource handle.
   * @return Invalid material resource handle.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_null(void);

  /* Acquire a shared GPU snapshot of a live image asset. Uses default texture
   * options. Identity includes manager, asset index and generation. Release
   * once per acquisition; do not destroy the borrowed texture directly.
   * In-place image edits/hot reload are not tracked by this snapshot cache.
   */
  LDK_API LDKResourceTexture ldk_renderer_image_acquire(
      LDKRenderer* renderer, struct LDKAssetManager* assets,
      LDKAssetImage image);
  LDK_API void ldk_renderer_image_release(
      LDKRenderer* renderer, LDKResourceTexture texture);

  /**
   * @brief Resolve an authored material description into renderer resources.
   *
   * Textured materials acquire a shared renderer texture from the supplied
   * asset manager. If the authored image is unavailable, the renderer uses
   * the shared missing-image fallback. Lit normal/specular maps are acquired
   * when available; missing maps resolve to renderer-owned neutral fallbacks.
   * Existing output resources are replaced only after the new material has
   * been created successfully.
   *
   * The caller owns the returned material resource and one acquisition of each
   * non-null returned image texture. A later successful resolve replaces those
   * resources. The caller must destroy/release remaining resources when its
   * owner terminates.
   *
   * @param renderer Renderer that owns the runtime resources.
   * @param assets Asset manager used to resolve authored image assets. Required
   * for textured materials and for lit materials that reference maps.
   * @param material_desc Authored material description.
   * @param renderer_material In/out renderer material resource.
   * @param renderer_texture In/out acquired albedo texture resource.
   * @param renderer_normal_map In/out acquired normal-map texture resource.
   * @param renderer_specular_map In/out acquired specular-map texture resource.
   * @return true when the material was resolved successfully.
   */
  LDK_API bool ldk_renderer_material_resolve(LDKRenderer *renderer,
      struct LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
      LDKResourceMaterial *renderer_material,
      LDKResourceTexture *renderer_texture,
      LDKResourceTexture *renderer_normal_map,
      LDKResourceTexture *renderer_specular_map);

  /**
   * @brief Check whether a material handle refers to a live renderer material.
   * @param renderer Renderer that owns the material resource.
   * @param material Material resource handle to validate.
   * @return true if the material is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_material_is_valid(
      LDKRenderer* renderer,
      LDKResourceMaterial material);

  /**
   * @brief Create a renderer-owned material resource.
   *
   * Textured materials require a live renderer texture. The referenced texture
   * must remain alive until the material is destroyed. Textured materials may
   * use opaque or cutout alpha mode; cutout thresholds must be finite and in
   * [0, 1]. Lit normal/specular maps are optional borrowed texture resources and
   * must also remain alive while the material exists; null map handles use
   * renderer-owned neutral fallbacks. Vertex-color materials ignore texture and
   * alpha fields. Unlit materials ignore map handles. Lit surface values must be
   * finite and non-negative. A zero shininess uses the default value (32).
   *
   * @param renderer Renderer that will own the material resource.
   * @param desc Resolved renderer material description.
   * @return New material handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_create(
      LDKRenderer* renderer,
      LDKRendererMaterialDesc const* desc);

  /**
   * @brief Destroy a renderer-owned material resource.
   *
   * Destroying an invalid or already-dead material is a no-op. Material
   * resources are also destroyed automatically when the renderer terminates.
   *
   * @param renderer Renderer that owns the material resource.
   * @param material Material resource to destroy.
   */
  LDK_API void ldk_renderer_material_destroy(
      LDKRenderer* renderer,
      LDKResourceMaterial material);

  /**
   * @brief Return the renderer-owned default mesh material.
   *
   * The default material is a white vertex-color material created during
   * renderer initialization and destroyed during renderer termination.
   *
   * @param renderer Renderer that owns the default material.
   * @return Default material handle, or an invalid handle when unavailable.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_default_get(
      LDKRenderer* renderer);

  // ---------------------------------------------------------------------------
  // Font cache Resources
  // ---------------------------------------------------------------------------
  /**
   * @brief Get or create the renderer texture for a font atlas page.
   *
   * The UI system renders text using font atlas pages generated by LDKFontInstance.
   * This function returns a texture handle for the requested page, creating the
   * underlying renderer/RHI texture on demand and caching it for later calls.
   *
   * The returned handle is intended for UI draw commands. It is owned by the
   * renderer and must not be destroyed by the caller. Cached font page textures
   * are released when the renderer is terminated.
   *
   * @param renderer Renderer that owns the font page texture cache.
   * @param font Font instance that owns the requested atlas page.
   * @param page_index Index of the font atlas page.
   * @return UI texture handle for the atlas page, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture(LDKRenderer* renderer, LDKFontInstance* font, u32 page_index);

  /**
   * @brief Callback wrapper for ldk_renderer_get_font_page_texture().
   *
   * This function matches the callback shape expected by the UI/font rendering
   * code. The user pointer must be an LDKRenderer*.
   *
   * @param user Renderer pointer passed as opaque callback user data.
   * @param font Font instance that owns the requested atlas page.
   * @param page_index Index of the font atlas page.
   * @return UI texture handle for the atlas page, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index);

  // ---------------------------------------------------------------------------
  // Primitive Submission
  // ---------------------------------------------------------------------------
  /**
   * @brief Submit a camera view used for scene rendering this frame.
   *
   * The renderer stores the view and projection matrices as transient frame
   * state. Submitted meshes will be rendered using this view when
   * ldk_renderer_render_frame() is called.
   *
   * The submitted view is consumed for the current frame only. After rendering,
   * the renderer clears the active view state.
   *
   * @param renderer Renderer instance.
   * @param view_id Stable non-zero identifier supplied by the caller.
   * @param view View matrix.
   * @param projection Projection matrix.
   * @return true if the view was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      Mat4 view,
      Mat4 projection);

  /**
   * @brief Select the submitted view used as the game output.
   *
   * The selected view is returned by ldk_renderer_game_texture_get() and is
   * presented directly when the renderer was configured with present_game.
   * The selection is transient and must be set again on every frame.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of a view submitted for the current frame.
   * @return true if the submitted view exists, false otherwise.
   */
  LDK_API bool ldk_renderer_game_view_set(
      LDKRenderer* renderer,
      LDKRendererViewId view_id);

  /**
   * @brief Submit UI render data for the current frame.
   *
   * The renderer stores a pointer to the supplied UI render data and renders it
   * during ldk_renderer_render_frame().
   *
   * The render data is not copied, so it must remain valid until the frame is
   * rendered. After rendering, the renderer clears the submitted UI pointer.
   *
   * @param renderer Renderer instance.
   * @param render_data UI render data to draw this frame.
   */
  LDK_API void ldk_renderer_submit_ui(
      LDKRenderer* renderer,
      LDKUIRenderData const* render_data);

  /**
   * @brief Submit a mesh instance for scene rendering this frame.
   *
   * The mesh and material handles are validated against their renderer caches,
   * then queued with the supplied world transform. The queued mesh is rendered
   * during ldk_renderer_render_frame() for every submitted view.
   *
   * Submitted mesh instances are transient frame data. After rendering, the
   * renderer clears the submitted mesh queue.
   *
   * @param renderer Renderer instance.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);

  /* Explicit flags for scene/procedural submissions. The convenience mesh
   * submission functions enable CAST_SHADOWS; overlay submissions never cast.
   * These forms allow disabling casting without changing material lighting.
   */
  LDK_API bool ldk_renderer_submit_mesh_with_flags(LDKRenderer *renderer,
      LDKResourceMesh mesh, LDKResourceMaterial material, Mat4 world,
      u32 flags);
  LDK_API bool ldk_renderer_submit_mesh_range_with_flags(LDKRenderer *renderer,
      LDKResourceMesh mesh, LDKResourceMaterial material, u32 first_index,
      u32 index_count, Mat4 world, u32 flags);

  /**
   * @brief Submit a contiguous index range of a mesh for scene rendering.
   *
   * This is the ranged form of ldk_renderer_submit_mesh(). The mesh resource
   * remains independent of material topology; callers use this function to
   * submit an individual submesh with its resolved material.
   *
   * @param renderer Renderer instance.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param first_index First mesh index to draw.
   * @param index_count Number of indices to draw.
   * @param world World transform for this mesh instance.
   * @return true if the mesh range was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh_range(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      u32 first_index,
      u32 index_count,
      Mat4 world);

  /**
   * @brief Submit a mesh instance to a single render view for this frame.
   *
   * Unlike ldk_renderer_submit_mesh(), this mesh is only drawn while rendering
   * the view identified by view_id. This is intended for view-local scene
   * content that must not appear in other views.
   *
   * The view does not need to have been submitted before this call. If no view
   * with the supplied ID is submitted during the frame, the mesh is not drawn.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw this mesh.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);

  /**
   * @brief Submit a procedural ground grid to a single render view.
   *
   * The grid is rendered after regular scene meshes and before overlay meshes,
   * with depth testing enabled and depth writes disabled. Its visible area is
   * centered on grid_center and fades before reaching grid_extent.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw the grid.
   * @param grid_center Center of the visible grid area on the ground plane.
   * @param grid_extent Radius of the visible grid area in world units.
   * @param grid_spacing Distance between adjacent grid lines in world units.
   * @return true if the grid was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_grid_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      Vec3 grid_center,
      float grid_extent,
      float grid_spacing);

  /**
   * @brief Submit a mesh as a view-local overlay for this frame.
   *
   * Overlay meshes are rendered after regular meshes for the selected view,
   * with depth testing and depth writes disabled. This makes them independent
   * of the scene depth buffer and suitable for editor gizmos.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw this mesh.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_overlay_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);


#ifdef __cplusplus
}
#endif

#endif
