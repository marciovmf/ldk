/**
 * @file ldk_text3d.h
 * @brief World-space text source component data.
 */

#ifndef LDK_TEXT3D_H
#define LDK_TEXT3D_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_component.h>
#include <stdx/stdx_string.h>

#ifdef __cplusplus
extern "C" {
#endif

  //@component
  typedef struct LDKText3DComponent
  {
    XSmallstr text;
    LDKAssetFont font;
    //@inspect slider min=1 max=256
    float pixel_height;
    //@inspect widget=COLOR
    u32 color;
    bool billboard;
    bool depth_test;
  } LDKText3DComponent;

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_text3d_component_desc(u32 initial_capacity);
#endif // LDK_ENGINE

#ifdef __cplusplus
}
#endif

#endif // LDK_TEXT3D_H
