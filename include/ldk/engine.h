/**
 *
 * engine.h
 * 
 * This header provides high a level abstraction for the overall architecture
 * of a game or application made with LDK engine.
 *
 * Engine initialization - Engine initialization will also initialize engine
 * modules, register handlers for common asset types and register types withing
 * the type system.
 * 
 * Engine run/stop  - ldkEngineRun() will assume control of the calling thread
 * and will automatically poll events and dispatch them for registered event
 * handlers. The event loop can be stopped by calling ldkenginStop() from any
 * event handler.
 */

#ifndef LDK_ENGINE_H
#define LDK_ENGINE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

  LDK_API void ldkEngineSetTimeScale(float scale);
  LDK_API float ldkEngineGetTimeScale();

  LDK_API bool ldkEngineInitialize(void);
  LDK_API void ldkEngineTerminate(void);
  LDK_API int32 ldkEngineRun(void);
  LDK_API void ldkEngineStop(int32);
#ifdef LDK_EDITOR
  LDK_API bool ldkEngineIsEditorRunning(void);
#endif


#endif //LDK_ENGINE_H

#ifdef __cplusplus
}
#endif

