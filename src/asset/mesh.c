#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/os.h"
#include "ldk/module/renderer.h"
#include <string.h>

LDKHMesh ldkAssetMeshLoadFunc(const char* path)
{
  float vertices[] = {
    -1.0f,    1.0f,       -1.0f,  0.0f,     1.0f,       0.0f,   0.875001f,     0.500001f,
    1.0f,     1.0f,       1.0f,   0.0f,     1.0f,       0.0f,   0.625001f,     0.750001f,
    1.0f,     1.0f,       -1.0f,	0.0f,     1.0f,       0.0f,   0.625001f,     0.500001f,
    1.0f,     1.0f,       1.0f,   0.0f,     0.0f,       1.0f,   0.625001f,     0.750001f,
    -1.0f,    -1.0f,      1.0f,   0.0f,     0.0f,       1.0f,   0.375001f,     1.000001f,
    1.0f,     -1.0f,      1.0f,   0.0f,     0.0f,       1.0f,   0.375001f,     0.750001f,
    -1.0f,    1.0f,       1.0f,   -1.0f,    0.0f,       0.0f,   0.625001f,     0.000001f,
    -1.0f,    -1.0f,      -1.0f,	-1.0f,    0.0f,       0.0f,   0.375001f,     0.250001f,
    -1.0f,    -1.0f,      1.0f,   -1.0f,    0.0f,       0.0f,   0.375001f,     0.000001f,
    1.0f,     -1.0f,      -1.0f,	0.0f,     -1.0f,      0.0f,   0.375001f,     0.500001f,
    -1.0f,    -1.0f,      1.0f,   0.0f,     -1.0f,      0.0f,   0.125001f,     0.750001f,
    -1.0f,    -1.0f,      -1.0f,	0.0f,     -1.0f,      0.0f,   0.125001f,     0.500001f,
    1.0f,     1.0f,       -1.0f,	1.0f,     0.0f,       0.0f,   0.625001f,     0.500001f,
    1.0f,     -1.0f,      1.0f,   1.0f,     0.0f,       0.0f,   0.375001f,     0.750001f,
    1.0f,     -1.0f,      -1.0f,	1.0f,     0.0f,       0.0f,   0.375001f,     0.500001f,
    -1.0f,    1.0f,       -1.0f,	0.0f,     0.0f,       -1.0f,	0.625001f,     0.250001f,
    1.0f,     -1.0f,      -1.0f,	0.0f,     0.0f,       -1.0f,	0.375001f,     0.500001f,
    -1.0f,    -1.0f,      -1.0f,	0.0f,     0.0f,       -1.0f,	0.375001f,     0.250001f,
    -1.0f,    1.0f,       1.0f,   0.0f,     1.0f,       0.0f,   0.875001f,     0.750001f,
    -1.0f,    1.0f,       1.0f,   0.0f,     0.0f,       1.0f,   0.625001f,     1.000001f,
    -1.0f,    1.0f,       -1.0f,	-1.0f,    0.0f,       0.0f,   0.625001f,     0.250001f,
    1.0f,     -1.0f,      1.0f,   0.0f,     -1.0f,      0.0f,   0.375001f,     0.750001f,
    1.0f,     1.0f,       1.0f,   1.0f,     0.0f,       0.0f,   0.625001f,     0.750001f,
    1.0f,     1.0f,       -1.0f,	0.0f,     0.0f,       -1.0f,	0.625001f,     0.500001f
  };
  uint16 indices[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,0,18,1,3,19,4,6,20,7,9,21,10,12,22,13,15,23,16 };

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
