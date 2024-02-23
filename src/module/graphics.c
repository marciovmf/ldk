#include "ldk/module/graphics.h"
#include "ldk/gl.h"
#include "ldk/os.h"

static struct
{
  LDKWindow mainWindow;
  LDKGCtx context;
  LDKGraphicsAPI  api;
  int32 multiSampleLevel;
} internal;

bool ldkGraphicsInitialize(LDKGraphicsAPI api)
{
  //TODO(marcio): Where should we get these parameters from ?
  const int32 colorBits = 24;
  const int32 depthBits = 8;

  switch(api)
  {
    case LDK_GRAPHICS_API_NONE:
      break;
    case LDK_GRAPHICS_API_OPENGL_ES_2_0:
      internal.context = ldkOsGraphicsContextOpenglesCreate(2, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_ES_3_0:
      internal.context = ldkOsGraphicsContextOpenglesCreate(2, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_3_0:
      internal.context = ldkOsGraphicsContextOpenglCreate(3, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_3_3:
      internal.context = ldkOsGraphicsContextOpenglCreate(3, 3, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_4_0:
      internal.context = ldkOsGraphicsContextOpenglCreate(4, 0, colorBits, depthBits);
      break;
  }

  internal.mainWindow = ldkOsWindowCreate("LDK", 800, 600);
  ldkOsGraphicsContextCurrent(internal.mainWindow, internal.context);
  bool success = internal.context != NULL;

  internal.api = api;
  return success;
}

LDKGraphicsAPI ldkGraphicsApiName(void)
{
  return internal.api;
}

void ldkGraphicsTerminate(void)
{
  ldkOsWindowDestroy(internal.mainWindow);
  ldkOsGraphicsContextDestroy(internal.context);
}

void ldkGraphicsFullscreenSet(bool fullscreen)
{
  ldkOsWindowFullscreenSet(internal.mainWindow, true);
}

bool ldkGraphicsFullscreenGet(void)
{
  return ldkOsWindowFullscreenGet(internal.mainWindow);
}

void ldkGraphicsViewportTitleSet(const char* title)
{
  ldkOsWindowTitleSet(internal.mainWindow, title);
}

size_t ldkGraphicsViewportTitleGet(LDKSmallStr* outTitle)
{
  return ldkOsWindowTitleGet(internal.mainWindow, outTitle);
}

bool ldkGraphicsViewportIconSet(const char* iconPath)
{
  return ldkOsWindowIconSet(internal.mainWindow, iconPath);
}

void ldkGraphicsViewportPositionSet(int x, int y)
{
  ldkOsWindowPositionSet(internal.mainWindow, x, y);
}

void ldkGraphicsViewportSizeSet(int w, int h)
{
  ldkOsWindowClientAreaSizeSet(internal.mainWindow, w, h);
}

LDKSize ldkGraphicsViewportSizeGet(void)
{
  LDKSize size = ldkOsWindowClientAreaSizeGet(internal.mainWindow);
  return size;
}

float ldkGraphicsViewportRatio(void)
{
  LDKSize size = ldkGraphicsViewportSizeGet();
  if (size.height <= 0)
    return 0.0f;

  return (size.width / (float) size.height);
}

LDKGCtx ldkGraphicsContextGet(void)
{
  return internal.context;
}

void ldkGraphicsSwapBuffers(void)
{
  ldkOsWindowBuffersSwap(internal.mainWindow);
}

void ldkGraphicsVsyncSet(bool vsync)
{
  //NOTE(marcio): Should we support addaptive ?
  ldkOsGraphicsVSyncSet(vsync);
}

int32 ldkGraphicsVsyncGet(void)
{
  return ldkOsGraphicsVSyncGet();
}

void  ldkGraphicsMultisamplesSet(int32 samples)
{
  if (internal.api == LDK_GRAPHICS_API_OPENGL_3_3
      || internal.api == LDK_GRAPHICS_API_OPENGL_3_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_3_3
      || internal.api == LDK_GRAPHICS_API_OPENGL_4_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_ES_2_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_ES_3_0)
  {
    glEnable(GL_MULTISAMPLE);
    glSampleCoverage(samples * 1.0f, GL_TRUE);
    internal.multiSampleLevel = samples;
  }
  else
  {
    LDK_NOT_IMPLEMENTED();
  }
}

int32 ldkGraphicsMultisamplesGet(void)
{
  return internal.multiSampleLevel;
}

void lkdGraphicsInfoPrint(void)
{
  if (internal.api == LDK_GRAPHICS_API_OPENGL_3_3
      || internal.api == LDK_GRAPHICS_API_OPENGL_3_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_3_3
      || internal.api == LDK_GRAPHICS_API_OPENGL_4_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_ES_2_0
      || internal.api == LDK_GRAPHICS_API_OPENGL_ES_3_0)
  {
    ldkLogInfo("OpenGL Renderer: Vendor: %s Renderer: %s Version: %s Shading Language Version: %s",
        glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
  }
  else
  {
    LDK_NOT_IMPLEMENTED();
  }
}

