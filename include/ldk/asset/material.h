/**
 *
 * material.h
 * 
 * Asset handler for .material asset files.
 */

#ifndef LDK_MATERIAL_H
#define LDK_MATERIAL_H

#include "../common.h"
#include "../maths.h"
#include "../module/asset.h"
#include "../asset/shader.h"
#include "../asset/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LDK_MATERIAL_MAX_PARAMS
#define LDK_MATERIAL_MAX_PARAMS 8
#endif

#define LDK_MATERIAL_MAX_TEXTURES LDK_MATERIAL_MAX_PARAMS

  typedef struct
  {
    uint32 type;
    int32 size;
    uint32 location;
  } LDKGLMaterialParamInfo;

  typedef struct
  {
    LDKGLMaterialParamInfo gl;
    LDKSmallStr name;
    union
    {
      uint32    textureIndexValue;
      int       intValue;
      uint32    uintValue;
      float     floatValue;
      Vec2      vec2Value;
      Vec3      vec3Value;
      Vec4      vec4Value;
      Mat4      mat4Value;
    };
  } LDKMaterialParam;

  typedef struct
  {
    LDK_DECLARE_ASSET;
    const char* name;
    LDKHandle program;
    LDKMaterialParam param[LDK_MATERIAL_MAX_PARAMS];
    LDKHandle textures[LDK_MATERIAL_MAX_TEXTURES];
    uint32 numParam;
    uint32 numTextures;
  } LDKMaterial;

  LDK_API bool ldkAssetMaterialLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetMaterialUnloadFunc(LDKAsset asset);

#ifdef __cplusplus
}
#endif
#endif // LKD_MATERIAL_H

