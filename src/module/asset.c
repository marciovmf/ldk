#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/module/asset.h"
#include "ldk/arena.h"
#include <string.h>

#ifndef LDK_ASSET_FILE_EXTENTION_MAX_LENGTH
#define LDK_ASSET_FILE_EXTENTION_MAX_LENGTH 32
#endif

#ifndef LDK_ASSET_MAX_HANDLERS
#define LDK_ASSET_MAX_HANDLERS 32
#endif

#ifndef LDK_ASSET_INFO_INITIAL_CAPACITY
#define LDK_ASSET_INFO_INITIAL_CAPACITY 32
#endif

typedef struct
{
  char fileExtention[LDK_ASSET_FILE_EXTENTION_MAX_LENGTH];
  LDKAssetHandlerLoadFunc loadFunc;
  LDKAssetHandlerUnloadFunc unloadFunc;
  LDKTypeId assetTypeId;
  uint32 handlerId;     //Index of this handler on the handler list
} LDKAssetHandler;

typedef struct
{
  LDKPath   path;
  LDKTypeId assetType;
  LDKHandle handle;
  LDKHash   hash;
  uint64    loadTime;
  uint32    handlerId;    //Index of the handler on the handler list
} LDKAssetInfo;

static struct
{
  LDKAssetHandler handlers[LDK_ASSET_MAX_HANDLERS];
  LDKArena assetInfoList;           // List of assetInfo
  LDKArena recycledAssetInfoIndex;  // List of assetInfo slots to be reused
  uint32 numRecycled;               // How many recycled assetInfo entries are there
  uint32 numHandlers;
} internal = { 0 };

static LDKAssetHandler* internalAssetHandlerGet(const char* fileExtention)
{
  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    if(strncmp((char*) &internal.handlers[i].fileExtention, fileExtention, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH) == 0)
    {
      return &internal.handlers[i];
    }
  }

  return NULL;
}

static inline void internalAssetInfoRecycle(uint32 index)
{
  uint32* mem = (uint32*) ldkArenaAllocate(&internal.recycledAssetInfoIndex, sizeof(uint32));
  *mem = index;
  internal.numRecycled++;
}

static inline LDKAssetInfo* internalAssetInfoGetNewOrRecycled()
{
  if (internal.numRecycled == 0)
  {
    return (LDKAssetInfo*) ldkArenaAllocate(&internal.assetInfoList, sizeof(LDKAssetInfo));
  }

  uint32* indexArray = (uint32*) ldkArenaDataGet(&internal.recycledAssetInfoIndex);
  uint32 lastIndex = indexArray[internal.numRecycled - 1];
  ldkArenaFree(&internal.recycledAssetInfoIndex, sizeof(uint32));
  internal.numRecycled--;

  LDKAssetInfo* assetInfoArray = (LDKAssetInfo*) ldkArenaDataGet(&internal.assetInfoList);
  LDKAssetInfo* recycledAssetInfo = &assetInfoArray[lastIndex];
  return recycledAssetInfo;
}

bool ldkAssetInitialize()
{
  if (internal.assetInfoList.bufferSize != 0)
  {
    return false;
  }

  ldkArenaCreate(&internal.assetInfoList, LDK_ASSET_INFO_INITIAL_CAPACITY * sizeof(LDKAssetInfo));
  ldkArenaCreate(&internal.recycledAssetInfoIndex, LDK_ASSET_INFO_INITIAL_CAPACITY * sizeof(uint32));
  return true;
}

void ldkAssetTerminate()
{
  const uint32 numAssets = (uint32) (ldkArenaUsedGet(&internal.assetInfoList) / sizeof(LDKAssetInfo));
  LDKAssetInfo* assetInfo = (LDKAssetInfo*) ldkArenaDataGet(&internal.assetInfoList);

  // is it really necessary ?
  for(uint32 i = 0; i < numAssets; i++)
  {
    LDKAssetHandler* handler = &internal.handlers[assetInfo->handlerId];
    handler->unloadFunc(assetInfo->handle);
    assetInfo++;
  }

  ldkArenaDestroy(&internal.assetInfoList);
}

bool ldkAssetHandlerIsRegistered(const char* fileExtention)
{
  return internalAssetHandlerGet(fileExtention) != NULL;
}

bool ldkAssetHandlerRegister(LDKTypeId id, const char* fileExtention, LDKAssetHandlerLoadFunc loadFunc, LDKAssetHandlerUnloadFunc unloadFunc)
{
  LDKAssetHandler* handler = internalAssetHandlerGet(fileExtention);
  if (handler != NULL)
  {
    ldkLogError("A handler for type file extention '%s' is already registered and associated with '%s' types",
        fileExtention, ldkTypeName(handler->assetTypeId));
    return false;
  }

  if (internal.numHandlers >= (LDK_ASSET_MAX_HANDLERS - 1))
  {
    ldkLogError("Unable to register handler for file extention '%s'. The maximum number of handlers (%d) was reached.",
        fileExtention, LDK_ASSET_MAX_HANDLERS);
    return false;
  }

  if (strlen(fileExtention) >= LDK_ASSET_FILE_EXTENTION_MAX_LENGTH)
  {
    ldkLogError("Unable to register handler for file extention '%s'. The maximum length of an asset file extention is (%d).",
        fileExtention, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH);
    return false;
  }

  const uint32 handlerId = internal.numHandlers++;
  handler = &internal.handlers[handlerId];
  strncpy(handler->fileExtention, fileExtention, LDK_ASSET_FILE_EXTENTION_MAX_LENGTH);
  handler->assetTypeId = id;
  handler->loadFunc = loadFunc;
  handler->unloadFunc = unloadFunc;
  handler->handlerId = handlerId;
  return true;
}

LDKHandle ldkAssetGet(const char* path)
{
  LDKSubStr extention = ldkPathFileExtentionGetSubstring(path);
  LDKAssetHandler* handler = internalAssetHandlerGet(extention.ptr);
  const uint32 numAssets = (uint32) (ldkArenaUsedGet(&internal.assetInfoList) / sizeof(LDKAssetInfo));

  if (handler == NULL)
  {
    ldkLogError("There are no handlers for files with extention '%s'", extention);
    return LDK_HANDLE_INVALID;
  }

  // Find asset
  LDKHash hash = ldkHash(path);
  LDKAssetInfo* assetInfo = (LDKAssetInfo*) ldkArenaDataGet(&internal.assetInfoList);
  for (uint32 i = 0; i < numAssets; i++)
  {
    if (assetInfo->hash == hash && strncmp(path, assetInfo->path.path, assetInfo->path.length) == 0)
      return assetInfo->handle;

    assetInfo++;
  }

  // Add if not found
  //assetInfo = (LDKAssetInfo*) ldkArenaAllocate(&internal.assetInfoList, sizeof(LDKAssetInfo));
  ldkLogInfo("Loading asset %s", path);
  assetInfo = internalAssetInfoGetNewOrRecycled();
  ldkPath(&assetInfo->path, path);
  assetInfo->hash = hash;
  assetInfo->loadTime = ldkOsTimeTicksGet();
  assetInfo->assetType = handler->assetTypeId;
  assetInfo->handle = handler->loadFunc(path);
  assetInfo->handlerId = handler->handlerId;
  return assetInfo->handle;
}

void ldkAssetDispose(LDKHandle handle)
{
  LDKAssetInfo* assetInfo = (LDKAssetInfo*) ldkArenaDataGet(&internal.assetInfoList);
  const uint32 numAssets = (uint32) (ldkArenaUsedGet(&internal.assetInfoList) / sizeof(LDKAssetInfo));
  for (uint32 i = 0; i < numAssets; i++)
  {
    if (assetInfo->handle == handle)
    {
      LDKAssetHandler* handler = &internal.handlers[assetInfo->handlerId];
      LDK_ASSERT(handler->handlerId == assetInfo->handlerId);
      handler->unloadFunc(handle);
      memset(assetInfo, 0, sizeof(LDKAssetInfo));
      internalAssetInfoRecycle(i);
      break;
    }
    assetInfo++;
  }
}

