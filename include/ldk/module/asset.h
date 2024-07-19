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

#ifndef LDK_ASSET_HANDLER_MAX_EXTENSIONS
#define LDK_ASSET_HANDLER_MAX_EXTENSIONS 8
#endif

#ifndef LDK_ASSET_INFO_INITIAL_CAPACITY
#define LDK_ASSET_INFO_INITIAL_CAPACITY 32
#endif

#ifdef __cplusplus
extern "C" {
#endif


  typedef struct
  {
    bool      isFile;
    LDKPath   path;
    LDKTypeId assetType;
    LDKHash   hash;
    uint64    loadTime;
    uint32    handlerId;    // Index of the handler on the handler list
    LDKHAsset handle;
  } LDKAssetInfo;

  /*
   * This macro must be included inside the declaration of an asset structure
   * to ensure the entity contains an LDKAssetInfo struct;
   */
#define LDK_DECLARE_ASSET LDKAssetInfo asset

#define ldkAssetGet(type, path) ((type*) ldkAssetGetByType(typeid(type), path))
#define ldkAssetNew(type) ((type*) ldkAssetNewByType(typeid(type)))
#define ldkAssetLookup(type, handle) (type*) ldkAssetLookupType(typeid(type), handle)

#define ldkAssetHandlerRegister(type, loadFunc, unloadFunc, capacity,  ...) ldkAssetHandlerRegisterNew(typeid(type), loadFunc, unloadFunc, capacity, __VA_ARGS__, NULL)

  typedef void* LDKAsset;
  typedef bool (*LDKAssetHandlerLoadFunc) (const char* path, LDKAsset asset);
  typedef void (*LDKAssetHandlerUnloadFunc) (LDKAsset asset);

  LDK_API bool ldkAssetInitialize(void);
  LDK_API void ldkAssetTerminate(void);
  LDK_API bool ldkAssetHandlerIsRegistered(const char* fileExtension);
  LDK_API bool ldkAssetHandlerRegisterNew(LDKTypeId id, LDKAssetHandlerLoadFunc loadFunc, LDKAssetHandlerUnloadFunc unloadFunc, uint32 capacity, const char* ext, ...);
  LDK_API LDKAsset ldkAssetGetByType(LDKTypeId typeId, const char* path);
  LDK_API void ldkAssetDispose(LDKAsset asset);
  LDK_API LDKAsset ldkAssetLookupType(LDKTypeId type, LDKHAsset handle);
  LDK_API LDKAsset ldkAssetNewByType(LDKTypeId type);


#ifdef __cplusplus
}
#endif

#endif //LDK_ASSET_H
