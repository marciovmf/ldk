#include "ldk/module/graphics.h"
#include "ldk/asset/config.h"
#include "ldk/gl.h"
#include "ldk/os.h"

static struct
{
  LDKWindow mainWindow;
  LDKGCtx context;
  LDKGraphicsAPI  api;
  int32 multiSampleLevel;
} s_graphics;

#ifndef LDK_GRAPHICS_COLOR_BITS
#define LDK_GRAPHICS_COLOR_BITS 24
#endif  // LDK_GRAPHICS_COLOR_BITS

#ifndef LDK_GRAPHICS_DEPTH_BITS
#define LDK_GRAPHICS_DEPTH_BITS 24
#endif  // LDK_GRAPHICS_DEPTH_BITS

bool ldkGraphicsInitialize(LDKConfig* config, LDKGraphicsAPI api)
{
  s_graphics.api = api;
  const int32 colorBits = LDK_GRAPHICS_COLOR_BITS;
  const int32 depthBits = LDK_GRAPHICS_DEPTH_BITS;

  switch(api)
  {
    case LDK_GRAPHICS_API_NONE:
      break;
    case LDK_GRAPHICS_API_OPENGL_ES_2_0:
      s_graphics.context = ldkOsGraphicsContextOpenglesCreate(2, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_ES_3_0:
      s_graphics.context = ldkOsGraphicsContextOpenglesCreate(2, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_3_0:
      s_graphics.context = ldkOsGraphicsContextOpenglCreate(3, 0, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_3_3:
      s_graphics.context = ldkOsGraphicsContextOpenglCreate(3, 3, colorBits, depthBits);
      break;
    case LDK_GRAPHICS_API_OPENGL_4_0:
      s_graphics.context = ldkOsGraphicsContextOpenglCreate(4, 0, colorBits, depthBits);
      break;
  }


  int32 fullscreen = ldkConfigGetInt(config, "display.fullscreen");
  Vec2 displaySize = ldkConfigGetVec2(config, "display.size");
  const char* displayTitle = ldkConfigGetString(config, "display.title");
  if (displaySize.x == 0 || displaySize.y == 0)
    displaySize = vec2(800, 600);

  s_graphics.mainWindow = ldkOsWindowCreate(displayTitle, (int32) displaySize.x, (int32) displaySize.y);
  ldkOsGraphicsContextCurrent(s_graphics.mainWindow, s_graphics.context);
  bool success = s_graphics.context != NULL;

  int32 vSync = ldkConfigGetInt(config, "graphics.vsync");
  int32 multiSampleLevel = ldkConfigGetInt(config, "graphics.multisamples");
  const char* icon = ldkConfigGetString(config, "display.icon");

  ldkGraphicsVsyncSet(vSync);
  ldkGraphicsMultisamplesSet(multiSampleLevel);
  ldkGraphicsFullscreenSet(fullscreen);
  ldkGraphicsViewportIconSet(icon);

  return success;
}

LDKGraphicsAPI ldkGraphicsApiName(void)
{
  return s_graphics.api;
}

void ldkGraphicsTerminate(void)
{
  ldkOsWindowDestroy(s_graphics.mainWindow);
  ldkOsGraphicsContextDestroy(s_graphics.context);
}

void ldkGraphicsFullscreenSet(bool fullscreen)
{
  ldkOsWindowFullscreenSet(s_graphics.mainWindow, fullscreen);
}

bool ldkGraphicsFullscreenGet(void)
{
  return ldkOsWindowFullscreenGet(s_graphics.mainWindow);
}

void ldkGraphicsViewportTitleSet(const char* title)
{
  ldkOsWindowTitleSet(s_graphics.mainWindow, title);
}

size_t ldkGraphicsViewportTitleGet(LDKSmallStr* outTitle)
{
  return ldkOsWindowTitleGet(s_graphics.mainWindow, outTitle);
}

bool ldkGraphicsViewportIconSet(const char* iconPath)
{
  return ldkOsWindowIconSet(s_graphics.mainWindow, iconPath);
}

void ldkGraphicsViewportPositionSet(int x, int y)
{
  ldkOsWindowPositionSet(s_graphics.mainWindow, x, y);
}

void ldkGraphicsViewportSizeSet(int w, int h)
{
  ldkOsWindowClientAreaSizeSet(s_graphics.mainWindow, w, h);
}

LDKSize ldkGraphicsViewportSizeGet(void)
{
  LDKSize size = ldkOsWindowClientAreaSizeGet(s_graphics.mainWindow);
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
  return s_graphics.context;
}

void ldkGraphicsSwapBuffers(void)
{
  ldkOsWindowBuffersSwap(s_graphics.mainWindow);
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
  if (s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_3
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_3
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_4_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_ES_2_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_ES_3_0)
  {
    if (samples > 0)
    {
      glEnable(GL_MULTISAMPLE);
      glSampleCoverage((float) samples, GL_TRUE);
      s_graphics.multiSampleLevel = samples;
    }
    else {
      glDisable(GL_MULTISAMPLE);
    }
  }
  else
  {
    LDK_NOT_IMPLEMENTED();
  }
}

int32 ldkGraphicsMultisamplesGet(void)
{
  return s_graphics.multiSampleLevel;
}

void lkdGraphicsInfoPrint(void)
{
  if (s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_3
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_3_3
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_4_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_ES_2_0
      || s_graphics.api == LDK_GRAPHICS_API_OPENGL_ES_3_0)
  {
    ldkLogInfo("OpenGL Renderer: Vendor: %s Renderer: %s Version: %s Shading Language Version: %s",
        glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
  }
  else
  {
    LDK_NOT_IMPLEMENTED();
  }
}

