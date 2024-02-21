/**
 * image.h
 *
 * Asset handler for .image asset files
 *
 */

#ifndef LDK_IMAGE_H
#define LDK_IMAGE_H

#include "../common.h"
#include "../module/asset.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct 
  {
    LDK_DECLARE_ASSET;
    uint32 width;
    uint32 height;
    unsigned char* data;
    bool raw;
  } LDKImage;

  typedef LDKHandle LDKHImage;

  LDK_API bool ldkAssetImageLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetImageUnloadFunc(LDKAsset asset);

#ifdef __cplusplus
}
#endif
#endif //LDK_IMAGE_H

