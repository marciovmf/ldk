/**
 * @file ldk_mesh_source.h
 * @brief Mesh source component data.
 */

#ifndef LDK_MESH_SOURCE_H
#define LDK_MESH_SOURCE_H

#include <ldk_common.h>
#include <ldk_material.h>
#include <ldk_mesh.h>
#include <ldk_resource.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

  //@component
  typedef struct LDKMeshSource
  {
    LDKAssetMesh source_asset;
    //@inspect hidden
    LDKMaterialDesc material;
    //@inspect hidden
    LDKResourceMesh renderer_mesh;
    //@inspect hidden
    LDKResourceMaterial renderer_material;
    //@inspect hidden
    LDKResourceTexture renderer_texture;
    //@inspect hidden
    struct LDKRenderer* renderer;
    //@inspect hidden
    bool dirty;
    //@inspect hidden
    bool material_dirty;
  } LDKMeshSource;

  LDK_API bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset);
  LDK_API bool ldk_mesh_source_set_material(
      LDKMeshSource* mesh_source,
      LDKMaterialDesc const* material);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_mesh_source_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_MESH_SOURCE_H
