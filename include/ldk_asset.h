/**
 * # Asset identifiers
 *
 * Common typed asset handles and identifiers used by the LDK asset manager.
 */

#ifndef LDK_ASSET_H
#define LDK_ASSET_H

#include <ldk_common.h>
#include <ldk_package.h>
#include <stdx/stdx_hpool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group core.assets */
  typedef XHandle LDKHandle;

/** @group core.assets */
#define LDK_ASSET_PATH_MAX_LENGTH LDK_PACKAGE_PATH_MAX_LENGTH

  /** @group core.assets */
  typedef struct LDKAssetPath
  {
    char buf[LDK_ASSET_PATH_MAX_LENGTH + 1u];
    size_t length;
  } LDKAssetPath;

  /** @group core.assets */
  typedef enum LDKAssetType
  {
    LDK_ASSET_TYPE_NULL = 0,
    LDK_ASSET_TYPE_TEXT_FILE = 1,
    LDK_ASSET_TYPE_FONT = 2,
    LDK_ASSET_TYPE_IMAGE = 3,
    LDK_ASSET_TYPE_MESH = 4,
    LDK_ASSET_TYPE_MATERIAL = 5,
    LDK_ASSET_TYPE_SKYBOX = 6,
    LDK_ASSET_TYPE_AUDIO = 7,
    LDK_ASSET_TYPE_KEYFRAME_ANIMATION = 8,
    LDK_ASSET_TYPE_TEXTURE_ATLAS = 9
  } LDKAssetType;

  /** @group core.assets */
  typedef struct LDKAssetHandle
  {
    LDKHandle h;
  } LDKAssetHandle;

  /** @group core.assets */
  typedef struct LDKAssetTextFile
  {
    LDKHandle h;
  } LDKAssetTextFile;

  /** @group core.assets */
  typedef struct LDKAssetImage
  {
    LDKHandle h;
  } LDKAssetImage;

  /** @group core.assets */
  typedef struct LDKAssetFont
  {
    LDKHandle h;
  } LDKAssetFont;

  /** @group core.assets */
  typedef struct LDKAssetMaterial
  {
    LDKHandle h;
  } LDKAssetMaterial;

  /** @group core.assets */
  typedef struct LDKAssetMesh
  {
    LDKHandle h;
  } LDKAssetMesh;

  /** @group core.assets */
  typedef struct LDKAssetSkybox
  {
    LDKHandle h;
  } LDKAssetSkybox;

  /** @group core.assets */
  typedef struct LDKAssetKeyframeAnimation
  {
    LDKHandle h;
  } LDKAssetKeyframeAnimation;

  /** @group core.assets */
  typedef struct LDKAssetTextureAtlas
  {
    LDKHandle h;
  } LDKAssetTextureAtlas;

  /** @group core.assets */
  typedef struct LDKAssetAudio
  {
    LDKHandle h;
  } LDKAssetAudio;

#ifdef __cplusplus
}
#endif

#endif // LDK_ASSET_H
