/**
 * @file ldk_mesh_source.h
 * @brief Mesh source component data.
 */

#ifndef LDK_MESH_SOURCE_H
#define LDK_MESH_SOURCE_H

#include <ldk_common.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef uintptr_t LDKRendererMeshHandle;

#define LDK_RENDERER_MESH_INVALID ((LDKRendererMeshHandle)0)

  typedef struct LDKMeshSource
  {
    LDKAssetMesh source_asset;
    u32 version;
    uintptr_t renderer_mesh;
    u32 renderer_version;
  } LDKMeshSource;

  LDK_API bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_mesh_source_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_MESH_SOURCE_H
