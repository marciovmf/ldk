/**
 *
 * graphics.h
 *
 * This is a thin layer over some OS functions to provide higher level graphics/context related operations.
 *
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../common.h"
#include "../os.h"

#ifdef __cplusplus
extern "C" {
#endif

  // LDKGraphicsAPI
  typedef enum
  {
    LDK_GRAPHICS_API_NONE           = 0x00,
    LDK_GRAPHICS_API_OPENGL_ES_2_0  = 0x01,
    LDK_GRAPHICS_API_OPENGL_ES_3_0  = 0x02,
    LDK_GRAPHICS_API_OPENGL_3_0     = 0x03,
    LDK_GRAPHICS_API_OPENGL_3_3     = 0x04,
    LDK_GRAPHICS_API_OPENGL_4_0     = 0x05,
  } LDKGraphicsAPI;

  LDK_API bool    ldkGraphicsInitialize(LDKGraphicsAPI api);
  LDK_API void    ldkGraphicsTerminate();
  LDK_API void    ldkGraphicsFullscreenSet(bool fullscreen);
  LDK_API bool    ldkGraphicsFullscreenGet();
  LDK_API void    ldkGraphicsViewportTitleSet(const char* title);
  LDK_API size_t  ldkGraphicsViewportTitleGet(LDKSmallStr* outTitle);
  LDK_API bool    ldkGraphicsViewportIconSet(const char* iconPath);
  LDK_API void    ldkGraphicsViewportPositionSet(int x, int y);
  LDK_API void    ldkGraphicsViewportSizeSet(int w, int h);
  LDK_API LDKSize ldkGraphicsViewportSizeGet();
  LDK_API float   ldkGraphicsViewportRatio();
  LDK_API LDKGraphicsContext ldkGraphicsContextGet();
  LDK_API void    ldkGraphicsSwapBuffers();
  LDK_API void    ldkGraphicsVsyncSet(bool vsync);
  LDK_API int32   ldkGraphicsVsyncGet();
  LDK_API void  ldkGraphicsMultisamplesSet(int samples);
  LDK_API int32 ldkGraphicsMultisamplesGet();
  //LDK_API void  ldkGraphicsAntialiasingSet(int quality);
  //LDK_API int32 ldkGraphicsAntialiasingGet();
  //LDK_API void  ldkGraphicsViewportScreenshot();

  /* Viewport */

  /* Cursor */
  //LDK_API void ldkGraphicsCursorHiddenSet(bool hidden);
  //LDK_API bool ldkGraphicsCursorHiddenGet();


#ifdef __cplusplus
}
#endif

#endif //GRAPHICS_H

