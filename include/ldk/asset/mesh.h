#ifndef LDK_MESH_H
#define LDK_MESH_H

#include "common.h"
#include "maths.h"
#include "material.h"
#include "../module/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16 first;
  uint16 count;
  uint16 materialIndex;
} LDKSurface;

typedef struct
{
  LDKVertexBuffer vBuffer;
  float*  vertices;
  uint16* indices;
  LDKHMaterial* materials;
  LDKSurface* surfaces;
  uint32 numVertices;
  uint32 numIndices;
  uint32 numMaterials;
  uint32 numSurfaces;
} LDKMesh;


LDK_API LDKHMesh ldkAssetMeshLoadFunc(const char* path);
LDK_API void ldkAssetMeshUnloadFunc(LDKHMesh handle);
LDK_API LDKVertexBuffer ldkAssetMeshGetVertexBuffer(LDKHMesh handle);
LDK_API LDKMesh* ldkAssetMeshGetPointer(LDKHMesh handle);

#ifdef __cplusplus
}
#endif
#endif //LDK_MESH_H

