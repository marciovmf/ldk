/**
 * # Scene graph
 *
 * Maintain scene hierarchy and world-transform relationships.
 */

#ifndef LDK_MODULE_SCENEGRAPH_H
#define LDK_MODULE_SCENEGRAPH_H

#include <ldk_common.h>
#include <module/ldk_entity.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * @brief Update the scenegraph.
   * @param dt Elapsed time since the preceding update, in seconds.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_scenegraph_update(float dt);
  /**
   * @brief Update scene-graph state for an entity.
   * @param entity Entity to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_scenegraph_update_entity(LDKEntity entity);

  /**
   * @brief Set an entity parent in the scene graph.
   * @param child_entity Entity whose parent is being set.
   * @param parent_entity Parent entity to assign.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_scenegraph_set_parent(LDKEntity child_entity, LDKEntity parent_entity);
  /**
   * @brief Detach an entity from the scene graph.
   * @param entity Entity to inspect or modify.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_scenegraph_detach(LDKEntity entity);
  /**
   * @brief Read the scene-graph parent of an entity.
   * @param entity Entity to inspect or modify.
   * @return The requested value.
   */
  LDK_API LDKEntity ldk_scenegraph_get_parent(LDKEntity entity);

#ifdef __cplusplus
}
#endif

#endif
