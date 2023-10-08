#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/os.h"
#include "ldk/module/renderer.h"
#include <string.h>

LDKHMesh ldkAssetMeshLoadFunc(const char* path)
{
  float vertices[] = {
    // Position, Normal, Texcoord
    0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,//  0 - top right
    0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,//  1 - bottom right
    -0.5f, -0.5f, 0.0f,0.0f, 0.0f, 0.0f, 0.0f, 0.0f,//  2 - bottom left
    -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,// 3 - top left 
  };

  uint16 indices[] =
  { 
    0, 1, 3,   // first triangle
    1, 2, 3    // second triangle
  };

  const uint32 numIndices   = sizeof(indices) / sizeof(uint16);
  const uint32 numVertices  = sizeof(vertices) / sizeof(float);
  const uint32 numSurfaces  = 1;
  const uint32 numMaterials = 1;

  LDKMesh* mesh   = (LDKMesh*)  ldkOsMemoryAlloc(sizeof(LDKMesh));
  mesh->vertices  = (float*)    ldkOsMemoryAlloc(sizeof(vertices));
  mesh->indices   = (uint16*)   ldkOsMemoryAlloc(sizeof(indices));
  mesh->surfaces  = (LDKSurface*)ldkOsMemoryAlloc(sizeof(LDKSurface) * numSurfaces);
  mesh->materials = (LDKHMaterial*)ldkOsMemoryAlloc(sizeof(LDKHMaterial) * numMaterials);

  mesh->numVertices   = numVertices;
  mesh->numIndices    = numIndices;
  mesh->numMaterials  = numMaterials;
  mesh->numSurfaces   = numSurfaces;

  memcpy(mesh->vertices, vertices, sizeof(vertices));
  memcpy(mesh->indices, indices, sizeof(indices));
  mesh->surfaces[0].first = 0;
  mesh->surfaces[0].count = numIndices;
  mesh->surfaces[0].materialIndex = 0;

  mesh->materials[0] = ldkAssetGet("../runtree/default.material");

  mesh->surfaces[0].first = 0;
  mesh->surfaces[0].count = numIndices;
  mesh->vBuffer = ldkVertexBufferCreate(LDK_VERTEX_LAYOUT_PNU);
  ldkVertexBufferData(mesh->vBuffer, (void*) vertices, sizeof(vertices));
  ldkVertexBufferIndexData(mesh->vBuffer, indices, sizeof(indices));
  return (LDKHMesh) mesh;
}

LDKVertexBuffer ldkAssetMeshGetVertexBuffer(LDKHMesh handle)
{
  LDKMesh* mesh = (LDKMesh*) handle;
  return mesh->vBuffer;
}

void ldkAssetMeshUnloadFunc(LDKHMesh handle)
{
  LDKMesh* mesh = (LDKMesh*) handle;

  ldkVertexBufferDestroy(mesh->vBuffer);
  ldkOsMemoryFree(mesh->indices);
  ldkOsMemoryFree(mesh->vertices);
  ldkOsMemoryFree(mesh->materials);
  ldkOsMemoryFree(mesh->surfaces);
  ldkOsMemoryFree(mesh);
}


LDKMesh* ldkAssetMeshGetPointer(LDKHMesh handle)
{
  return (LDKMesh*) handle;
}
