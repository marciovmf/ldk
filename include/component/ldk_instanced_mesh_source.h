/**
 * # Instanced mesh component
 *
 * An explicitly instanced mesh with entity-local transforms.
 */

#ifndef LDK_INSTANCED_MESH_SOURCE_H
#define LDK_INSTANCED_MESH_SOURCE_H

#include <component/ldk_mesh_source.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group ldk.component.instanced_mesh_source */
  //@component
  typedef struct LDKInstancedMeshSource
  {
    // Reuse mesh/material authoring and ownership; not a second ECS component.
    //@inspect hidden runtime
    LDKMeshSource source;
    //@inspect hidden runtime
    Mat4 *instances;
    //@inspect hidden runtime
    u32 *instance_colors;
    //@inspect readonly runtime
    u32 instance_count;
    //@inspect hidden runtime
    u32 instance_capacity;
  } LDKInstancedMeshSource;

  /** Ensure capacity for at least count local transforms. Failure preserves the
   * current buffer and instance count. The component owns the buffer.
   * @param source Source object or source data for this operation.
   * @param count Number of items to process.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.instanced_mesh_source
   */
  LDK_API bool ldk_instanced_mesh_source_reserve_instances(
      LDKInstancedMeshSource *source, u32 count);

  /** Resize the local transform set while retaining allocated capacity. Newly
   * exposed transforms are identity and their colors are opaque white. Failure
   * preserves the set. Systems may write the component buffers before render.
   * @param source Source object or source data for this operation.
   * @param count Number of items to process.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.instanced_mesh_source
   */
  LDK_API bool ldk_instanced_mesh_source_resize_instances(
      LDKInstancedMeshSource *source, u32 count);

  /** Copy local transforms. Newly exposed colors are opaque white. Zero count
   * clears the active set but retains capacity. Failure preserves it. Use
   * existing mesh-source setters on source for mesh/material authoring.
   * @param source Source object or source data for this operation.
   * @param instances Instances used by this operation.
   * @param count Number of items to process.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.instanced_mesh_source
   */
  LDK_API bool ldk_instanced_mesh_source_set_instances(
      LDKInstancedMeshSource *source, const Mat4 *instances, u32 count);

  /** Copy one RGBA tint per active instance. Colors use 0xRRGGBBAA. The
   * supplied count must match instance_count.
   * @param source Source object or source data for this operation.
   * @param colors Colors used by this operation.
   * @param count Number of items to process.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.component.instanced_mesh_source
   */
  LDK_API bool ldk_instanced_mesh_source_set_instance_colors(
      LDKInstancedMeshSource *source, const u32 *colors, u32 count);

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the instanced mesh source component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group ldk.component.instanced_mesh_source
   */
  LDK_API LDKComponentDesc ldk_instanced_mesh_source_component_desc(
      u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif
