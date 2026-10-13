/**
 * # Skybox assets
 *
 * Create, load, update, and save skyboxes managed as assets.
 */

#ifndef LDK_SKYBOX_ASSET_H
#define LDK_SKYBOX_ASSET_H

#include <ldk_skybox_io.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKAssetSkyboxData
  {
    LDKSkyboxDesc descriptor;
    u64 revision;
    bool dirty;
  } LDKAssetSkyboxData;

  /**
   * @brief Return an invalid handle for the asset skybox.
   * @return An invalid or empty handle.
   */
  LDK_API LDKAssetSkybox ldk_asset_skybox_null(void);
  /**
   * @brief Get read-only access to the asset manager skybox.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKAssetSkyboxData *ldk_asset_manager_skybox_get_const(
      LDKAssetManager *manager, LDKAssetSkybox asset);

  /**
   * @brief Create the asset manager skybox.
   * @param context Context used by this operation.
   * @param path Path identifying the file or resource.
   * @param descriptor Resource or component descriptor.
   * @param result Optional result structure populated with error details.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetSkybox ldk_asset_manager_skybox_create(
      const LDKSkyboxIOContext *context, const char *path,
      const LDKSkyboxDesc *descriptor, LDKSkyboxIOResult *result);

  /**
   * @brief Load or reuse shared the asset manager skybox.
   * @param context Context used by this operation.
   * @param path Path identifying the file or resource.
   * @param result Optional result structure populated with error details.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKAssetSkybox ldk_asset_manager_skybox_load_shared(
      const LDKSkyboxIOContext *context, const char *path,
      LDKSkyboxIOResult *result);

  /**
   * @brief Update the asset manager skybox.
   * @param manager Asset manager that owns or resolves the asset.
   * @param asset Asset handle to inspect or modify.
   * @param descriptor Resource or component descriptor.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_skybox_update(LDKAssetManager *manager,
      LDKAssetSkybox asset, const LDKSkyboxDesc *descriptor);

  /**
   * @brief Save the asset manager skybox.
   * @param context Context used by this operation.
   * @param asset Asset handle to inspect or modify.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_asset_manager_skybox_save(
      const LDKSkyboxIOContext *context, LDKAssetSkybox asset,
      LDKSkyboxIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_ASSET_H */
