#ifndef LDK_MESH_H
#define LDK_MESH_H

#include <ldk_common.h>
#include <stdx/stdx_math.h>

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

#ifdef __cplusplus
}
#endif

#endif
