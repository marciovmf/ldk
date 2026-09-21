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
  } LDKInstancedMeshSource;

  /** Copy local transforms. Zero count clears the set. Failure preserves it.
   * The component owns the copy; do not free or replace instances directly.
   * Use existing mesh-source setters on source for mesh/material authoring.
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
