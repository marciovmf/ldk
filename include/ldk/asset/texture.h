/*
 * tesxture.h
 *
 * Asset handler for .texture asset files.
 *
 */

#ifndef LDK_TEXTURE_H
#define LDK_TEXTURE_H

#include "../common.h"
#include "../module/renderer.h"


#ifdef __cplusplus
extern "C" {
#endif

  LDK_API LDKHTexture ldkAssetTextureLoadFunc(const char* path);
  LDK_API void ldkAssetTextureUnloadFunc(LDKHTexture handle);


#ifdef __cplusplus
}
#endif

#endif // LDK_TEXTURE_H

