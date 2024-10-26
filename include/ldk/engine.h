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

  /**
   * ldkEngineInitialize
   * 
   * The engie will change it's current working directory (CWD) to the runtreeDir then
   * loads the ldk.cfg file and Initializes the engine and all subsystems.
   *
   * @param runtreeDir Path to the runtree directory. If NULL, the CWD is not changed.
   * @return Returns true in case of sucessfull initialization of all engine subsystems or false if any initialization fails.
   */
  LDK_API bool ldkEngineInitialize(const char* runtreeDir);


  /**
   * ldkEngineTerminate
   * Terminates all subsystems and clean resources.
   */
  LDK_API void ldkEngineTerminate(void);


  /**
   * ldkEngineRun
   *
   * Runs the engine main loop. It serves as the a core function to manage the
   * main loop of a game or application by handling events, rendering and
   * updating application state.
   * This function blocks execution of the main thread.
   *
   * @return Returns the the exit status of the application.
   */
  LDK_API int32 ldkEngineRun(void);

  /**
   * ldkEngineStop
   *
   * ldkEngineStop is used to force ldkEngineRun to stop and return.
   */
  LDK_API void ldkEngineStop(int32);

#ifdef LDK_EDITOR
  LDK_API bool ldkEngineIsEditorRunning(void);
#endif


#endif //LDK_ENGINE_H

#ifdef __cplusplus
}
#endif

