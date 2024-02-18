/**
 * image.h
 *
 * Asset handler for .image asset files
 *
 */

#ifndef LDK_IMAGE_H
#define LDK_IMAGE_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct 
  {
    uint32 width;
    uint32 height;
    unsigned char* data;
  } LDKImage;

  typedef LDKHandle LDKHImage;

  LDK_API LDKHImage ldkAssetImageLoadFunc(const char* path);
  LDK_API void ldkAssetImageUnloadFunc(LDKHImage handle);
  LDK_API LDKImage* ldkAssetImageGetPointer(LDKHImage handle);

#ifdef __cplusplus
}
#endif
#endif //LDK_IMAGE_H

