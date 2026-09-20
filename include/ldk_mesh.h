#ifndef LDK_MESH_H
#define LDK_MESH_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <stdx/stdx_math.h>

typedef struct LDKAssetManager LDKAssetManager;

#ifdef __cplusplus
extern "C" {
#endif

#define LDK_MESH_NAME_CAPACITY 128u
#define LDK_MESH_NODE_NAME_CAPACITY 128u
#define LDK_MESH_MATERIAL_SLOT_NAME_CAPACITY 128u
#define LDK_MESH_NODE_NONE (-1)
#define LDK_MESH_INDEX_NONE (-1)

  typedef struct LDKMeshVertex
  {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    u32 color;
    /* Tangent xyz plus handedness in w. w == 0 means unavailable. */
    Vec4 tangent;
  } LDKMeshVertex;

  typedef struct LDKMeshData
  {
    LDKMeshVertex* vertices;
    u32 vertex_count;
    u32* indices;
    u32 index_count;
    /* True when tangent storage was authored/initialized for this mesh. */
    bool has_tangents;
  } LDKMeshData;

  typedef struct LDKMeshSubmesh
  {
    u32 first_index;
    u32 index_count;
    u32 material_slot;
  } LDKMeshSubmesh;

  typedef struct LDKMeshMaterialSlot
  {
    char name[LDK_MESH_MATERIAL_SLOT_NAME_CAPACITY];
  } LDKMeshMaterialSlot;

  typedef struct LDKMeshNode
  {
    char name[LDK_MESH_NODE_NAME_CAPACITY];
    i32 parent_index;
    i32 mesh_index;
    Vec3 local_position;
    Quat local_rotation;
    Vec3 local_scale;
  } LDKMeshNode;

  typedef enum LDKMeshPrimitive
  {
    LDK_MESH_PRIMITIVE_CUBE = 0,
    LDK_MESH_PRIMITIVE_CONE,
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
