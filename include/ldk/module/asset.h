#ifndef ASSET_H
#define ASSET_H

#include "../common.h"

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

#endif //ASSET_H
