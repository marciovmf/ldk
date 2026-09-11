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
   * Loads the current LDK text mesh format:
   *
   *   version 2.0
   *   vertex_format STATIC
   *   vertex_count <count>
   *   index_count <count>          # must be a multiple of 3
   *   vertex <px> <py> <pz> <nx> <ny> <nz> <u> <v> <0xRRGGBBAA>
   *   index_list <i0> <i1> ...
   *
   * Vertex and index entries may span multiple lines. Comments begin with '#'.
   * STATIC maps directly to LDKMeshVertex. Colors are specified as canonical
   * RRGGBBAA values and converted to the runtime rgba32 representation.
   *
   * Reuses an existing cached mesh by normalized absolute path, or loads it
   * once. The returned asset is manager-owned until clear/termination; callers
   * must not unload it individually. No automatic file hot reload is performed.
   */
  LDK_API LDKAssetMesh ldk_asset_manager_mesh_load_shared(
      LDKAssetManager *manager, const char *path,
      LDKMeshAssetResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MESH_ASSET_H */
