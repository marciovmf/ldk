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

  typedef struct LDKSkyboxIOContext
  {
    LDKAssetManager *assets;
  } LDKSkyboxIOContext;

  typedef struct LDKSkyboxIOResult
  {
    char error[512];
  } LDKSkyboxIOResult;

  LDK_API bool ldk_skybox_desc_read(const LDKSkyboxIOContext *context,
      const TMLDocument *doc, const TMLNode *fields,
      LDKSkyboxDesc *out_desc, LDKSkyboxIOResult *result);

  LDK_API bool ldk_skybox_desc_write(const LDKSkyboxIOContext *context,
      const LDKSkyboxDesc *desc, XStrBuilder *out, u32 indent,
      LDKSkyboxIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_IO_H */
