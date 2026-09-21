/** @file ldk_instanced_mesh_source.h
 * @brief An explicitly instanced mesh with entity-local transforms.
 */
#ifndef LDK_INSTANCED_MESH_SOURCE_H
#define LDK_INSTANCED_MESH_SOURCE_H

#include <component/ldk_mesh_source.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //@component
  typedef struct LDKInstancedMeshSource
  {
    // Reuse mesh/material authoring and ownership; not a second ECS component.
    //@inspect hidden runtime
    LDKMeshSource source;
    //@inspect hidden runtime
    Mat4 *instances;
    //@inspect readonly runtime
    u32 instance_count;
    //@inspect hidden runtime
    u32 instance_capacity;
  } LDKInstancedMeshSource;

  /** Ensure capacity for at least count local transforms. Failure preserves the
   * current buffer and instance count. The component owns the buffer.
   */
  LDK_API bool ldk_instanced_mesh_source_reserve_instances(
      LDKInstancedMeshSource *source, u32 count);

  /** Resize the local transform set while retaining allocated capacity. Newly
   * exposed transforms are initialized to identity. Failure preserves the set.
   * The returned component buffer may be written by systems before rendering.
   */
  LDK_API bool ldk_instanced_mesh_source_resize_instances(
      LDKInstancedMeshSource *source, u32 count);

  /** Copy local transforms. Zero count clears the active set but retains
   * capacity. Failure preserves it. Use existing mesh-source setters on source
   * for mesh/material authoring.
   */
  LDK_API bool ldk_instanced_mesh_source_set_instances(
      LDKInstancedMeshSource *source, const Mat4 *instances, u32 count);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_instanced_mesh_source_component_desc(
      u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif
