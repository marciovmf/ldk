#ifndef LDK_ENGINE_H
#define LDK_ENGINE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API bool ldkEngineInitialize();
  LDK_API void ldkEngineTerminate();
  LDK_API int32 ldkEngineRun();
  LDK_API void ldkEngineStop(int32);

#endif //LDK_ENGINE_H

#ifdef __cplusplus
}
#endif

