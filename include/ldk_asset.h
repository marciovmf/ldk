#ifndef LDK_ASSET_H
#define LDK_ASSET_H

#include <ldk_common.h>
#include <stdx/stdx_hpool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef XHandle LDKHandle;

  typedef enum LDKAssetType
  {
    LDK_ASSET_TYPE_NULL = 0,
    LDK_ASSET_TYPE_TEXT_FILE = 1,
    LDK_ASSET_TYPE_FONT = 2,
    LDK_ASSET_TYPE_IMAGE = 3,
    LDK_ASSET_TYPE_MESH = 4
  } LDKAssetType;

  typedef struct LDKAssetHandle
  {
    LDKHandle h;
  } LDKAssetHandle;

  typedef struct LDKAssetTextFile
  {
    LDKHandle h;
  } LDKAssetTextFile;

  typedef struct LDKAssetImage
  {
    LDKHandle h;
  } LDKAssetImage;

  typedef struct LDKAssetFont
  {
    LDKHandle h;
  } LDKAssetFont;

  typedef struct LDKAssetMesh
  {
    LDKHandle h;
  } LDKAssetMesh;

#ifdef __cplusplus
}
#endif

#endif // LDK_ASSET_H
