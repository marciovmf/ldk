#include "ldk/os.h"
#include "ldk/engine.h"
#include "ldk/eventqueue.h"
#include "ldk/module/graphics.h"
#include "ldk/module/render.h"
#include "ldk/module/asset.h"
#include "ldk/asset/shader.h"
#include "ldk/asset/material.h"
#include <string.h>

static struct
{
  bool running;
  int32 exitCode;
} internal = {0};

void logModuleInit(const char* moduleName, bool success)
{
  //ldkLogPrintRaw("LDK", "%s initializing %s",  success ? "[ OK ]" : "[FAIL]", moduleName);
  ldkLogPrintRaw("INFO", "initializing %s", moduleName);
}

void logModuleTerminate(const char* moduleName)
{
  ldkLogPrintRaw("INFO", "Terminating %s", moduleName);
}

bool ldkEngineInitialize()
{
  ldkOsCwdSetFromExecutablePath();

  bool success = true;
  bool stepSuccess;

  // Startup Log
  stepSuccess = ldkLogInitialize("ldk.log");
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
