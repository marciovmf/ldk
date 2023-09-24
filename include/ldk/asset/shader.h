#ifndef SHADER_H
#define SHADER_H

#include "../common.h"
#include "../module/render.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDKHShader ldkShaderLoadFunc(const char* path);
  void ldkShaderUnloadFunc(LDKHShader handle);

#ifdef __cplusplus
}
#endif

#endif  // SHADER_H
