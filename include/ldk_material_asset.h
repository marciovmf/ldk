/**
 * # Material assets
 *
 * Create, load, edit, and save material assets through the asset manager.
 */

#ifndef LDK_MATERIAL_ASSET_H
#define LDK_MATERIAL_ASSET_H

#include <ldk_material_io.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group ldk.asset.material */
  typedef struct LDKAssetMaterialData
  {
    LDKMaterialDesc descriptor;
    u64 revision;
    bool dirty;
    bool is_missing; /* Read-only procedural fallback; preserves the file path. */
  } LDKAssetMaterialData;

  /* Material assets are manager-owned until clear/termination. Image references
   * are shared. Use update instead of mutating the descriptor directly.
   * Paths are logical asset paths; identity is the path within the current
   * asset source. No automatic file reload is performed. */
  /**
   * @brief Return an invalid handle for the asset material.
   * @return An invalid or empty handle.
   * @group ldk.asset.material
   */
  LDK_API LDKAssetMaterial ldk_asset_material_null(void);
  /**
   * @brief Get read-only access to the asset manager material.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.asset.material
   */
  LDK_API const LDKAssetMaterialData *ldk_asset_manager_material_get_const(
      LDKAssetManager *manager, LDKAssetMaterial asset);

  /* Create an unsaved asset (revision 1, dirty). Fails if the path is already
   * cached or exists on disk. Does not write a file or create directories. */
  /**
   * @brief Create the asset manager material.
   * @param context Context used by this operation.
   * @param path Path identifying the file or resource.
   * @param descriptor Resource or component descriptor.
   * @param result Optional result structure populated with error details.
   * @return Handle to the requested object or resource.
   * @group ldk.asset.material
   */
  LDK_API LDKAssetMaterial ldk_asset_manager_material_create(
      const LDKMaterialIOContext *context, const char *path,
      const LDKMaterialDesc *descriptor, LDKMaterialIOResult *result);

  /* Reuses an existing cached asset, including unsaved edits. Otherwise reads
   * a material: node containing the existing material_type/color/texture fields.
   * Texture references remain logical asset paths. Missing files
   * return a cached unlit magenta-checker placeholder and emit an error diagnostic.
   * Invalid files and allocation failures still return a null handle. */
  /**
   * @brief Load or reuse shared the asset manager material.
   * @param context Context used by this operation.
   * @param path Path identifying the file or resource.
   * @param result Optional result structure populated with error details.
   * @return Handle to the requested object or resource.
   * @group ldk.asset.material
   */
  LDK_API LDKAssetMaterial ldk_asset_manager_material_load_shared(
      const LDKMaterialIOContext *context, const char *path,
      LDKMaterialIOResult *result);

  /* Equal descriptors are a successful no-op. Changes increment revision and
   * set dirty. Invalid handles/descriptors leave the asset untouched. */
  /**
   * @brief Update the asset manager material.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @param descriptor Resource or component descriptor.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.material
   */
  LDK_API bool ldk_asset_manager_material_update(LDKAssetManager *manager,
      LDKAssetMaterial asset, const LDKMaterialDesc *descriptor);

  /* Saves to the asset's own path. Clears dirty only after a successful write
   * and close; saving does not change revision. result is optional. */
  /**
   * @brief Save the asset manager material.
   * @param context Context used by this operation.
   * @param asset Asset handle to inspect or modify.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.material
   */
  LDK_API bool ldk_asset_manager_material_save(
      const LDKMaterialIOContext *context, LDKAssetMaterial asset,
      LDKMaterialIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MATERIAL_ASSET_H */
