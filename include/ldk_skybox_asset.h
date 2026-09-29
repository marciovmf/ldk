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

  LDK_API LDKAssetSkybox ldk_asset_skybox_null(void);
  LDK_API const LDKAssetSkyboxData *ldk_asset_manager_skybox_get_const(
      LDKAssetManager *manager, LDKAssetSkybox asset);

  LDK_API LDKAssetSkybox ldk_asset_manager_skybox_create(
      const LDKSkyboxIOContext *context, const char *path,
      const LDKSkyboxDesc *descriptor, LDKSkyboxIOResult *result);

  LDK_API LDKAssetSkybox ldk_asset_manager_skybox_load_shared(
      const LDKSkyboxIOContext *context, const char *path,
      LDKSkyboxIOResult *result);

  LDK_API bool ldk_asset_manager_skybox_update(LDKAssetManager *manager,
      LDKAssetSkybox asset, const LDKSkyboxDesc *descriptor);

  LDK_API bool ldk_asset_manager_skybox_save(
      const LDKSkyboxIOContext *context, LDKAssetSkybox asset,
      LDKSkyboxIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_ASSET_H */
