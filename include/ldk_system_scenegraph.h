#ifndef LDK_SYSTEM_SCENEGRAPH_H
#define LDK_SYSTEM_SCENEGRAPH_H

#include <ldk_common.h>
#include <ldk_system.h>

#ifdef __cplusplus
extern "C" {
#endif

LDK_API bool ldk_scenegraph_update(float dt);
LDK_API const LDKSystemDesc* ldk_scenegraph_system_desc(void);

#ifdef __cplusplus
}
#endif

#endif
