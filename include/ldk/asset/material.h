#ifndef MATERIAL_H
#define MATERIAL_H

#include "../common.h"
#include "../module/render.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API LDKHMaterial ldkMaterialLoadFunc(const char* path);
  LDK_API void ldkMaterialUnloadFunc(LDKHMaterial handle);

#ifdef __cplusplus
}
#endif
#endif //MATERIAL_H

