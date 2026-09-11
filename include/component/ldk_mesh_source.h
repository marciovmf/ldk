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

  typedef struct LDKMeshSourceMaterialBinding
  {
    LDKMaterialDesc material;
    LDKAssetMaterial material_asset;
    u64 material_revision;
    LDKResourceMaterial renderer_material;
    LDKResourceTexture renderer_texture;
    bool material_dirty;
  } LDKMeshSourceMaterialBinding;

  //@component
  typedef struct LDKMeshSource
  {
    LDKAssetMesh source_asset;
    //@inspect hidden
    LDKMaterialDesc material;
    //@inspect hidden
    LDKAssetMaterial material_asset;
    //@inspect hidden
    u64 material_revision; /* Zero means inline material; otherwise asset-bound. */
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
    //@inspect hidden
    LDKMeshSourceMaterialBinding* additional_materials;
    //@inspect hidden
    u32 material_count;
  } LDKMeshSource;

  LDK_API bool ldk_mesh_source_set_data(
      LDKMeshSource* mesh_source, LDKAssetMesh asset);

  /* Resize material bindings to match the selected Mesh's material slots. */
  LDK_API bool ldk_mesh_source_materials_sync(
      LDKMeshSource* mesh_source, LDKAssetManager* assets);
  LDK_API u32 ldk_mesh_source_material_count(
      const LDKMeshSource* mesh_source);

  /* Slots greater than zero are stored here. Slot zero uses the historical
   * material/material_asset/runtime fields directly on LDKMeshSource. */
  LDK_API LDKMeshSourceMaterialBinding*
      ldk_mesh_source_additional_material_binding(
          LDKMeshSource* mesh_source, u32 material_slot);
  LDK_API const LDKMeshSourceMaterialBinding*
      ldk_mesh_source_additional_material_binding_const(
          const LDKMeshSource* mesh_source, u32 material_slot);

  /* Assign an inline descriptor. The historical helper addresses slot zero. */
  LDK_API bool ldk_mesh_source_set_material(
      LDKMeshSource* mesh_source,
      LDKMaterialDesc const* material);
  LDK_API bool ldk_mesh_source_set_material_at(
      LDKMeshSource* mesh_source,
      u32 material_slot,
      LDKMaterialDesc const* material);

  /* Binding validates the asset and copies its current descriptor. */
  LDK_API bool ldk_mesh_source_set_material_asset(LDKMeshSource* mesh_source,
      LDKAssetManager* assets, LDKAssetMaterial asset);
  LDK_API bool ldk_mesh_source_set_material_asset_at(
      LDKMeshSource* mesh_source, LDKAssetManager* assets,
      u32 material_slot, LDKAssetMaterial asset);

  /* Refresh authored values after shared edits, without invalidating geometry. */
  LDK_API bool ldk_mesh_source_material_sync(LDKMeshSource* mesh_source,
      LDKAssetManager* assets);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_mesh_source_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_MESH_SOURCE_H
