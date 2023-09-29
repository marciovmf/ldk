#include "ldk/os.h"
#include "ldk/engine.h"
#include "ldk/eventqueue.h"
#include "ldk/module/graphics.h"
#include "ldk/module/renderer.h"
#include "ldk/module/asset.h"
#include "ldk/asset/shader.h"
#include "ldk/asset/material.h"
#include <string.h>
#include <signal.h>

static struct
{
  bool running;
  int32 exitCode;
} internal = {0};

static void ldkOnSignal(int32 signal)
{
  const char* signalName = "Unknown signal";
  switch(signal)
  {
    case SIGABRT: signalName = (const char*) "SIGABRT"; break;
    case SIGFPE:  signalName = (const char*) "SIGFPE"; break;
    case SIGILL:  signalName = (const char*) "SIGILL"; break;
    case SIGINT:  signalName = (const char*) "SIGINT"; break;
    case SIGSEGV: signalName = (const char*) "SIGSEGV"; break;
    case SIGTERM: signalName = (const char*) "SIGTERM"; break;
  }

  ldkLogError("Signal %s\n", signalName);
  ldkOsStackTracePrint();
  ldkEngineStop(signal);
  ldkEngineTerminate();
}

void logModuleInit(const char* moduleName, bool success)
{
  ldkLogInfo("initializing %s", moduleName);
}

void logModuleTerminate(const char* moduleName)
{
  ldkLogInfo("Terminating %s", moduleName);
}

bool ldkEngineInitialize()
{
  signal(SIGABRT, ldkOnSignal);
  signal(SIGFPE, ldkOnSignal);
  signal(SIGILL, ldkOnSignal);
  signal(SIGINT, ldkOnSignal);
  signal(SIGSEGV, ldkOnSignal);
  signal(SIGTERM, ldkOnSignal);

  ldkOsInitialize();
  ldkOsCwdSetFromExecutablePath();

  bool success = true;
  bool stepSuccess;

  LDKDateTime  dateTime;
  ldkOsSystemDateTimeGet(&dateTime);
  char strDateTime[64];
  snprintf(strDateTime, 64, "---- %d.%d.%d %d:%d:%d - LDK Engine Startup ----",
      dateTime.year, dateTime.month, dateTime.day,
      dateTime.hour, dateTime.minute, dateTime.Second);

  // Startup Log
  stepSuccess = ldkLogInitialize("ldk.log", strDateTime);
  logModuleInit("Log", stepSuccess);
  success &= stepSuccess;

  // Register common types
  typeid(LDKHandle);

  // Startup Graphics
  stepSuccess = ldkGraphicsInitialize(LDK_GRAPHICS_API_OPENGL_3_3);
  logModuleInit("Graphics", stepSuccess);
  success &= stepSuccess;

  // Startup renderer
  stepSuccess = ldkRenderInitialize();
  logModuleInit("Render", stepSuccess);
  success &= stepSuccess;

  // Startup Asset Handlers
  stepSuccess = ldkAssetInitialize();
  stepSuccess &= ldkAssetHandlerRegister(typeid(LDKHShader), "vs", ldkShaderLoadFunc, ldkShaderUnloadFunc);
  stepSuccess &= ldkAssetHandlerRegister(typeid(LDKHShader), "fs", ldkShaderLoadFunc, ldkShaderUnloadFunc);
  stepSuccess &= ldkAssetHandlerRegister(typeid(LDKHShader), "gs", ldkShaderLoadFunc, ldkShaderUnloadFunc);
  stepSuccess &= ldkAssetHandlerRegister(typeid(LDKHShader), "material", ldkMaterialLoadFunc, ldkMaterialUnloadFunc);
  logModuleInit("Asset Handler", stepSuccess);

  success &= stepSuccess;
  logModuleInit("LDK is ready", stepSuccess);
  lkdGraphicsInfoPrint();

  return success;
}

void ldkEngineTerminate()
{
  logModuleTerminate("Asset Handler");
  ldkAssetTerminate();
  logModuleTerminate("Render");
  ldkRenderTerminate();
  logModuleTerminate("Graphics");
  ldkGraphicsTerminate();
  logModuleTerminate("Log\n ---");
  ldkLogTerminate();
  ldkOsTerminate();
}

LDK_API int32 ldkEngineRun()
{
  internal.running = true;
  while (internal.running)
  {
    LDKEvent event;
    while(ldkOsEventsPoll(&event))
    {
      ldkEventPush(&event);
    }

    event.type = LDK_EVENT_TYPE_FRAME;
    event.frameEvent.type = LDK_FRAME_EVENT_BEFORE_RENDER;
    event.frameEvent.ticks = ldkOsTimeTicksGet();
    ldkEventPush(&event);
    ldkEventQueueBroadcast();

    ldkGraphicsSwapBuffers();

    event.type = LDK_EVENT_TYPE_FRAME;
    event.frameEvent.type = LDK_FRAME_EVENT_AFTER_RENDER;
    event.frameEvent.ticks = ldkOsTimeTicksGet();
    ldkEventPush(&event);
    ldkEventQueueBroadcast();
  }

  return internal.exitCode;
}

void ldkEngineStop(int32 exitCode)
{
  internal.exitCode = exitCode;
  internal.running = false;
}
