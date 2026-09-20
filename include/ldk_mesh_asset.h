#ifndef LDK_MESH_ASSET_H
#define LDK_MESH_ASSET_H

#include <module/ldk_asset_manager.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKMeshAssetResult
  {
    char error[256];
  } LDKMeshAssetResult;

  /*
   * Loads the current LDK text mesh format. One physical .mesh file is one
   * LDKAssetMesh and may contain multiple indexed Meshes plus an authored node
   * hierarchy. MeshSource stores the asset handle and a mesh index separately.
   *
   *   version 4.0                         version 4.1
   *   vertex_format STATIC               vertex_format STATIC_TANGENT
   *   mesh_count <count>
   *   node_count <count>
   *
   *   mesh <index> "Name"
   *   vertex_count <count>
   *   index_count <count>
   *   material_slot_count <count>
   *   submesh_count <count>
   *   material_slot <index> "Name"
   *   vertex <px> <py> <pz> <nx> <ny> <nz> <u> <v> <0xRRGGBBAA>
   *   # STATIC_TANGENT appends <tx> <ty> <tz> <handedness>
   *   index_list <i0> <i1> ...
   *   submesh <first_index> <index_count> <material_slot>
   *   end_mesh
   *
   *   node <index> "Name"
   *   parent <parent_index | -1>
   *   mesh <mesh_index | -1>
   *   position <x> <y> <z>
   *   rotation <x> <y> <z> <w>
   *   scale <x> <y> <z>
   *   end_node
   *
   * Index data may use multiple index_list lines. Comments begin with '#'.
   * STATIC remains the 4.0 compatibility layout and initializes tangent to
   * zero. STATIC_TANGENT stores tangent.xyz plus handedness in tangent.w; a
   * zero handedness means tangent space is unavailable for that vertex. Colors
   * are canonical RRGGBBAA and converted to the runtime rgba32 representation.
   *
   * Parent nodes must appear before their children. A node may reference no
   * Mesh, which preserves Blender empties/pivots. Multiple nodes may reference
   * the same Mesh index.
   *
   * Reuses an asset already cached for the normalized absolute file path, or
   * loads the file once. Shared Mesh assets live until manager clear/
   * termination; callers must not individually unload them. No automatic hot
   * reload occurs.
   */
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_load_shared(
      LDKAssetManager *manager, const char *path,
      LDKMeshAssetResult *result);

  /* Mesh discovery. Names are descriptive; indices are the authored/runtime
   * references used by MeshSource and nodes. Programmatic/built-in assets are
   * exposed as one implicit Mesh at index zero. */
  LDK_API u32 ldk_asset_manager_mesh_count(
      LDKAssetManager *manager, LDKAssetMesh asset);
  LDK_API const char *ldk_asset_manager_mesh_name(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index);
  LDK_API const LDKMeshData *ldk_asset_manager_mesh_data_at(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index);
  LDK_API u32 ldk_asset_manager_mesh_material_slot_count(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index);
  LDK_API const char *ldk_asset_manager_mesh_material_slot_name(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index,
      u32 material_slot);
  LDK_API u32 ldk_asset_manager_mesh_submesh_count(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index);
  LDK_API const LDKMeshSubmesh *ldk_asset_manager_mesh_submesh_at(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index,
      u32 submesh_index);

  /* Authored hierarchy discovery. Built-in/programmatic assets have no
   * nodes. */
  LDK_API u32 ldk_asset_manager_mesh_node_count(
      LDKAssetManager *manager, LDKAssetMesh asset);
  LDK_API const LDKMeshNode *ldk_asset_manager_mesh_node_at(
      LDKAssetManager *manager, LDKAssetMesh asset, u32 node_index);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MESH_ASSET_H */
