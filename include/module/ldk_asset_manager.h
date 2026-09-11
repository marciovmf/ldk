#ifndef LDK_ASSET_MANAGER_H
#define LDK_ASSET_MANAGER_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <ldk_ttf.h>
#include <ldk_image.h>
#include <ldk_mesh.h>
#include <stdx/stdx_hpool.h>
#include <stdx/stdx_filesystem.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKAssetInfo
  {
    LDKAssetType type;
    void* data;
    XFSPath asset_path;
    u64 load_timestamp;
  } LDKAssetInfo;

  typedef bool (*LDKAssetIterFn)(LDKAssetHandle asset, LDKAssetInfo* info, void* user);

  typedef struct LDKAssetManager
  {
    XHPool pool;
  } LDKAssetManager;

  // ---------------------------------------------------------------------------
  // General asset operations
  // ---------------------------------------------------------------------------

  LDK_API bool ldk_asset_manager_initialize(LDKAssetManager* manager, u32 page_capacity, u32 initial_pages);
  LDK_API void ldk_asset_manager_terminate(LDKAssetManager* manager);
  LDK_API void ldk_asset_manager_clear(LDKAssetManager* manager);

  LDK_API LDKAssetHandle ldk_asset_handle_null(void);

  LDK_API bool ldk_asset_handle_is_alive(LDKAssetManager* manager, LDKAssetHandle asset);

  LDK_API LDKAssetInfo* ldk_asset_get_info(LDKAssetManager* manager, LDKAssetHandle asset);
  LDK_API const LDKAssetInfo* ldk_asset_get_info_const(LDKAssetManager* manager, LDKAssetHandle asset);
  LDK_API LDKAssetType ldk_asset_get_type(LDKAssetManager* manager, LDKAssetHandle asset);
  LDK_API u32 ldk_asset_alive_count(LDKAssetManager* manager);
  LDK_API void ldk_asset_foreach(LDKAssetManager* manager, LDKAssetIterFn fn, void* user);


  // ---------------------------------------------------------------------------
  // Text file asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetTextFileData
  {
    char* text;
    u64 byte_count;
  } LDKAssetTextFileData;

  LDK_API LDKAssetTextFile ldk_asset_text_file_null(void);
  LDK_API bool ldk_asset_manager_text_file_is_alive(LDKAssetManager* manager, LDKAssetTextFile asset);
  LDK_API LDKAssetTextFile ldk_asset_manager_text_file_create(LDKAssetManager* manager, const char* text, u64 byte_count);
  LDK_API LDKAssetTextFile ldk_asset_manager_text_file_load(LDKAssetManager* manager, const char* path);
  LDK_API void ldk_asset_manager_text_file_unload(LDKAssetManager* manager, LDKAssetTextFile asset);
  LDK_API LDKAssetTextFileData* ldk_asset_manager_text_file_get(LDKAssetManager* manager, LDKAssetTextFile asset);
  LDK_API const LDKAssetTextFileData* ldk_asset_manager_text_file_get_const(LDKAssetManager* manager, LDKAssetTextFile asset);


  // ---------------------------------------------------------------------------
  // Image
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetImageData
  {
    LDKImage* image;
    bool is_missing; /* Procedural substitute for an unavailable image. */
  } LDKAssetImageData;

  LDK_API LDKAssetImage ldk_asset_image_null(void);
  LDK_API bool ldk_asset_manager_image_is_alive(LDKAssetManager* manager, LDKAssetImage asset);
  LDK_API LDKAssetImage ldk_asset_manager_image_create(
      LDKAssetManager* manager,
      u32 width,
      u32 height,
      const void* pixels);
  LDK_API LDKAssetImage ldk_asset_manager_image_load(
      LDKAssetManager* manager,
      const char* path);
  /* Reuse a loaded image by normalized absolute path, or load it once.
   * The returned asset is shared and lives until manager clear/termination;
   * callers must not individually unload it. No file hot reload is performed. */
  LDK_API LDKAssetImage ldk_asset_manager_image_load_shared(
      LDKAssetManager* manager, const char* path);
  /* Shared magenta/black checkerboard for an unavailable image. Preserve the
   * missing absolute path for scene saves; NULL selects the unassigned fallback.
   * Manager-owned, like image_load_shared. */
  LDK_API LDKAssetImage ldk_asset_manager_image_missing(
      LDKAssetManager* manager, const char* path);
  LDK_API void ldk_asset_manager_image_unload(
      LDKAssetManager* manager,
      LDKAssetImage asset);
  LDK_API LDKAssetImageData* ldk_asset_manager_image_get(
      LDKAssetManager* manager,
      LDKAssetImage asset);
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

  LDK_API LDKAssetFont ldk_asset_font_null(void);
  LDK_API bool ldk_asset_manager_font_is_alive(LDKAssetManager* manager, LDKAssetFont asset);
  LDK_API LDKAssetFont ldk_asset_manager_font_create(LDKAssetManager* manager, const void* data, u32 data_size);
  LDK_API LDKAssetFont ldk_asset_manager_font_load(LDKAssetManager* manager, const char* path);
  LDK_API void ldk_asset_manager_font_unload(LDKAssetManager* manager, LDKAssetFont asset);
  LDK_API LDKAssetFontData* ldk_asset_manager_font_get(LDKAssetManager* manager, LDKAssetFont asset);
  LDK_API const LDKAssetFontData* ldk_asset_manager_font_get_const(LDKAssetManager* manager, LDKAssetFont asset);



  // ---------------------------------------------------------------------------
  // Mesh asset
  // ---------------------------------------------------------------------------

  typedef struct LDKAssetMeshData
  {
    char name[LDK_MESH_NAME_CAPACITY];
    LDKMeshData mesh;
    LDKMeshSubmesh* submeshes;
    u32 submesh_count;
    LDKMeshMaterialSlot* material_slots;
    u32 material_slot_count;
    u32 source_mesh_index;
    u32 source_mesh_count;
  } LDKAssetMeshData;

  LDK_API LDKAssetMesh ldk_asset_mesh_null(void);
  LDK_API bool ldk_asset_manager_mesh_is_alive(LDKAssetManager* manager, LDKAssetMesh asset);
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_create(LDKAssetManager* manager, const LDKMeshVertex* vertices, u32 vertex_count, const u32* indices, u32 index_count);
  LDK_API void ldk_asset_manager_mesh_unload(LDKAssetManager* manager, LDKAssetMesh asset);
  LDK_API LDKAssetMeshData* ldk_asset_manager_mesh_get(LDKAssetManager* manager, LDKAssetMesh asset);
  LDK_API const LDKAssetMeshData* ldk_asset_manager_mesh_get_const(LDKAssetManager* manager, LDKAssetMesh asset);


#ifdef __cplusplus
}
#endif

#endif //LDK_ASSET_MANAGER_H
