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
    LDK_GRAPHICS_API_NONE           = 0,
    LDK_GRAPHICS_API_OPENGL_ES_2_0  = 1 << 1,
    LDK_GRAPHICS_API_OPENGL_ES_3_0  = 1 << 2,
    LDK_GRAPHICS_API_OPENGL_3_0     = 1 << 3,
    LDK_GRAPHICS_API_OPENGL_3_3     = 1 << 4,
    LDK_GRAPHICS_API_OPENGL_4_0     = 1 << 5,

    // Used to match if currecnt api is opengl
    LDK_GRAPHICS_OPENGL =
      LDK_GRAPHICS_API_OPENGL_ES_2_0
      | LDK_GRAPHICS_API_OPENGL_ES_3_0 
      | LDK_GRAPHICS_API_OPENGL_3_0
      | LDK_GRAPHICS_API_OPENGL_3_3
      | LDK_GRAPHICS_API_OPENGL_4_0,

    // Used to match if currecnt api is opengl core
    LDK_GRAPHICS_API_OPENGL_CORE = 
       LDK_GRAPHICS_API_OPENGL_3_0
      | LDK_GRAPHICS_API_OPENGL_3_3
      | LDK_GRAPHICS_API_OPENGL_4_0,

    // Used to match if currecnt api is opengl es
    LDK_GRAPHICS_OPENGL_ES =
      LDK_GRAPHICS_API_OPENGL_ES_2_0
      | LDK_GRAPHICS_API_OPENGL_ES_3_0 

  } LDKGraphicsAPI;

  LDK_API bool    ldkGraphicsInitialize(LDKGraphicsAPI api);
  LDK_API LDKGraphicsAPI ldkGraphicsApiName();
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
  LDK_API LDKGCtx ldkGraphicsContextGet();
  LDK_API void    ldkGraphicsSwapBuffers();
  LDK_API void    ldkGraphicsVsyncSet(bool vsync);
  LDK_API int32   ldkGraphicsVsyncGet();
  LDK_API void    ldkGraphicsMultisamplesSet(int samples);
  LDK_API int32   ldkGraphicsMultisamplesGet();
  LDK_API void    lkdGraphicsInfoPrint();
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

