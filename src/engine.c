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
#include "ldk/entity/pointlight.h"
#include "ldk/entity/directionallight.h"
#include "ldk/entity/spotlight.h"
#include "ldk/asset/image.h"
#include "ldk/asset/shader.h"
#include "ldk/asset/material.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/config.h"
#include "ldk/asset/texture.h"

#ifdef LDK_EDITOR
#include "ldk/module/editor.h"
#endif

#include <string.h>
#include <signal.h>

static struct
{
  bool running;
  int32 exitCode;
  float timeScale;
  bool editorStartsEnabled;
} internal = {0};

static void s_onSignal(int32 signal)
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

static bool s_eventHandler(const LDKEvent* event, void* data)
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

void ldkEngineSetTimeScale(float scale)
{
  internal.timeScale = scale;
}

float ldkEngineGetTimeScale()
{
  return internal.timeScale;
}

bool ldkEngineInitialize(void)
{
  signal(SIGABRT, s_onSignal);
  signal(SIGFPE,  s_onSignal);
  signal(SIGILL,  s_onSignal);
  signal(SIGINT,  s_onSignal);
  signal(SIGSEGV, s_onSignal);
  signal(SIGTERM, s_onSignal);

  ldkOsInitialize();
  ldkOsCwdSetFromExecutablePath();

  bool success = true;
  bool stepSuccess;

  internal.timeScale = 1.0f;

  LDKDateTime  dateTime;
  ldkOsSystemDateTimeGet(&dateTime);
  char strDateTime[64];
  snprintf(strDateTime, 64, "---- %d.%d.%d %d:%d:%d - LDK v%d.%d.%d %s ----",
      dateTime.year, dateTime.month, dateTime.day,
      dateTime.hour, dateTime.minute, dateTime.second,
      LDK_VERSION_MAJOR, LDK_VERSION_PATCH, LDK_VERSION_MINOR, LDK_BUILD_TYPE
      );

  // Startup Log
  stepSuccess = ldkLogInitialize("ldk.log", strDateTime);
  logModuleInit("Log", stepSuccess);
  success &= stepSuccess;

  // Startup Asset Handlers
  stepSuccess = ldkAssetInitialize();
  stepSuccess &= ldkAssetHandlerRegister(LDKConfig,   ldkAssetConfigLoadFunc,   ldkAssetConfigUnloadFunc,   2,  "cfg");
  LDKConfig* config = ldkAssetGet(LDKConfig, "ldk.cfg");

  stepSuccess &= ldkAssetHandlerRegister(LDKImage,    ldkAssetImageLoadFunc,    ldkAssetImageUnloadFunc,    16, "png", "bmp", "tga", "hdr", "jpg");
  stepSuccess &= ldkAssetHandlerRegister(LDKTexture,  ldkAssetTextureLoadFunc,  ldkAssetTextureUnloadFunc,  16, "texture");
  stepSuccess &= ldkAssetHandlerRegister(LDKShader,   ldkAssetShaderLoadFunc,   ldkAssetShaderUnloadFunc,   16, "shader");
  stepSuccess &= ldkAssetHandlerRegister(LDKMaterial, ldkAssetMaterialLoadFunc, ldkAssetMaterialUnloadFunc, 16, "material");
  stepSuccess &= ldkAssetHandlerRegister(LDKMesh,     ldkAssetMeshLoadFunc,     ldkAssetMeshUnloadFunc,     32, "mesh");
  logModuleInit("Asset Handler", stepSuccess);

  // Register EntityManager
  stepSuccess &= ldkEntityManagerInit();
  stepSuccess &= ldkEntityTypeRegister(LDKCamera, ldkCameraEntityCreate, ldkCameraEntityDestroy,
      ldkCameraEntityGetTransform, ldkCameraEntitySetTransform, 2);
  stepSuccess &= ldkEntityTypeRegister(LDKStaticObject, ldkStaticObjectEntityCreate, ldkStaticObjectEntityDestroy,
      ldkStaticObjectEntityGetTransform, ldkStaticObjectEntitySetTransform, 32);
  stepSuccess &= ldkEntityTypeRegister(LDKInstancedObject, ldkInstancedObjectEntityCreate, ldkInstancedObjectEntityDestroy,
      ldkInstancedObjectEntityGetTransform, ldkInstancedObjectEntitySetTransform, 8);
  stepSuccess &= ldkEntityTypeRegister(LDKPointLight, ldkPointLightEntityCreate, ldkPointLightEntityDestroy,
      ldkPointLightEntityGetTransform, ldkPointLightEntitySetTransform, 32);
  stepSuccess &= ldkEntityTypeRegister(LDKDirectionalLight, ldkDirectionalLightEntityCreate, ldkDirectionalLightEntityDestroy,
      ldkDirectionalLightEntityGetTransform, ldkDirectionalLightEntitySetTransform, 32);
  stepSuccess &= ldkEntityTypeRegister(LDKSpotLight, ldkSpotLightEntityCreate, ldkSpotLightEntityDestroy,
      ldkSpotLightEntityGetTransform, ldkSpotLightEntitySetTransform, 32);
  success &= stepSuccess;
  logModuleInit("Entity Manager", stepSuccess);


  // Startup Graphics
  stepSuccess = ldkGraphicsInitialize(config, LDK_GRAPHICS_API_OPENGL_3_3);
  success &= stepSuccess;
  logModuleInit("Graphics", stepSuccess);

  // Startup renderer
  stepSuccess = ldkRendererInitialize(config);
  success &= stepSuccess;
  logModuleInit("Renderer", stepSuccess);

#ifdef LDK_EDITOR
  // Startup editor
  internal.editorStartsEnabled = ldkConfigGetInt(config, "editor-enabled-on-start");
  stepSuccess = ldkEditorInitialize();
  success &= stepSuccess;
  logModuleInit("Editor", stepSuccess);
#endif // LDK_EDITOR

  success &= stepSuccess;
  logModuleInit("LDK", success);

  lkdGraphicsInfoPrint();

  ldkEventHandlerAdd(s_eventHandler, LDK_EVENT_TYPE_WINDOW, NULL);

  return success;
}

void ldkEngineTerminate(void)
{
#ifdef LDK_EDITOR
  logModuleTerminate("Editor");
  ldkEditorTerminate();
#endif
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

  uint64 tickStart = ldkOsTimeTicksGet();
  uint64 tickEnd = tickStart;

  while (internal.running)
  {
    LDKEvent event;
    while(ldkOsEventsPoll(&event))
    {
      ldkEventPush(&event);
    }

    LDKKeyboardState kbdState;
    ldkOsKeyboardStateGet(&kbdState);

    double delta = (float) ldkOsTimeTicksIntervalGetMilliseconds(tickStart, tickEnd);
    float deltaTime = (float) delta / 1000;
    deltaTime *= internal.timeScale;

    tickStart = ldkOsTimeTicksGet();

#ifdef LDK_EDITOR

    if (internal.editorStartsEnabled)
    {
      ldkEditorEnable(true);
      internal.editorStartsEnabled = false;
    }
    else if (ldkOsKeyboardKeyDown(&kbdState, LDK_KEYCODE_F3))
    {
      ldkEditorEnable(!ldkEditorIsEnabled());
    }
#endif

    event.type = LDK_EVENT_TYPE_FRAME_BEFORE;
    event.frameEvent.ticks = tickStart;
    event.frameEvent.deltaTime = deltaTime;
    ldkEventPush(&event);
    ldkEventQueueBroadcast();

    ldkRendererRender(deltaTime);
#ifdef LDK_EDITOR
    if (ldkEditorIsEnabled())
      ldkEditorImmediateDraw(deltaTime);
#endif

    ldkGraphicsSwapBuffers();
    tickEnd = ldkOsTimeTicksGet();

    event.type = LDK_EVENT_TYPE_FRAME_AFTER;
    event.frameEvent.ticks = tickEnd;
    event.frameEvent.deltaTime = deltaTime;
    ldkEventPush(&event);
    ldkEventQueueBroadcast();

    //
    // Pass entities to the renderer
    //
    LDKHListIterator it;
    it = ldkEntityManagerGetIterator(LDKStaticObject);
    while(ldkHListIteratorNext(&it)) { ldkRendererAddStaticObject(ldkHListIteratorCurrent(&it)); }

    it = ldkEntityManagerGetIterator(LDKInstancedObject);
    while(ldkHListIteratorNext(&it)) { ldkRendererAddInstancedObject(ldkHListIteratorCurrent(&it)); }

    it = ldkEntityManagerGetIterator(LDKDirectionalLight);
    while(ldkHListIteratorNext(&it)) { ldkRendererAddDirectionalLight(ldkHListIteratorCurrent(&it)); }

    it = ldkEntityManagerGetIterator(LDKPointLight);
    while(ldkHListIteratorNext(&it)) { ldkRendererAddPointLight(ldkHListIteratorCurrent(&it)); }

    it = ldkEntityManagerGetIterator(LDKSpotLight);
    while(ldkHListIteratorNext(&it)) { ldkRendererAddSpotLight(ldkHListIteratorCurrent(&it)); }
  }

  ldkEngineTerminate();
  return internal.exitCode;
}

#ifdef LDK_EDITOR
bool ldkEngineIsEditorRunning(void)
{
  return ldkEditorIsEnabled();
}
#endif

void ldkEngineStop(int32 exitCode)
{
  internal.exitCode = exitCode;
  internal.running = false;
}

