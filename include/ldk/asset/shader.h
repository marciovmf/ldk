/**
 *
 * shader.h
 *
 * Asset handler for .shader asset files.
 * 
 */

#ifndef LDK_SHADER_H
#define LDK_SHADER_H

#include "../common.h"
#include "../module/asset.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef LDK_SHADER_MAX_PARAMS
#define LDK_SHADER_MAX_PARAMS 8  
#endif

  typedef struct 
  {
    uint32 id;
  } LDKGLShader;

  typedef struct
  {
    LDK_DECLARE_ASSET;
    union
    {
      LDKGLShader gl;
    };
  }LDKShader;

  LDK_API bool ldkAssetShaderLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetShaderUnloadFunc(LDKAsset handle);

#ifdef __cplusplus
}
#endif

#endif  // LDK_SHADER_H
