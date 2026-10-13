/**
 * # Asset manager
 *
 * Create, acquire, query, reload, and release typed runtime assets.
 */

#ifndef LDK_ASSET_MANAGER_H
#define LDK_ASSET_MANAGER_H

#include <ldk_asset.h>
#include <ldk_keyframe_animation.h>
#include <ldk_texture_atlas.h>
#include <ldk_common.h>
#include <ldk_ttf.h>
#include <ldk_image.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_hpool.h>
#include <stdx/stdx_filesystem.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKAssetInfo
  {
    LDKAssetType type;
    void* data;
    LDKAssetPath asset_path;
    u64 load_timestamp;
    u64 source_revision;
  } LDKAssetInfo;

  /**
   * @brief Callback invoked for each live asset during enumeration.
   * @param asset Asset handle to inspect or modify.
   * @param info Info used by this operation.
   * @param user Opaque caller-supplied data passed to the callback.
   * @return True if the operation succeeds; false otherwise.
   */
  typedef bool (*LDKAssetIterFn)(LDKAssetHandle asset, LDKAssetInfo* info, void* user);

  typedef struct LDKAssetManager
  {
    XHPool pool;
    LDKAssetSource* source;
  } LDKAssetManager;

  // ---------------------------------------------------------------------------
  // General asset operations
  // ---------------------------------------------------------------------------

  /**
   * @brief Initialize an asset manager with an asset source and storage capacity.
   * @param manager Asset manager that owns or resolves the asset.
   * @param source Source object or source data for this operation.
   * @param page_capacity Number of entries per allocation page.
   * @param initial_pages Number of pages initially allocated.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_initialize(LDKAssetManager* manager, LDKAssetSource* source, u32 page_capacity, u32 initial_pages);
  /**
   * @brief Shut down the asset manager.
   * @param manager Asset manager that owns or resolves the asset.
   */
  LDK_API void ldk_asset_manager_terminate(LDKAssetManager* manager);
  /* Clears manager-owned assets except audio data retained by active voices. */
  /**
   * @brief Clear manager-owned assets except audio retained by active voices.
   *
   * Audio data still referenced by active voices is retained.
   * @param manager Asset manager that owns or resolves the asset.
   */
  LDK_API void ldk_asset_manager_clear(LDKAssetManager* manager);

  /**
   * @brief Return an invalid handle for the asset handle.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetHandle ldk_asset_handle_null(void);

  /**
   * @brief Check whether the asset handle refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_handle_is_alive(LDKAssetManager* manager, LDKAssetHandle asset);

  /**
   * @brief Get mutable metadata for an asset handle.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetInfo* ldk_asset_get_info(LDKAssetManager* manager, LDKAssetHandle asset);
  /**
   * @brief Get read-only metadata for an asset handle.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetInfo* ldk_asset_get_info_const(LDKAssetManager* manager, LDKAssetHandle asset);
  /**
   * @brief Get the type tag of a live asset.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetType ldk_asset_get_type(LDKAssetManager* manager, LDKAssetHandle asset);
  /**
   * @brief Get the number of currently live asset handles.
   * @param manager Asset manager that owns or resolves the asset.
   * @return Number of matching entries.
   */
  LDK_API u32 ldk_asset_alive_count(LDKAssetManager* manager);
  /**
   * @brief Visit active assets with a caller-provided callback.
   * @param manager Asset manager that owns or resolves the asset.
   * @param fn Callback invoked by this operation.
   * @param user Opaque caller-supplied data passed to the callback.
   */
  LDK_API void ldk_asset_foreach(LDKAssetManager* manager, LDKAssetIterFn fn, void* user);


  // ---------------------------------------------------------------------------
  // Text file asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetTextFileData
  {
    char* text;
    u64 byte_count;
  } LDKAssetTextFileData;

  /**
   * @brief Return an invalid handle for the asset text file.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetTextFile ldk_asset_text_file_null(void);
  /**
   * @brief Check whether the asset manager text file refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_text_file_is_alive(LDKAssetManager* manager, LDKAssetTextFile asset);
  /**
   * @brief Create an in-memory text-file asset by copying source text.
   * @param manager Asset manager that owns or resolves the asset.
   * @param text Text used by this operation.
   * @param byte_count Number of bytes in the supplied data.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetTextFile ldk_asset_manager_text_file_create(LDKAssetManager* manager, const char* text, u64 byte_count);
  /**
   * @brief Load the asset manager text file.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetTextFile ldk_asset_manager_text_file_load(LDKAssetManager* manager, const char* path);
  /**
   * @brief Unload the asset manager text file.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   */
  LDK_API void ldk_asset_manager_text_file_unload(LDKAssetManager* manager, LDKAssetTextFile asset);
  /**
   * @brief Get the asset manager text file.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetTextFileData* ldk_asset_manager_text_file_get(LDKAssetManager* manager, LDKAssetTextFile asset);
  /**
   * @brief Get read-only access to the asset manager text file.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetTextFileData* ldk_asset_manager_text_file_get_const(LDKAssetManager* manager, LDKAssetTextFile asset);


  // ---------------------------------------------------------------------------
  // Keyframe animation (.anim)
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetKeyframeAnimationData
  {
    LDKKeyframeAnimation clip;
  } LDKAssetKeyframeAnimationData;

  /**
   * @brief Return an invalid keyframe-animation asset handle.
   * @return Null handle.
   */
  LDK_API LDKAssetKeyframeAnimation ldk_asset_keyframe_animation_null(void);
  /**
   * @brief Check whether a handle refers to a live keyframe-animation asset.
   * @param manager Initialized asset manager.
   * @param asset Keyframe animation asset handle.
   * @return True only for a currently alive asset of the expected type.
   */
  LDK_API bool ldk_asset_manager_keyframe_animation_is_alive(
      LDKAssetManager *manager, LDKAssetKeyframeAnimation asset);
  /**
   * @brief Create a transient, manager-owned keyframe asset from a code-authored clip.
   * @param manager Initialized asset manager.
   * @param clip Animation clip instance.
   * @return Live asset handle on success, null handle on failure. Clip is deep-copied.
   * Note: The created asset has no path and is not automatically scene-persistent.
   */
  LDK_API LDKAssetKeyframeAnimation ldk_asset_manager_keyframe_animation_create(
      LDKAssetManager *manager, const LDKKeyframeAnimation *clip);
  /**
   * @brief Destroy a keyframe-animation asset and invalidate its handle.
   * @param manager Initialized asset manager.
   * @param asset Keyframe animation asset handle.
   * @return Nothing. Detach it from all animation sources first.
   */
  LDK_API void ldk_asset_manager_keyframe_animation_unload(
      LDKAssetManager *manager, LDKAssetKeyframeAnimation asset);
  /**
   * @brief Load or reuse a version-2 .anim asset by path and source revision.
   * @param manager Initialized asset manager.
   * @param path Logical path to an .anim asset.
   * @return Live shared handle, or null on failure. Owned by the manager.
   */
  LDK_API LDKAssetKeyframeAnimation ldk_asset_manager_keyframe_animation_load_shared(
      LDKAssetManager *manager, const char *path);
  /**
   * @brief Reload a cached shared animation asset from its source.
   * @param manager Initialized asset manager.
   * @param path Logical path to a cached .anim asset.
   * @return True on successful refresh or if not cached; false on I/O or parsing errors.
   */
  LDK_API bool ldk_asset_manager_keyframe_animation_reload(
      LDKAssetManager *manager, const char *path);
  /**
   * @brief Get immutable animation data for a live keyframe asset handle.
   * @param manager Initialized asset manager.
   * @param asset Keyframe animation asset handle.
   * @return Borrowed pointer or NULL. Invalidated when the asset is unloaded or reloaded.
   */
  LDK_API const LDKAssetKeyframeAnimationData *ldk_asset_manager_keyframe_animation_get_const(
      LDKAssetManager *manager, LDKAssetKeyframeAnimation asset);

  // ---------------------------------------------------------------------------
  // Texture atlas (.atlas)
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetTextureAtlasData
  {
    LDKTextureAtlas atlas;
    LDKAssetImage image;
    u64 revision; /* Shared image; manager owns its lifetime. */
  } LDKAssetTextureAtlasData;

  /**
   * @brief Return an invalid handle for the asset texture atlas.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetTextureAtlas ldk_asset_texture_atlas_null(void);
  /**
   * @brief Check whether the asset manager texture atlas refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_texture_atlas_is_alive(
      LDKAssetManager *manager, LDKAssetTextureAtlas asset);
  /**
   * @brief Create the asset manager texture atlas.
   * @param manager Asset manager that owns or resolves the asset.
   * @param atlas Texture atlas being inspected or modified.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetTextureAtlas ldk_asset_manager_texture_atlas_create(
      LDKAssetManager *manager, const LDKTextureAtlas *atlas);
  /**
   * @brief Load or reuse shared the asset manager texture atlas.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetTextureAtlas ldk_asset_manager_texture_atlas_load_shared(
      LDKAssetManager *manager, const char *path);
  /**
   * @brief Reload the asset manager texture atlas.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_texture_atlas_reload(
      LDKAssetManager *manager, const char *path);
  /**
   * @brief Unload the asset manager texture atlas.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   */
  LDK_API void ldk_asset_manager_texture_atlas_unload(
      LDKAssetManager *manager, LDKAssetTextureAtlas asset);
  /**
   * @brief Get read-only access to the asset manager texture atlas.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetTextureAtlasData *ldk_asset_manager_texture_atlas_get_const(
      LDKAssetManager *manager, LDKAssetTextureAtlas asset);

  // ---------------------------------------------------------------------------
  // Image
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetImageData
  {
    LDKImage* image;
    bool is_missing; /* Procedural substitute for an unavailable image. */
  } LDKAssetImageData;

  /**
   * @brief Return an invalid handle for the asset image.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetImage ldk_asset_image_null(void);
  /**
   * @brief Check whether the asset manager image refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_image_is_alive(LDKAssetManager* manager, LDKAssetImage asset);
  /**
   * @brief Create the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param pixels Image pixel data.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetImage ldk_asset_manager_image_create(
      LDKAssetManager* manager,
      u32 width,
      u32 height,
      const void* pixels);
  /**
   * @brief Create an image asset using an explicit pixel format.
   * @param manager Asset manager that owns or resolves the asset.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param format Format to use for this resource.
   * @param pixels Image pixel data.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetImage ldk_asset_manager_image_create_format(
      LDKAssetManager* manager,
      u32 width,
      u32 height,
      LDKImageFormat format,
      const void* pixels);
  /**
   * @brief Load the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetImage ldk_asset_manager_image_load(
      LDKAssetManager* manager,
      const char* path);
  /* Reuse a loaded image by asset path, or load it once.
   * The returned asset is shared and lives until manager clear/termination;
   * callers must not individually unload it. No file hot reload is performed. */
  /**
   * @brief Load or reuse shared the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetImage ldk_asset_manager_image_load_shared(
      LDKAssetManager* manager, const char* path);
  /* Shared magenta/black checkerboard for an unavailable image. Preserve the
   * missing asset path for scene saves; NULL selects the unassigned fallback.
   * Manager-owned, like image_load_shared. */
  /**
   * @brief Create or retrieve a missing-image placeholder asset.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetImage ldk_asset_manager_image_missing(
      LDKAssetManager* manager, const char* path);
  /**
   * @brief Unload the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   */
  LDK_API void ldk_asset_manager_image_unload(
      LDKAssetManager* manager,
      LDKAssetImage asset);
  /**
   * @brief Get the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetImageData* ldk_asset_manager_image_get(
      LDKAssetManager* manager,
      LDKAssetImage asset);
  /**
   * @brief Get read-only access to the asset manager image.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetImageData* ldk_asset_manager_image_get_const(
      LDKAssetManager* manager,
      LDKAssetImage asset);

  // ---------------------------------------------------------------------------
  // Font asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetFontData
  {
    LDKFontFace* face;
  } LDKAssetFontData;

  /**
   * @brief Return an invalid handle for the asset font.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetFont ldk_asset_font_null(void);
  /**
   * @brief Check whether the asset manager font refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_font_is_alive(LDKAssetManager* manager, LDKAssetFont asset);
  /**
   * @brief Create the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param data Source data or system-owned context for this operation.
   * @param data_size Size of the data buffer or region.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetFont ldk_asset_manager_font_create(LDKAssetManager* manager, const void* data, u32 data_size);
  /**
   * @brief Load the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetFont ldk_asset_manager_font_load(LDKAssetManager* manager, const char* path);
  /* Reuse a loaded font by asset path, or load it once. The returned asset is
   * manager-owned and must not be individually unloaded. */
  /**
   * @brief Load or reuse shared the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetFont ldk_asset_manager_font_load_shared(
      LDKAssetManager* manager, const char* path);
  /**
   * @brief Unload the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   */
  LDK_API void ldk_asset_manager_font_unload(LDKAssetManager* manager, LDKAssetFont asset);
  /**
   * @brief Get the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetFontData* ldk_asset_manager_font_get(LDKAssetManager* manager, LDKAssetFont asset);
  /**
   * @brief Get read-only access to the asset manager font.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetFontData* ldk_asset_manager_font_get_const(LDKAssetManager* manager, LDKAssetFont asset);

  // ---------------------------------------------------------------------------
  // Audio asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetAudioData
  {
    LDKAssetSourceFile source_file;
    void* encoded_data;
    u64 encoded_size;
    u32 ref_count;
  } LDKAssetAudioData;

  /**
   * @brief Return an invalid handle for the asset audio.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetAudio ldk_asset_audio_null(void);
  /**
   * @brief Check whether the asset manager audio refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_audio_is_alive(
      LDKAssetManager* manager, LDKAssetAudio asset);
  /**
   * @brief Load or reuse shared the asset manager audio.
   * @param manager Asset manager that owns or resolves the asset.
   * @param path Path identifying the file or resource.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetAudio ldk_asset_manager_audio_load_shared(
      LDKAssetManager* manager, const char* path);
  /**
   * @brief Get the asset manager audio.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetAudioData* ldk_asset_manager_audio_get(
      LDKAssetManager* manager, LDKAssetAudio asset);
  /**
   * @brief Get read-only access to the asset manager audio.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetAudioData* ldk_asset_manager_audio_get_const(
      LDKAssetManager* manager, LDKAssetAudio asset);


  // ---------------------------------------------------------------------------
  // Mesh asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetMeshEntry
  {
    char name[LDK_MESH_NAME_CAPACITY];
    LDKMeshData mesh;
    LDKMeshSubmesh* submeshes;
    u32 submesh_count;
    LDKMeshMaterialSlot* material_slots;
    u32 material_slot_count;
  } LDKAssetMeshEntry;

  typedef struct LDKAssetMeshData
  {
    /*
     * Ownership anchors used by the asset manager destructor. External mesh
     * assets pack all vertex-side data in mesh.vertices and all indices in
     * mesh.indices. For programmatic/built-in assets this is also the implicit
     * mesh at index zero.
     */
    LDKMeshData mesh;
    /* LDK_MESH_PRIMITIVE_COUNT for non-built-in meshes. */
    LDKMeshPrimitive primitive;
    LDKAssetMeshEntry* meshes;
    u32 mesh_count;
    LDKMeshNode* nodes;
    u32 node_count;
  } LDKAssetMeshData;

  /**
   * @brief Return an invalid handle for the asset mesh.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetMesh ldk_asset_mesh_null(void);
  /**
   * @brief Check whether the asset manager mesh refers to a live resource.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_mesh_is_alive(LDKAssetManager* manager, LDKAssetMesh asset);
  /**
   * @brief Create the asset manager mesh.
   * @param manager Asset manager that owns or resolves the asset.
   * @param vertices Array of mesh vertices.
   * @param vertex_count Number of vertex entries.
   * @param indices Array of mesh indices.
   * @param index_count Number of indices to submit.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_create(LDKAssetManager* manager, const LDKMeshVertex* vertices, u32 vertex_count, const u32* indices, u32 index_count);
  /**
   * @brief Unload the asset manager mesh.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   */
  LDK_API void ldk_asset_manager_mesh_unload(LDKAssetManager* manager, LDKAssetMesh asset);
  /**
   * @brief Get the asset manager mesh.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKAssetMeshData* ldk_asset_manager_mesh_get(LDKAssetManager* manager, LDKAssetMesh asset);
  /**
   * @brief Get read-only access to the asset manager mesh.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetMeshData* ldk_asset_manager_mesh_get_const(LDKAssetManager* manager, LDKAssetMesh asset);


#ifdef __cplusplus
}
#endif

#endif //LDK_ASSET_MANAGER_H
