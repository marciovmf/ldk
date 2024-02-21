/**
 * mesh.h
 *
 * Asset handler for .mesh asset files.
 */

#ifndef LDK_MESH_H
#define LDK_MESH_H

#include "common.h"
#include "maths.h"
#include "material.h"
#include "../module/asset.h"

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
    LDK_DECLARE_ASSET;
    LDKVertexBuffer   vBuffer;
    float*            vertices;
    uint16*           indices;
    LDKHandle*        materials;
    LDKSurface*       surfaces;
    LDKBoundingBox    boundingBox;
    LDKBoundingSphere boundingSphere;
    uint32            numVertices;
    uint32            numIndices;
    uint32            numMaterials;
    uint32            numSurfaces;
  } LDKMesh;


  LDK_API bool ldkAssetMeshLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetMeshUnloadFunc(LDKAsset asset);

#ifdef __cplusplus
}
#endif
#endif //LDK_MESH_H

