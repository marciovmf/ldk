/**
 *
 * asset.h
 *
 * Interface for retrieveing assets by name.
 * If an asset is not already loaded, the asset manager will look for an asset
 * handler for the specific asset file extention.
 */

#ifndef LDK_ASSET_H
#define LDK_ASSET_H

#include "../common.h"

#ifndef LDK_ASSET_FILE_EXTENTION_MAX_LENGTH
#define LDK_ASSET_FILE_EXTENTION_MAX_LENGTH 32
#endif

#ifndef LDK_ASSET_MAX_HANDLERS
#define LDK_ASSET_MAX_HANDLERS 32
#endif

#ifndef LDK_ASSET_INFO_INITIAL_CAPACITY
#define LDK_ASSET_INFO_INITIAL_CAPACITY 32
#endif

#ifdef __cplusplus
extern "C" {
#endif

  typedef LDKHandle (*LDKAssetHandlerLoadFunc) (const char* path);
  typedef void (*LDKAssetHandlerUnloadFunc) (LDKHandle handle);

  LDK_API bool ldkAssetInitialize();
  LDK_API void ldkAssetTerminate();
  LDK_API bool ldkAssetHandlerIsRegistered(const char* fileExtention);
  LDK_API bool ldkAssetHandlerRegister(LDKTypeId id, const char* fileExtention, LDKAssetHandlerLoadFunc loadFunc, LDKAssetHandlerUnloadFunc unloadFunc);
  LDK_API LDKHandle ldkAssetGet(const char* path);
  LDK_API void ldkAssetDispose(LDKHandle handle);


#ifdef __cplusplus
}
#endif

#endif //LDK_ASSET_H
