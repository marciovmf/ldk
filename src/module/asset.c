#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/module/asset.h"
#include "ldk/arena.h"
#include "ldk/hashmap.h"
#include "ldk/hlist.h"
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <vadefs.h>

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
    return ldkAssetLookupType(id, handle);
  }

  LDKSubStr extention = ldkPathFileExtentionGetSubstring(path);
  LDKAssetHandler* handler = internalAssetHandlerGet(extention.ptr);

  if (handler == NULL)
  {
    ldkLogError("There are no handlers for files with extention '%s'", extention);
    return NULL;
  }

  // Add if not found
  LDKHandle hAsset = ldkHListReserve(&handler->assets);
  LDKAssetInfo* asset = (LDKAssetInfo*) ldkHListLookup(&handler->assets, hAsset);

  ldkPath(&asset->path, path);

  LDKHash hash = ldkHashStr(path);
  uint64 loadTime = ldkOsTimeTicksGet();


  // Set asset information so user code read it if necessary
  asset->hash = hash;
  asset->loadTime = loadTime;
  asset->assetType = handler->assetTypeId;
  asset->handle = hAsset;
  asset->handlerId = handler->handlerId;

  // Call user code to initialize the asset
  handler->loadFunc(path, asset);

  // We set up everything again, in case user code has messed up anything
  asset->hash = hash;
  asset->loadTime = loadTime;
  asset->assetType = handler->assetTypeId;
  asset->handle = hAsset;
  asset->handlerId = handler->handlerId;

  char* key = _strdup(path);
  ldkHashMapInsert(internal.assetMap, key, (void*) hAsset);

  ldkLogInfo("%s %s", asset->handle != LDK_HANDLE_INVALID ? "Loaded" : "Failed to load" ,path);
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


LDKAsset ldkAssetLookupType(LDKTypeId typeId, LDKHandle handle)
{
  LDKTypeId handleType = ldkHandleType(handle);

  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKAssetHandler* handler = &internal.handlers[i];
    if (handler->assetTypeId != handleType)
      continue;

    return ldkHListLookup(&handler->assets, handle);
  }

  return NULL;
}


