/**
 *
 * shader.h
 * 
 * Functions for loading/unloading shader assets
 */

#ifndef LDK_SHADER_H
#define LDK_SHADER_H

#include "../common.h"
#include "../module/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API LDKHShader ldkAssetShaderLoadFunc(const char* path);
  LDK_API void ldkAssetShaderUnloadFunc(LDKHShader handle);

#ifdef __cplusplus
}
#endif

#endif  // LDK_SHADER_H
