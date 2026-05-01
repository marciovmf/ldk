#ifndef LDK_COMPONENT_MESH_SOURCE_H
#define LDK_COMPONENT_MESH_SOURCE_H

#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_component.h>

typedef uintptr_t LDKRendererMeshHandle;
#define LDK_RENDERER_MESH_INVALID ((LDKRendererMeshHandle)0)

typedef struct LDKMeshSource
{
  LDKAssetMesh source_asset;
  u32 version;
  uintptr_t renderer_mesh;
  u32 renderer_version;
} LDKMeshSource;

LDK_API bool ldk_mesh_source_register(LDKComponentRegistry* registry, u32 initial_capacity);

LDK_API LDKMeshSource ldk_mesh_source_make_default(void);
LDK_API bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset);

LDK_API bool ldk_mesh_source_attach(LDKEntity entity, const LDKMeshSource* initial_value);
LDK_API bool ldk_mesh_source_detach(LDKEntity entity);
LDK_API bool ldk_mesh_source_get(LDKEntity entity, LDKMeshSource* out_mesh_source);
LDK_API bool ldk_mesh_source_set(LDKEntity entity, const LDKMeshSource* mesh_source);
LDK_API bool ldk_mesh_source_set_asset(LDKEntity entity, LDKAssetMesh asset);

#endif //LDK_COMPONENT_MESH_SOURCE_H
