/**
 * # Mesh source component
 *
 * Mesh source component data.
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

  /** @group ldk.component.mesh_source */
  typedef struct LDKMeshSourceMaterialBinding
  {
    LDKMaterialDesc material;
    LDKAssetMaterial material_asset;
    u64 material_revision;
    LDKResourceMaterial renderer_material;
    LDKResourceTexture renderer_texture;
    LDKResourceTexture renderer_normal_map;
    LDKResourceTexture renderer_specular_map;
    bool material_dirty;
  } LDKMeshSourceMaterialBinding;

  /** @group ldk.component.mesh_source */
  //@component
  typedef struct LDKMeshSource
  {
    LDKAssetMesh source_asset;
    u32 mesh_index;
    bool casts_shadows;
    /* Face the active render view in the color pass. */
    bool billboard;
    //@inspect hidden
    LDKMaterialDesc material;
    //@inspect hidden
    LDKAssetMaterial material_asset;
    //@inspect hidden
    /* Zero means inline material; otherwise asset-bound. */
    u64 material_revision;
    //@inspect hidden
    LDKResourceMesh renderer_mesh;
    //@inspect hidden
    LDKResourceMaterial renderer_material;
    //@inspect hidden
    LDKResourceTexture renderer_texture;
    //@inspect hidden runtime
    LDKResourceTexture renderer_normal_map;
    //@inspect hidden runtime
    LDKResourceTexture renderer_specular_map;
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

  /**
   * @brief Set mesh source data.
   * @param mesh_source Mesh source used by this operation.
   * @param asset Asset handle to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_data(
      LDKMeshSource* mesh_source, LDKAssetMesh asset);
  /**
   * @brief Set mesh source mesh index.
   * @param mesh_source Mesh source used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @param mesh_index Zero-based index of the mesh entry.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_mesh_index(
      LDKMeshSource* mesh_source, LDKAssetManager* assets, u32 mesh_index);

  /* Resize material bindings to match the selected Mesh's material slots. */
  /**
   * @brief Synchronize the mesh source materials.
   * @param mesh_source Mesh source used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_materials_sync(
      LDKMeshSource* mesh_source, LDKAssetManager* assets);
  /**
   * @brief Get the number of material slots in a mesh source.
   * @param mesh_source Mesh source used by this operation.
   * @return Number of matching entries.
   * @group ldk.component.mesh_source
   */
  LDK_API u32 ldk_mesh_source_material_count(
      const LDKMeshSource* mesh_source);

  /* Slots greater than zero are stored here. Slot zero uses the historical
   * material/material_asset/runtime fields directly on LDKMeshSource. */
  /**
   * @brief Get a mutable material binding for an additional mesh slot.
   * @param mesh_source Mesh source used by this operation.
   * @param material_slot Material slot used by this operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.component.mesh_source
   */
  LDK_API LDKMeshSourceMaterialBinding*
      ldk_mesh_source_additional_material_binding(
          LDKMeshSource* mesh_source, u32 material_slot);
  /**
   * @brief Get a read-only material binding for an additional mesh slot.
   * @param mesh_source Mesh source used by this operation.
   * @param material_slot Material slot used by this operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.component.mesh_source
   */
  LDK_API const LDKMeshSourceMaterialBinding*
      ldk_mesh_source_additional_material_binding_const(
          const LDKMeshSource* mesh_source, u32 material_slot);

  /* Assign an inline descriptor. The historical helper addresses slot zero. */
  /**
   * @brief Set mesh source material.
   * @param mesh_source Mesh source used by this operation.
   * @param material Material used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_material(
      LDKMeshSource* mesh_source,
      LDKMaterialDesc const* material);
  /**
   * @brief Assign a material descriptor to the specified mesh material slot.
   * @param mesh_source Mesh source used by this operation.
   * @param material_slot Material slot used by this operation.
   * @param material Material used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_material_at(
      LDKMeshSource* mesh_source,
      u32 material_slot,
      LDKMaterialDesc const* material);

  /* Binding validates the asset and copies its current descriptor. */
  /**
   * @brief Set mesh source material asset.
   * @param mesh_source Mesh source used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @param asset Asset handle to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_material_asset(LDKMeshSource* mesh_source,
      LDKAssetManager* assets, LDKAssetMaterial asset);
  /**
   * @brief Assign a material asset to the specified mesh material slot.
   * @param mesh_source Mesh source used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @param material_slot Material slot used by this operation.
   * @param asset Asset handle to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_set_material_asset_at(
      LDKMeshSource* mesh_source, LDKAssetManager* assets,
      u32 material_slot, LDKAssetMaterial asset);

  /* Refresh authored values after shared edits, without invalidating
   * geometry. */
  /**
   * @brief Synchronize the mesh source material.
   * @param mesh_source Mesh source used by this operation.
   * @param assets Asset manager used to resolve referenced assets.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.mesh_source
   */
  LDK_API bool ldk_mesh_source_material_sync(LDKMeshSource* mesh_source,
      LDKAssetManager* assets);

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the mesh source component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group ldk.component.mesh_source
   */
  LDK_API LDKComponentDesc ldk_mesh_source_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_MESH_SOURCE_H
