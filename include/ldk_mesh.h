#ifndef LDK_MESH_H
#define LDK_MESH_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

typedef struct LDKAssetManager LDKAssetManager;
typedef struct LDKAssetMesh LDKAssetMesh;

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKMeshVertex
  {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    u32 color;
  } LDKMeshVertex;

  typedef struct LDKMeshData
  {
    LDKMeshVertex* vertices;
    u32 vertex_count;
    u32* indices;
    u32 index_count;
  } LDKMeshData;

  typedef enum LDKMeshPrimitive
  {
    LDK_MESH_PRIMITIVE_CUBE = 0,
    LDK_MESH_PRIMITIVE_SPHERE,
    LDK_MESH_PRIMITIVE_CAPSULE,
    LDK_MESH_PRIMITIVE_PLANE,
    LDK_MESH_PRIMITIVE_QUAD,
    LDK_MESH_PRIMITIVE_COUNT
  } LDKMeshPrimitive;

  LDK_API bool ldk_mesh_primitive_create(
      LDKMeshPrimitive primitive, LDKMeshData* out_mesh);
  LDK_API void ldk_mesh_data_destroy(LDKMeshData* mesh);

  /**
   * Returns the shared asset for a built-in primitive mesh.
   *
   * The asset is created lazily and reused for subsequent requests while it
   * remains alive in the supplied asset manager.
   */
  LDK_API LDKAssetMesh ldk_mesh_primitive_asset_get(
      LDKAssetManager* manager, LDKMeshPrimitive primitive);

#ifdef __cplusplus
}
#endif

#endif
