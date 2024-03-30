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

#define LDK_MATERIAL_MAX_TEXTURES 8

  typedef enum
  {
    LDK_MATERIAL_PARAM_TYPE_INT     = 0,
    LDK_MATERIAL_PARAM_TYPE_UINT    = 1,
    LDK_MATERIAL_PARAM_TYPE_FLOAT   = 2,
    LDK_MATERIAL_PARAM_TYPE_VEC2    = 3,
    LDK_MATERIAL_PARAM_TYPE_VEC3    = 4,
    LDK_MATERIAL_PARAM_TYPE_VEC4    = 5,
    LDK_MATERIAL_PARAM_TYPE_MAT4    = 6,
    LDK_MATERIAL_PARAM_TYPE_TEXTURE = 7,
  }LDKMaterialParamType;

  typedef struct
  {
    LDKSmallStr name;
    LDKMaterialParamType type;

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
    LDKMaterialParam param[LDK_SHADER_MAX_PARAMS];
    LDKHandle textures[LDK_SHADER_MAX_PARAMS];
    uint32 numParam;
    uint32 numTextures;
  } LDKMaterial;

  LDK_API bool ldkAssetMaterialLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetMaterialUnloadFunc(LDKAsset asset);
  LDK_API LDKMaterial* ldkMaterialClone(LDKHandle hMaterial);


#ifdef __cplusplus
}
#endif
#endif // LKD_MATERIAL_H

