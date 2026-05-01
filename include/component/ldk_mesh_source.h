#ifndef LDK_COMPONENT_MESH_SOURCE_H
#define LDK_COMPONENT_MESH_SOURCE_H

#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>

typedef uintptr_t LDKRendererMeshHandle;
#define LDK_RENDERER_MESH_INVALID ((LDKRendererMeshHandle)0)

typedef struct LDKMeshSource
{
  LDKAssetMesh source_asset;
  u32 version;
  uintptr_t renderer_mesh;
  u32 renderer_version;
} LDKMeshSource;

LDK_API LDKMeshSource ldk_mesh_source_make_default(void);
LDK_API bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset);

#endif //LDK_COMPONENT_MESH_SOURCE_H
