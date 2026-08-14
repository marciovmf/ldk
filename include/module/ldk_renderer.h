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
    LDK_SHADER_GRID_PASS
  } LDKShader;

  typedef struct LDKRendererMeshDesc
  {
    const LDKMeshVertex* vertices;
    u32 vertex_count;
    const u32* indices;
    u32 index_count;
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

  typedef enum LDKRendererMeshSubmitFlag
  {
    LDK_RENDERER_MESH_SUBMIT_FLAG_NONE = 0,
    LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY = 1 << 0
  } LDKRendererMeshSubmitFlag;

  typedef struct LDKRendererMeshSubmit
  {
    LDKResourceMesh mesh;
    Mat4 world;
    LDKRendererViewId view_id;
    u32 flags;
  } LDKRendererMeshSubmit;

  typedef struct LDKRendererConfig
  {
    LDKRHIContext* rhi;
    u32 initial_ui_vertex_capacity;
    u32 initial_ui_index_capacity;
    u32 game_width;
    u32 game_height;
    bool present_game;
  } LDKRendererConfig;

  typedef struct LDKRendererFrameDesc
  {
    i32 framebuffer_width;
    i32 framebuffer_height;
    rgba32 clear_color;
    bool clear_color_enabled;
  } LDKRendererFrameDesc;

  typedef struct LDKRendererBindingsCacheEntry
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    LDKRHIBindings bindings;
  } LDKRendererBindingsCacheEntry;

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

  typedef struct LDKRendererMeshPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIPipeline overlay_pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBindings bindings;
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
  } LDKRendererTextureResource;

  typedef struct LDKRenderer
  {
    LDKRHIContext* rhi;
    LDKRendererUIPass ui_pass;
    LDKRendererMeshPass mesh_pass;
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

    // Font atlas cache
    LDKRendererFontPageCacheEntry* font_pages;
    u32 font_page_count;
    u32 font_page_capacity;

    // Submitted meshes
    LDKRendererMeshSubmit* submitted_meshes;
    u32 submitted_mesh_count;
    u32 submitted_mesh_capacity;

    bool is_initialized;
  } LDKRenderer;

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
   * @param renderer Renderer that owns the mesh resource.
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
   * The mesh handle is validated against the renderer mesh cache, then queued
   * with the supplied world transform. The queued mesh is rendered during
   * ldk_renderer_render_frame() for every submitted view.
   *
   * Submitted mesh instances are transient frame data. After rendering, the
   * renderer clears the submitted mesh queue.
   *
   * @param renderer Renderer instance.
   * @param mesh Mesh resource handle to render.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
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
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
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
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_overlay_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
      Mat4 world);


#ifdef __cplusplus
}
#endif

#endif
