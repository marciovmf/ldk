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
   * Loads the current LDK text mesh format. One file may contain multiple
   * named Meshes. Each Mesh is exposed as an individual LDKAssetMesh handle;
   * sibling handles keep the same physical asset_path and are distinguished by
   * LDKAssetMeshData::name/source_mesh_index.
   *
   *   version 3.0
   *   vertex_format STATIC
   *   mesh_count <count>
   *
   *   mesh "Name"
   *   vertex_count <count>
   *   index_count <count>
   *   material_slot_count <count>
   *   submesh_count <count>
   *   material_slot <index> "Name"
   *   vertex <px> <py> <pz> <nx> <ny> <nz> <u> <v> <0xRRGGBBAA>
   *   index_list <i0> <i1> ...
   *   submesh <first_index> <index_count> <material_slot>
   *   end_mesh
   *
   * Vertex and index entries may span multiple lines. Comments begin with '#'.
   * STATIC maps directly to LDKMeshVertex. Colors are canonical RRGGBBAA and
   * converted to the runtime rgba32 representation.
   *
   * Reuses all Meshes already cached for the normalized absolute file path, or
   * loads the file once. The returned handle identifies the first Mesh in the
   * file. Shared Meshes live until manager clear/termination; callers must not
   * individually unload them. No automatic file hot reload is performed.
   */
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_load_shared(
      LDKAssetManager *manager, const char *path,
      LDKMeshAssetResult *result);

  /* Resolve another Mesh contained by the same physical .mesh file. */
  LDK_API u32 ldk_asset_manager_mesh_file_mesh_count(
      LDKAssetManager *manager, LDKAssetMesh mesh);
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_file_mesh_at(
      LDKAssetManager *manager, LDKAssetMesh mesh, u32 index);
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_file_mesh_find(
      LDKAssetManager *manager, LDKAssetMesh mesh, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MESH_ASSET_H */
