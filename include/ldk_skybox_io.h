/**
 * # Skybox serialization
 *
 * Read and write skybox descriptors through TML and the asset manager.
 */

#ifndef LDK_SKYBOX_IO_H
#define LDK_SKYBOX_IO_H

#include <ldk_skybox.h>
#include <module/ldk_asset_manager.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_tml.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group ldk.asset.skybox */
  typedef struct LDKSkyboxIOContext
  {
    LDKAssetManager *assets;
  } LDKSkyboxIOContext;

  /** @group ldk.asset.skybox */
  typedef struct LDKSkyboxIOResult
  {
    char error[512];
  } LDKSkyboxIOResult;

  /**
   * @brief Read the skybox desc.
   * @param context Context used by this operation.
   * @param doc Doc used by this operation.
   * @param fields TML node containing serialized fields.
   * @param out_desc Receives the descriptor.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.skybox
   */
  LDK_API bool ldk_skybox_desc_read(const LDKSkyboxIOContext *context,
      const TMLDocument *doc, const TMLNode *fields,
      LDKSkyboxDesc *out_desc, LDKSkyboxIOResult *result);

  /**
   * @brief Write the skybox desc.
   * @param context Context used by this operation.
   * @param desc Description or configuration for this operation.
   * @param out Output object or buffer receiving the result.
   * @param indent Indentation depth used while serializing output.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.asset.skybox
   */
  LDK_API bool ldk_skybox_desc_write(const LDKSkyboxIOContext *context,
      const LDKSkyboxDesc *desc, XStrBuilder *out, u32 indent,
      LDKSkyboxIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_IO_H */
