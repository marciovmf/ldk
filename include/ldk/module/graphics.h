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
#include "ldk/asset/config.h"

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
  } LDKGraphicsAPI;

  enum
  {
    // Used to match if current api is opengl
    LDK_GRAPHICS_OPENGL =
      LDK_GRAPHICS_API_OPENGL_ES_2_0
      | LDK_GRAPHICS_API_OPENGL_ES_3_0 
      | LDK_GRAPHICS_API_OPENGL_3_0
      | LDK_GRAPHICS_API_OPENGL_3_3
      | LDK_GRAPHICS_API_OPENGL_4_0,

    // Used to match if current api is opengl core
    LDK_GRAPHICS_API_OPENGL_CORE = 
      LDK_GRAPHICS_API_OPENGL_3_0
      | LDK_GRAPHICS_API_OPENGL_3_3
      | LDK_GRAPHICS_API_OPENGL_4_0,

    // Used to match if current api is opengl es
    LDK_GRAPHICS_OPENGL_ES =
      LDK_GRAPHICS_API_OPENGL_ES_2_0
      | LDK_GRAPHICS_API_OPENGL_ES_3_0 
  };

  LDK_API bool    ldkGraphicsInitialize(LDKConfig* config, LDKGraphicsAPI api);
  LDK_API LDKGraphicsAPI ldkGraphicsApiName(void);
  LDK_API void    ldkGraphicsTerminate(void);
  LDK_API void    ldkGraphicsFullscreenSet(bool fullscreen);
  LDK_API bool    ldkGraphicsFullscreenGet(void);
  LDK_API void    ldkGraphicsViewportTitleSet(const char* title);
  LDK_API size_t  ldkGraphicsViewportTitleGet(LDKSmallStr* outTitle);
  LDK_API bool    ldkGraphicsViewportIconSet(const char* iconPath);
  LDK_API void    ldkGraphicsViewportPositionSet(int x, int y);
  LDK_API void    ldkGraphicsViewportSizeSet(int w, int h);
  LDK_API LDKSize ldkGraphicsViewportSizeGet(void);
  LDK_API float   ldkGraphicsViewportRatio(void);
  LDK_API LDKGCtx ldkGraphicsContextGet(void);
  LDK_API void    ldkGraphicsSwapBuffers(void);
  LDK_API void    ldkGraphicsVsyncSet(bool vsync);
  LDK_API int32   ldkGraphicsVsyncGet(void);
  LDK_API void    ldkGraphicsMultisamplesSet(int samples);
  LDK_API int32   ldkGraphicsMultisamplesGet(void);
  LDK_API void    lkdGraphicsInfoPrint(void);
  //LDK_API void  ldkGraphicsAntialiasingSet(int quality);
  //LDK_API int32 ldkGraphicsAntialiasingGet(void);
  //LDK_API void  ldkGraphicsViewportScreenshot(void);

  /* Viewport */

  /* Cursor */
  //LDK_API void ldkGraphicsCursorHiddenSet(bool hidden);
  //LDK_API bool ldkGraphicsCursorHiddenGet(void);


#ifdef __cplusplus
}
#endif

#endif //GRAPHICS_H

