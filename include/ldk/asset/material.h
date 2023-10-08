/**
 *
 * material.h
 * 
 * Functions for loading/unloading material assets
 */

#ifndef LDK_MATERIAL_H
#define LDK_MATERIAL_H

#include "../common.h"
#include "../module/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API LDKHMaterial ldkAssetMaterialLoadFunc(const char* path);
  LDK_API void ldkAssetMaterialUnloadFunc(LDKHMaterial handle);

#ifdef __cplusplus
}
#endif
#endif // LKD_MATERIAL_H

