#ifndef LDK_MODULE_SCENEGRAPH_H
#define LDK_MODULE_SCENEGRAPH_H

#include <ldk_common.h>
#include <module/ldk_entity.h>

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API bool ldk_scenegraph_update(float dt);
  LDK_API bool ldk_scenegraph_update_entity(LDKEntity entity);

  LDK_API bool ldk_scenegraph_set_parent(LDKEntity child_entity, LDKEntity parent_entity);
  LDK_API bool ldk_scenegraph_detach(LDKEntity entity);
  LDK_API LDKEntity ldk_scenegraph_get_parent(LDKEntity entity);

#ifdef __cplusplus
}
#endif

#endif
