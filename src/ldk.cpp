//
// LDK main translation unit.
//

#ifndef _LDK_ENGINE_
#define _LDK_ENGINE_
#endif // _LDK_ENGINE_

#include <ldk/ldk.h>
#include  <ldkengine/ldk_handle.h>
#include  <ldkengine/ldk_memory.h>
#include  <ldkengine/ldk_platform.h>

// platform specific implementation
#ifdef _LDK_WINDOWS_
  #include "win32/ldk_platform_win32.cpp"
#else
  #error "Unsupported platform"
#endif

#include "ldk_math.cpp"
#include "ldk_keyboard.cpp"
#include "ldk_mouse.cpp"
#include "ldk_joystick.cpp"
#include "ldk_handle.cpp"
#include "ldk_asset.cpp"
#include "ldk_cfg.cpp"
#include "ldk_sprite_batch.cpp"
#include "ldk_renderer_gl.cpp"
#include "ldk_game.cpp"
#include "ldk_heap.cpp"
#include "ldk_memory.cpp"
#include "ldk_audio.cpp"
