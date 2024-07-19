#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/module/asset.h"
#include "ldk/hashmap.h"
#include "ldk/hlist.h"
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <vadefs.h>
#include <stdlib.h>

typedef struct
{
  char fileExtension[LDK_ASSET_HANDLER_MAX_EXTENSIONS][LDK_ASSET_FILE_EXTENTION_MAX_LENGTH];
  LDKAssetHandlerLoadFunc loadFunc;
  LDKAssetHandlerUnloadFunc unloadFunc;
  LDKHList assets;
  LDKTypeId assetTypeId;
  uint32 handlerId;     //Index of this handler on the handler list
  uint32 numExtensions;
} LDKAssetHandler;

static struct
{
  LDKAssetHandler handlers[LDK_ASSET_MAX_HANDLERS];
  LDKHashMap* assetMap;              // Maps asset names to asset handle
  uint32 numHandlers;
} internal = { 0 };

static LDKAssetHandler* internalAssetHandlerGet(const char* fileExtension)
{
  if (!fileExtension)
    return NULL;

  //TODO(marcio): Should I use a dictionary here ?
  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKAssetHandler* handler = &internal.handlers[i];

    for (uint32 extIndex = 0; extIndex < handler->numExtensions; extIndex++)
    {
      if(strncmp((char*) &handler->fileExtension[extIndex], fileExtension, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH) == 0)
      {
        return handler;
      }
    }
  }
  return NULL;
}

bool ldkAssetInitialize(void)
{
  internal.assetMap = ldkHashMapCreate((ldkHashMapHashFunc) ldkHashStr, ldkHashMapStrCompareFunc);
  bool success = internal.assetMap != NULL;
  return success;
}

void ldkAssetTerminate(void)
{
  for(uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKAssetHandler handler = internal.handlers[i];
    ldkHListDestroy(&handler.assets);
  }

  ldkHashMapDestroy(internal.assetMap);
}

bool ldkAssetHandlerIsRegistered(const char* fileExtension)
{
  return internalAssetHandlerGet(fileExtension) != NULL;
}

bool ldkAssetHandlerRegisterNew(LDKTypeId typeId, LDKAssetHandlerLoadFunc loadFunc, LDKAssetHandlerUnloadFunc unloadFunc, uint32 capacity, const char* ext, ...)
{
  if (internal.numHandlers >= (LDK_ASSET_MAX_HANDLERS - 1))
  {
    ldkLogError("Unable to register handler for  '%s'. The maximum number of handlers (%d) was reached.", ldkTypeName(typeId), LDK_ASSET_MAX_HANDLERS);
    return false;
  }

  for(uint32 i = 0; i < internal.numHandlers; i++)
  {
    if (internal.handlers[i].assetTypeId == typeId)
    {
      ldkLogError("A handler for Asset type '%s' is already registered", ldkTypeName(typeId));
      return false;
    }
  }

  va_list args;
  va_start(args, ext);
  bool success = true;

  const uint32 handlerId = internal.numHandlers;
  LDKAssetHandler* handler = &internal.handlers[handlerId];
  handler->assetTypeId = typeId;
  handler->loadFunc = loadFunc;
  handler->unloadFunc = unloadFunc;
  handler->handlerId = handlerId;
  handler->numExtensions = 0;

  const char* fileExtension = ext;

  while(1)
  {
    if (!fileExtension)
      break;

    LDKAssetHandler* existingHandler = internalAssetHandlerGet(ext);
    if (existingHandler)
    {
      ldkLogError("A handler for type file extention '%s' is already registered and associated with '%s' types",
          fileExtension, ldkTypeName(handler->assetTypeId));
      success = false;
      break;
    }

    if (strlen(fileExtension) >= LDK_ASSET_FILE_EXTENTION_MAX_LENGTH)
    {
      ldkLogError("Unable to register handler for file extention '%s'. The maximum length of an asset file extention is (%d).",
          fileExtension, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH);
      success = false;
      break;
    }

    if (handler->numExtensions >= LDK_ASSET_HANDLER_MAX_EXTENSIONS)
    {
      ldkLogError("Unable to register handler for file extention '%s'. The maximum number file extentions is per handler was reached (%d).",
          fileExtension, LDK_ASSET_HANDLER_MAX_EXTENSIONS);
      success = false;
      break;
    }

    uint32 extensionIndex = handler->numExtensions++;
    strncpy(handler->fileExtension[extensionIndex], fileExtension, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH);

    fileExtension = va_arg(args, const char*);
    if (!fileExtension)
      break;
  }

  if (success)
  {
    success &= ldkHListCreate(&handler->assets, typeId, typesize(typeId), capacity);
    if (success)
      internal.numHandlers++;
  }

  va_end(args);
  return success;
}

LDKAsset ldkAssetGetByType(LDKTypeId id, const char* path)
{
  LDKHandle handle = (LDKHandle) ldkHashMapGet(internal.assetMap, (void*) path);
  if (handle)
  {
    LDKHAsset assetHandle = ldkHandleCast(LDKHAsset, handle);
    return ldkAssetLookupType(id, assetHandle);
  }

  LDKSubStr extention = ldkPathFileExtentionGetSubstring(path);
  LDKAssetHandler* handler = internalAssetHandlerGet(extention.ptr);

  if (handler == NULL)
  {
    ldkLogError("There are no handlers for files with extention '%s'", extention);
    return NULL;
  }

  // Add if not found
  //
  LDKHAsset hAsset = ldkHandleCast(LDKHAsset, ldkHListReserve(&handler->assets));
  LDKAssetInfo* asset = (LDKAssetInfo*) ldkHListLookup(&handler->assets, toLDKHandle(hAsset));

  ldkPath(&asset->path, path);

  LDKHash hash = ldkHashStr(path);
  uint64 loadTime = ldkOsTimeTicksGet();

  // Set asset information so user code read it if necessary
  asset->hash = hash;
  asset->loadTime = loadTime;
  asset->assetType = handler->assetTypeId;
  asset->handle = hAsset;
  asset->handlerId = handler->handlerId;
  asset->isFile = true;

  // Call user code to initialize the asset
  bool success = handler->loadFunc(path, asset);

  // We set up everything again, in case user code has messed up anything
  asset->hash = hash;
  asset->loadTime = loadTime;
  asset->assetType = handler->assetTypeId;
  asset->handle = hAsset;
  asset->handlerId = handler->handlerId;
  asset->isFile = true;

  char* key = _strdup(path);
  ldkHashMapInsert(internal.assetMap, key, (void*) toLDKHandle(hAsset));

  if (!success)
    ldkLogError("Failed to load %s", path);
#ifdef LDK_DEBUG
  else
    ldkLogInfo("Loaded %s", path);
#endif

  return asset;
}

static void disposeHashmapKey(void* key, void* value)
{
  free(key);
}

void ldkAssetDispose(LDKAsset asset)
{
  if (!asset)
    return;

  LDKAssetInfo* assetInfo = (LDKAssetInfo*) asset;
  internal.handlers[assetInfo->handlerId].unloadFunc(asset);
  ldkHashMapRemoveWith(internal.assetMap, &assetInfo->path, disposeHashmapKey);
}

LDKAsset ldkAssetLookupType(LDKTypeId typeId, LDKHAsset handle)
{
  LDKTypeId handleType = ldkHandleType(toLDKHandle(handle));

  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKAssetHandler* handler = &internal.handlers[i];
    if (handler->assetTypeId != handleType)
      continue;
    return ldkHListLookup(&handler->assets, toLDKHandle(handle));
  }

  return NULL;
}

LDKAsset ldkAssetNewByType(LDKTypeId type)
{
  static uint32 counter = 0;

  // Find the handler for this type

  LDKAssetHandler* handler = NULL;
  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    if (internal.handlers[i].assetTypeId == type) 
    {
      handler = &internal.handlers[i];
      break;
    }
  }

  if (handler == NULL)
  {
    ldkLogError("There are no handlers for type '%s'", typename(type));
    return NULL;
  }

  LDKHAsset hAsset = ldkHandleCast(LDKHAsset,ldkHListReserve(&handler->assets));
  LDKAssetInfo* asset = (LDKAssetInfo*) ldkHListLookup(&handler->assets, toLDKHandle(hAsset));

  char path[255];
  sprintf(path, "%s#%d#%d", typename(type), handler->assets.count, counter++);
  ldkPath(&asset->path, path);

  LDKHash hash = ldkHashStr(path);
  uint64 loadTime = ldkOsTimeTicksGet();

  // Set asset information so user code read it if necessary
  asset->hash = hash;
  asset->loadTime = loadTime;
  asset->assetType = handler->assetTypeId;
  asset->handle = hAsset;
  asset->handlerId = handler->handlerId;
  asset->isFile = false;

  char* key = _strdup(path);
  ldkHashMapInsert(internal.assetMap, key, (void*) toLDKHandle(hAsset));

  return asset;
}
