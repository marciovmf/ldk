
#include "../include/ldk/os.h"
#include "../include/ldk/engine.h"
#include "../include/ldk/eventqueue.h"
#include "../include/ldk/module/graphics.h"

static struct
{
  bool running;
  int32 exitCode;
} internal = {0};

bool ldkEngineInitialize()
{
  ldkOsCwdSetFromExecutablePath();

  bool success = true;
  bool stepSuccess;

  // Startup Log
  stepSuccess = ldkLogInitialize("ldk.log");
  ldkLogInfo("-- Initializing Log %s", stepSuccess ? "SUCCES" : "FAIL"); 
  success &= stepSuccess;

  // Startup Graphics
  stepSuccess  = ldkGraphicsInitialize(LDK_GRAPHICS_API_OPENGL_3_3);
  ldkLogInfo("-- Initializing Graphics %s", stepSuccess ? "SUCCES" : "FAIL"); 
  success &= stepSuccess;

  ldkLogInfo("-- Engine Initialization %s", stepSuccess ? "SUCCES" : "FAIL"); 

  return success;
}

void ldkEngineTerminate()
{
  ldkLogInfo("-- Terminating Graphics...", 0);
  ldkGraphicsTerminate();
  ldkLogInfo("-- Terminating Log...\n-- Engine terminated.\n", 0);
  ldkLogTerminate();
}

LDK_API int32 ldkEngineRun()
{
  LDKEventType event = {0};
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
