/*
 * tesxture.h
 *
 * Asset handler for .texture asset files.
 *
 */

#ifndef LDK_TEXTURE_H
#define LDK_TEXTURE_H

#include "../module/asset.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum
  {
    LDK_TEXTURE_WRAP_CLAMP_TO_EDGE   = 0,
    LDK_TEXTURE_WRAP_CLAMP_TO_BORDER = 1,
    LDK_TEXTURE_WRAP_MIRRORED_REPEAT = 2,
    LDK_TEXTURE_WRAP_REPEAT          = 3,
  } LDKTextureParamWrap;

  typedef enum
  {
    LDK_TEXTURE_FILTER_LINEAR        = 0,
    LDK_TEXTURE_FILTER_NEAREST       = 1,
  } LDKTextureParamFilter;

  typedef enum
  {
    LDK_TEXTURE_MIPMAP_MODE_NONE     = 0,
    LDK_TEXTURE_MIPMAP_MODE_NEAREST  = 1,
    LDK_TEXTURE_MIPMAP_MODE_LINEAR   = 2,
  } LDKTextureParamMipmap;


  typedef struct
  {
    uint32 id;
  }LDKGLTexture;

  typedef struct
  {
    LDK_DECLARE_ASSET;
    union
    {
      LDKGLTexture gl;
    };
    bool useMipmap;
  } LDKTexture;

  LDK_API bool ldkAssetTextureLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetTextureUnloadFunc(LDKAsset handle);


#ifdef __cplusplus
}
#endif

#endif // LDK_TEXTURE_H

