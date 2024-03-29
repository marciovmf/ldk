#include "ldk/os.h"
#include "ldk/engine.h"
#include "ldk/eventqueue.h"
#include "ldk/module/graphics.h"
#include "ldk/module/renderer.h"
#include "ldk/module/entity.h"
#include "ldk/module/asset.h"
#include "ldk/entity/camera.h"
#include "ldk/entity/staticobject.h"
#include "ldk/entity/instancedobject.h"
#include "ldk/asset/image.h"
#include "ldk/asset/shader.h"
#include "ldk/asset/material.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/config.h"
#include "ldk/asset/texture.h"
#include <string.h>
#include <signal.h>

static struct
{
  bool running;
  int32 exitCode;
} internal = {0};

static void internalOnSignal(int32 signal)
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

static bool internalEventHandler(const LDKEvent* event, void* data)
{
  if (!internal.running)
    return false;

  if (event->windowEvent.type == LDK_WINDOW_EVENT_RESIZED || event->windowEvent.type == LDK_WINDOW_EVENT_MAXIMIZED)
  {
    uint32 width = event->windowEvent.width;
    uint32 height = event->windowEvent.height;
    if (width && height)
      ldkRendererResize(width, height);
  }
  return false;
}

void logModuleInit(const char* moduleName, bool success)
{
  uint32 dotsCount = 25 - (int32) strlen(moduleName) - 4;
  ldkLogInfo("Init %s%.*s%s", moduleName, dotsCount, ".........................", success ? " OK " : "FAIL");
}

void logModuleTerminate(const char* moduleName)
{
  ldkLogInfo("Terminating %s", moduleName);
}

bool ldkEngineInitialize(void)
{
  signal(SIGABRT, internalOnSignal);
  signal(SIGFPE,  internalOnSignal);
  signal(SIGILL,  internalOnSignal);
  signal(SIGINT,  internalOnSignal);
  signal(SIGSEGV, internalOnSignal);
  signal(SIGTERM, internalOnSignal);

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

  // Register LDKHandle so any other handle type is larger than 0
  typeid(LDKHandle);

  // Startup Asset Handlers
  stepSuccess = ldkAssetInitialize();
  stepSuccess &= ldkAssetHandlerRegister(LDKConfig,   ldkAssetConfigLoadFunc,   ldkAssetConfigUnloadFunc,   2,  "cfg");
  LDKConfig* config = ldkAssetGet(LDKConfig, "ldk.cfg");


  stepSuccess &= ldkAssetHandlerRegister(LDKImage,    ldkAssetImageLoadFunc,    ldkAssetImageUnloadFunc,    8,  "png", "bmp", "tga", "hdr");
  stepSuccess &= ldkAssetHandlerRegister(LDKTexture,  ldkAssetTextureLoadFunc,  ldkAssetTextureUnloadFunc,  8,  "texture");
  stepSuccess &= ldkAssetHandlerRegister(LDKShader,   ldkAssetShaderLoadFunc,   ldkAssetShaderUnloadFunc,   8,  "shader");
  stepSuccess &= ldkAssetHandlerRegister(LDKMaterial, ldkAssetMaterialLoadFunc, ldkAssetMaterialUnloadFunc, 8,  "material");
  stepSuccess &= ldkAssetHandlerRegister(LDKMesh,     ldkAssetMeshLoadFunc,     ldkAssetMeshUnloadFunc,     16, "mesh");
  logModuleInit("Asset Handler", stepSuccess);

  // Register EntityManager
  stepSuccess &= ldkEntityManagerInit();
  stepSuccess &= ldkEntityTypeRegister(LDKCamera, ldkCameraEntityCreate, ldkCameraEntityDestroy, 2);
  stepSuccess &= ldkEntityTypeRegister(LDKStaticObject, ldkStaticObjectEntityCreate, ldkStaticObjectEntityDestroy, 32);
  stepSuccess &= ldkEntityTypeRegister(LDKInstancedObject, ldkInstancedObjectEntityCreate, ldkInstancedObjectEntityDestroy, 8);
  success &= stepSuccess;
  logModuleInit("Entity Manager", stepSuccess);

  // Startup Graphics
  //TODO(marcio): Get initial display width and height from a config file
  stepSuccess = ldkGraphicsInitialize(config, LDK_GRAPHICS_API_OPENGL_3_3);
  success &= stepSuccess;
  logModuleInit("Graphics", stepSuccess);

  // Startup renderer
  stepSuccess = ldkRendererInitialize(config);
  success &= stepSuccess;
  logModuleInit("Renderer", stepSuccess);

  success &= stepSuccess;
  logModuleInit("LDK", success);

  lkdGraphicsInfoPrint();

  ldkEventHandlerAdd(internalEventHandler, LDK_EVENT_TYPE_WINDOW, NULL);

  return success;
}

void ldkEngineTerminate(void)
{
  logModuleTerminate("Renderer");
  ldkRendererTerminate();
  logModuleTerminate("Graphics");
  ldkGraphicsTerminate();
  logModuleTerminate("EntityManager Handler");
  ldkEntityManagerTerminate();
  logModuleTerminate("Asset Handler");
  ldkAssetTerminate();
  logModuleTerminate("Log\n ---");
  ldkLogTerminate();
  ldkOsTerminate();
}

LDK_API int32 ldkEngineRun(void)
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
  // unload all assets

  ldkEngineTerminate();
  return internal.exitCode;
}

void ldkEngineStop(int32 exitCode)
{
  internal.exitCode = exitCode;
  internal.running = false;
}

