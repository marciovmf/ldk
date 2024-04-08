/** editor.h
 * 
 */

#ifndef LDK_EDITOR_H
#define LDK_EDITOR_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API void ldkEditorEnable(bool enabled);
  LDK_API bool ldkEditorInitialize(void);
  LDK_API void ldkEditorTerminate(void);
  LDK_API void ldkEditorUpdate(float deltaTime);

#ifdef __cplusplus
}
#endif

#endif //LDK_EDITOR_H

