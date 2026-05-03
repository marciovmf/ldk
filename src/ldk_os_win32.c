#include <ldk.h>
#include <ldk_os.h>
#include "ldk_gl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

#include <stdx/stdx_string.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_log.h>

#ifdef LDK_DEBUG
#include <DbgHelp.h>
#endif

#ifndef LDK_WIN32_MAX_EVENTS
#define LDK_WIN32_MAX_EVENTS 64
#endif

#ifndef LDK_WIN32_MAX_WINDOWS
#define LDK_WIN32_MAX_WINDOWS 64
#endif

#ifndef LDK_WIN32_STACK_TRACE_SIZE_MAX
#define LDK_WIN32_STACK_TRACE_SIZE_MAX 64
#endif


// ---------------------------------------------------------------------------
// Extern
// ---------------------------------------------------------------------------

extern void* ldk_win32_opengl_proc_address_get(char* name);
extern void ldk_opengl_function_pointers_get();

// XInput specifics
typedef struct _XINPUT_GAMEPAD
{
  WORD  wButtons;
  BYTE  bLeftTrigger;
  BYTE  bRightTrigger;
  SHORT sThumbLX;
  SHORT sThumbLY;
  SHORT sThumbRX;
  SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
  DWORD          dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
  WORD wLeftMotorSpeed;
  WORD wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;

#define XINPUT_GAMEPAD_DPAD_UP	0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN	0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT	0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT	0x0008
#define XINPUT_GAMEPAD_START	0x0010
#define XINPUT_GAMEPAD_BACK	0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB	0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB	0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER	0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER	0x0200
#define XINPUT_GAMEPAD_A	0x1000
#define XINPUT_GAMEPAD_B	0x2000
#define XINPUT_GAMEPAD_X	0x4000
#define XINPUT_GAMEPAD_Y	0x8000
//#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  9000
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
#define XINPUT_MAX_AXIS_VALUE 32767
#define XINPUT_MIN_AXIS_VALUE -32768
#define XINPUT_MAX_TRIGGER_VALUE 255

typedef DWORD (*XInputGetStateFunc)(DWORD dwUserIndex, XINPUT_STATE *pState);
typedef DWORD (*XInputSetStateFunc)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);


// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

typedef struct
{
  bool close_flag;
  bool is_fullscreen;
  bool cursor_changed;
  LDKWindowFlags activation_flags; // if window is created hidden, set this on show
  HWND handle;
  HDC dc;
  LONG_PTR prev_style;
  WINDOWPLACEMENT prev_placement;
} LDKWin32Window;

typedef struct 
{
  PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormatARB;
  PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
  PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT;
  PFNWGLGETSWAPINTERVALEXTPROC      wglGetSwapIntervalEXT;
  HGLRC rc;
  u32 version_major;
  u32 version_minor;
  i32 pixel_format_attribs[16];
  i32 context_attribs[16];
} LDKWin32OpenGLAPI;

typedef enum 
{
  WIN32_GRAPHICS_API_NONE     = 0,
  WIN32_GRAPHICS_API_OPENGL   = 1,
  WIN32_GRAPHICS_API_OPENGLES = 2,
  /* Expand this enum as we support other APIS */
} Win32GraphicsAPI;

static struct
{
  Win32GraphicsAPI api;
  union
  {
    LDKWin32OpenGLAPI gl;
  };
} s_graphicsAPIInfo = {0};

static struct 
{
  LARGE_INTEGER frequency;

  // input state
  LDKKeyboardState  keyboard_state;
  LDKMouseState     mouse_state;
  LDKJoystickState  joystick_state[LDK_JOYSTICK_MAX];

  // Event queue
  LDKEvent events[LDK_WIN32_MAX_EVENTS];
  u32 events_count;
  u32 events_poll_index;

  // All windows
  LDKWindow all_windows[LDK_WIN32_MAX_WINDOWS];
  LDKWindow all_hwnd[LDK_WIN32_MAX_WINDOWS];
  u32 windowCount;

  HCURSOR default_cursor;
  XInputGetStateFunc xinput_get_state;
  XInputSetStateFunc xinput_set_state;
} s_oswin32;

static LRESULT s_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

DWORD s_xinput_get_state_stub(DWORD dwUserIndex, XINPUT_STATE *pState)
{
  ldk_log_error("No XInput ...");
  return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD s_xinput_set_state_stub(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
{
  ldk_log_error("No XInput ...");
  return ERROR_DEVICE_NOT_CONNECTED;
}

static bool s_xinput_init(void)
{
  memset(&s_oswin32.joystick_state, 0, sizeof(s_oswin32.joystick_state));

  char* xInputDllName = "xinput1_1.dll"; 
  HMODULE hXInput = LoadLibraryA(xInputDllName);
  if (!hXInput)
  {				
    xInputDllName = "xinput9_1_0.dll";
    hXInput = LoadLibraryA(xInputDllName);
  }

  if (!hXInput)
  {
    xInputDllName = "xinput1_3.dll";
    hXInput = LoadLibraryA(xInputDllName);
  }

  if (!hXInput)
  {
    ldk_log_error("could not initialize xinput. No valid xinput dll found");
    s_oswin32.xinput_get_state = (XInputGetStateFunc) s_xinput_get_state_stub;
    s_oswin32.xinput_set_state = (XInputSetStateFunc) s_xinput_set_state_stub;
    return false;
  }

  //get xinput function pointers
  s_oswin32.xinput_get_state = (XInputGetStateFunc) GetProcAddress(hXInput, "XInputGetState");
  s_oswin32.xinput_set_state = (XInputSetStateFunc) GetProcAddress(hXInput, "XInputSetState");

  if (!s_oswin32.xinput_get_state)
    s_oswin32.xinput_get_state = (XInputGetStateFunc) s_xinput_get_state_stub;

  if (!s_oswin32.xinput_set_state)
    s_oswin32.xinput_set_state = (XInputSetStateFunc) s_xinput_set_state_stub;
  return true;
}

static inline void s_xinput_poll_events(void)
{
  // get gamepad input
  for(i32 gamepadIndex = 0; gamepadIndex < LDK_JOYSTICK_MAX; gamepadIndex++)
  {
    XINPUT_STATE xinputState = {0};
    LDKJoystickState* joystick_state = &s_oswin32.joystick_state[gamepadIndex];

    // ignore unconnected controllers
    if (s_oswin32.xinput_get_state(gamepadIndex, &xinputState) == ERROR_DEVICE_NOT_CONNECTED)
    {
      joystick_state->connected = false;
      continue;
    }

    // digital buttons
    WORD buttons = xinputState.Gamepad.wButtons;
    u8 isDown=0;
    u8 wasDown=0;

    const u32 pressedBit = LDK_JOYSTICK_PRESSED_BIT;

    // Buttons
    isDown = (buttons & XINPUT_GAMEPAD_DPAD_UP) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_UP] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_UP] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_LEFT] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_LEFT] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_RIGHT] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_DPAD_RIGHT] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_START) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_START] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_START] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_BACK) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_BACK] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_BACK] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_LEFT_THUMB) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_LEFT_THUMB] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_LEFT_THUMB] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_RIGHT_THUMB] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_RIGHT_THUMB] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_LEFT_SHOULDER] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_LEFT_SHOULDER] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_A) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_A] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_A] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_B) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_B] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_B] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_X) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_X] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_X] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_Y) > 0;
    wasDown = joystick_state->button[LDK_JOYSTICK_BUTTON_Y] & pressedBit;
    joystick_state->button[LDK_JOYSTICK_BUTTON_Y] = ((isDown ^ wasDown) << 1) | isDown;

#define GAMEPAD_AXIS_VALUE(value) (value/(float)(value < 0 ? XINPUT_MIN_AXIS_VALUE * -1: XINPUT_MAX_AXIS_VALUE))
#define GAMEPAD_AXIS_IS_DEADZONE(value, deadzone) ( value > -deadzone && value < deadzone)

    // Left thumb axis
    i32 axisX = xinputState.Gamepad.sThumbLX;
    i32 axisY = xinputState.Gamepad.sThumbLY;
    i32 deadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

    // TODO(marcio): Implement deadZone filtering correctly. This is not enough!
    joystick_state->axis[LDK_JOYSTICK_AXIS_LX] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :
      GAMEPAD_AXIS_VALUE(axisX);

    joystick_state->axis[LDK_JOYSTICK_AXIS_LY] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      GAMEPAD_AXIS_VALUE(axisY);

    // Right thumb axis
    axisX = xinputState.Gamepad.sThumbRX;
    axisY = xinputState.Gamepad.sThumbRY;
    deadZone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

    joystick_state->axis[LDK_JOYSTICK_AXIS_RX] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :
      GAMEPAD_AXIS_VALUE(axisX);

    joystick_state->axis[LDK_JOYSTICK_AXIS_RY] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      GAMEPAD_AXIS_VALUE(axisY);

    // Left trigger
    axisX = xinputState.Gamepad.bLeftTrigger;
    axisY = xinputState.Gamepad.bRightTrigger;
    deadZone = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

    joystick_state->axis[LDK_JOYSTICK_AXIS_LTRIGGER] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :	
      axisX/(float) XINPUT_MAX_TRIGGER_VALUE;

    joystick_state->axis[LDK_JOYSTICK_AXIS_RTRIGGER] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      axisY/(float) XINPUT_MAX_TRIGGER_VALUE;

#undef GAMEPAD_AXIS_IS_DEADZONE
#undef GAMEPAD_AXIS_VALUE

    joystick_state->connected = true;
  }
}

inline static bool s_graphics_api_is_opengl(Win32GraphicsAPI api)
{
  return (api == WIN32_GRAPHICS_API_OPENGL || api == WIN32_GRAPHICS_API_OPENGLES);
}

static LDKEvent* s_win32_event_new(void)
{
  if (s_oswin32.events_count >= LDK_WIN32_MAX_EVENTS)
  {
    ldk_log_error("Reached the maximum number of OS events %d.", LDK_WIN32_MAX_EVENTS);
    return NULL;
  }

  LDKEvent* eventPtr = &(s_oswin32.events[s_oswin32.events_count++]);
  return eventPtr;
}

inline static bool s_opengl_init(Win32GraphicsAPI api, i32 glVersionMajor, i32 glVersionMinor, i32 color_bits, i32 depth_bits)
{
  LDKWin32Window* dummyWindow =  ldk_os_window_create("", 0,0);
  bool isGLES = api == WIN32_GRAPHICS_API_OPENGLES;

  PIXELFORMATDESCRIPTOR pfd = { 
    sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    (BYTE)depth_bits,
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0, 0, 0, 0,
    (BYTE)color_bits,
    0,
    0,
    PFD_MAIN_PLANE,
    0,
    0, 0, 0
  }; 

  int pixelFormat = ChoosePixelFormat(dummyWindow->dc, &pfd);
  if (! pixelFormat)
  {
    ldk_log_error("Unable to allocate a pixel format");
    ldk_os_window_destroy(dummyWindow);
    return false;
  }

  if (! SetPixelFormat(dummyWindow->dc, pixelFormat, &pfd))
  {
    ldk_log_error("Unable to set a pixel format");
    ldk_os_window_destroy(dummyWindow);
    return false;
  }

  HGLRC rc = wglCreateContext(dummyWindow->dc);
  if (! rc)
  {
    ldk_log_error("Unable to create a valid OpenGL context");
    ldk_os_window_destroy(dummyWindow);
    return false;
  }

  if (! wglMakeCurrent(dummyWindow->dc, rc))
  {
    ldk_log_error("Unable to set OpenGL context current");
    ldk_os_window_destroy(dummyWindow);
    return false;
  }

  const int pixel_format_attrib_list[] =
  {
    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
    WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
    WGL_COLOR_BITS_ARB, color_bits,
    WGL_DEPTH_BITS_ARB, depth_bits,
    //WGL_STENCIL_BITS_ARB, 8,

    // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_SRGB extension
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_SRGB.txt
    //WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB,  GL_TRUE, 
    // uncomment for multisampeld framebuffer, from WGL_ARB_multisample extension
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
    WGL_SAMPLE_BUFFERS_ARB, 1,
    WGL_SAMPLES_ARB,        4, // 4x MSAA
    0
  };

  const int context_attribs[] =
  {
    WGL_CONTEXT_MAJOR_VERSION_ARB, glVersionMajor,
    WGL_CONTEXT_MINOR_VERSION_ARB, glVersionMinor,
    WGL_CONTEXT_FLAGS_ARB,
#ifdef LDK_DEBUG
    WGL_CONTEXT_DEBUG_BIT_ARB |
#endif
      (isGLES ? WGL_CONTEXT_ES_PROFILE_BIT_EXT : WGL_CONTEXT_CORE_PROFILE_BIT_ARB),
    0
  };

  // Initialize the global rendering api info with OpenGL api details
  s_graphicsAPIInfo.api = api;
  s_graphicsAPIInfo.gl.rc = 0;
  s_graphicsAPIInfo.gl.version_major = glVersionMajor;
  s_graphicsAPIInfo.gl.version_minor = glVersionMinor;
  s_graphicsAPIInfo.gl.wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC) wglGetProcAddress("wglChoosePixelFormatARB");
  s_graphicsAPIInfo.gl.wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");
  s_graphicsAPIInfo.gl.wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
  s_graphicsAPIInfo.gl.wglGetSwapIntervalEXT      = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");

  memcpy(s_graphicsAPIInfo.gl.pixel_format_attribs, pixel_format_attrib_list, sizeof(pixel_format_attrib_list));
  memcpy(s_graphicsAPIInfo.gl.context_attribs, context_attribs, sizeof(context_attribs));


  // Get function pointers
  ldk_opengl_function_pointers_get();

  wglMakeCurrent(0, 0);
  wglDeleteContext(rc);
  ldk_os_window_destroy(dummyWindow);
  return true;
}

inline static LDKWindow s_ldkwindow_from_win32_hwnd(HWND hWnd)
{
  for (u32 i =0; i < s_oswin32.windowCount; i++)
  {
    if ( s_oswin32.all_hwnd[i] == hWnd)
      return s_oswin32.all_windows[i];
  }

  return NULL;
}


// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool ldk_os_initialize()
{
  //s_ = (LDKWin32s_*) ldk_os_memory_alloc(sizeof(LDKWin32s_));
  //memset(s_oswin32, 0, sizeof(LDKWin32s_));
  QueryPerformanceFrequency(&s_oswin32.frequency);
  s_xinput_init();
  return true;
}

void ldk_os_terminate(void)
{
}

void ldk_os_stack_trace_print(void)
{
#ifdef LDK_DEBUG
  static bool once = false;
  if(!once)
  {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    once = true;
  }

  const int max_stack_trace_size = LDK_WIN32_STACK_TRACE_SIZE_MAX;
  void* stack_trace[LDK_WIN32_STACK_TRACE_SIZE_MAX];
  USHORT frames = CaptureStackBackTrace(1, max_stack_trace_size, stack_trace, NULL);

  SYMBOL_INFO* symbol_info = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
  symbol_info->MaxNameLen = 255;
  symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (USHORT i = 0; i < frames; i++)
  {
    SymFromAddr(GetCurrentProcess(), (DWORD64)(stack_trace[i]), 0, symbol_info);

    IMAGEHLP_LINE64 line_info;
    line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displacement;
    SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(stack_trace[i]), &displacement, &line_info);

#ifdef LDK_COMPILER_MSVC
    if (x_cstr_ends_with(symbol_info->Name, "s_on_signal") || x_cstr_ends_with(symbol_info->Name, "seh_filter_exe"))
      continue;

    if (line_info.FileName && (x_cstr_ends_with(line_info.FileName, "vcstartup\\src\\startup\\exe_common.inl")
          || x_cstr_ends_with(line_info.FileName, "src\\vctools\\crt\\vcstartup\\src\\startup\\exe_main.cpp")
          || x_cstr_ends_with(line_info.FileName, "vctools\\crt\\vcstartup\\src\\rtc\\error.cpp")
          || x_cstr_ends_with(line_info.FileName, "vctools\\crt\\vcstartup\\src\\rtc\\stack.cpp")
          ))
    {
      continue;
    }
#endif
    // Print the stack trace information

    ldk_log_info("\tat %s(%s:%d)\n",
        symbol_info->Name,
        (line_info.FileName ? line_info.FileName : "N/A"), line_info.LineNumber);
  }

  free(symbol_info);
#endif
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

void* ldk_os_memory_alloc(size_t size)
{
  void* mem = malloc(size);
  if(mem == NULL)
  {
    ldk_log_error("Memmory allocation failed");
    ldk_os_stack_trace_print();
  }
  return mem;
}

void ldk_os_memory_free(void* memory)
{
  free(memory);
}

void* ldk_os_memory_resize(void* memory, size_t size)
{
  void* mem = realloc(memory, size);
  if(mem == NULL)
  {
    ldk_log_error("Memmory allocation failed");
    ldk_os_stack_trace_print();
  }

  return mem;
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

u64 ldk_os_time_ticks_get(void)
{
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return counter.QuadPart;
}

double ldk_os_time_ticks_interval_get_seconds(u64 start, u64 end)
{
  return ((end - start) / (double) s_oswin32.frequency.QuadPart);
}

double ldk_os_time_ticks_interval_get_milliseconds(u64 start, u64 end)
{
  double difference = (end - start) * 1000.0;
  return (difference / s_oswin32.frequency.QuadPart);
}

double ldk_os_time_ticks_interval_get_nanoseconds(u64 start, u64 end)
{
  double difference = (end - start) * 1000000000.0;
  return (difference / s_oswin32.frequency.QuadPart);
}


// ---------------------------------------------------------------------------
// Windowing
// ---------------------------------------------------------------------------

void LDK_API ldk_os_graphics_context_destroy(LDKGCtx context)
{
  X_ASSERT(context == &s_graphicsAPIInfo);

  bool isOpenGL = s_graphics_api_is_opengl(s_graphicsAPIInfo.api);
  if (isOpenGL)
  {
    wglDeleteContext(s_graphicsAPIInfo.gl.rc);
    s_graphicsAPIInfo.api = WIN32_GRAPHICS_API_NONE;
  }
}

void ldk_os_window_destroy(LDKWindow window)
{
  u32 lastWindowIndex = s_oswin32.windowCount - 1;
  for (u32 i =0; i < s_oswin32.windowCount; i++)
  {
    if (s_oswin32.all_windows[i] == window)
    {
      if (i != lastWindowIndex)
      {
        s_oswin32.all_windows[i] = s_oswin32.all_windows[lastWindowIndex];
        s_oswin32.all_hwnd[i] = s_oswin32.all_hwnd[lastWindowIndex];
      }

      s_oswin32.all_windows[lastWindowIndex] = NULL;
      s_oswin32.all_hwnd[lastWindowIndex] = NULL;
      s_oswin32.windowCount--;
      break;
    }
  }

  LDKWin32Window* ldk_window = ((LDKWin32Window*)window);
  DeleteDC(ldk_window->dc);
  DestroyWindow(ldk_window->handle);
  ldk_os_memory_free(window);
}

LDKWindow ldk_os_window_create_with_flags(const char* title, i32 width, i32 height, LDKWindowFlags flags)
{
  if (s_oswin32.windowCount > LDK_WIN32_MAX_WINDOWS)
  {
    ldk_log_error("Exceeded maximum number of windows %d", LDK_WIN32_MAX_WINDOWS);
    return NULL;
  }

  const char* ldk_window_class = "LDK_WINDOW_CLASS";
  HINSTANCE hInstance = GetModuleHandleA(NULL);
  WNDCLASSEXA wc = {0};

  // Calculate total window size
  RECT clientArea = {(LONG)0,(LONG)0, (LONG)width, (LONG)height};
  if (!AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, FALSE))
  {
    ldk_log_error("Could not calculate window size", 0);
  }

  if (! GetClassInfoExA(hInstance, ldk_window_class, &wc))
  {
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = s_window_proc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = ldk_window_class;

    // Do not try registering the class multiple times
    if (! RegisterClassExA(&wc))
    {
      ldk_log_error("Could not register window class", 0);
      return NULL;
    }
  }

  DWORD windowStyle = WS_OVERLAPPEDWINDOW;
  if (flags & LDK_WINDOW_FLAG_NORESIZE)
    windowStyle &= ~WS_SIZEBOX;

  u32 windowWidth = clientArea.right - clientArea.left;
  u32 windowHeight = clientArea.bottom - clientArea.top;
  HWND window_handle = CreateWindowExA(
      0,
      ldk_window_class,
      title, 
      windowStyle,
      CW_USEDEFAULT, CW_USEDEFAULT,
      windowWidth, windowHeight,
      NULL, NULL,
      hInstance,
      NULL);

  if (! window_handle)
  {
    ldk_log_error("Could not create a window", 0);
    return NULL;
  }

  DWORD showFlag = (flags & LDK_WINDOW_FLAG_NORMAL) ? SW_SHOWNORMAL : 0;
  showFlag |= (flags & LDK_WINDOW_FLAG_MINIMIZED) ? SW_SHOWMINIMIZED : 0;
  showFlag |= (flags & LDK_WINDOW_FLAG_MAXIMIZED) ? SW_SHOWMAXIMIZED : 0;

  if (! (flags & LDK_WINDOW_FLAG_HIDDEN))
    ShowWindow(window_handle, showFlag);

  LDKWin32Window* window = ldk_os_memory_alloc(sizeof(LDKWin32Window));
  window->handle = window_handle;
  window->dc = GetDC(window_handle);
  window->is_fullscreen = false;
  window->close_flag = false;

  bool isOpenGL = s_graphics_api_is_opengl(s_graphicsAPIInfo .api);
  if(isOpenGL)
  {
    int pixelFormat;
    int num_pixel_formats = 0;
    PIXELFORMATDESCRIPTOR pfd;

    const int* pixel_format_attrib_list  = (const int*) s_graphicsAPIInfo .gl.pixel_format_attribs;
    const int* context_attrib_list      = (const int*) s_graphicsAPIInfo.gl.context_attribs;

    s_graphicsAPIInfo.gl.wglChoosePixelFormatARB(window->dc,
        pixel_format_attrib_list,
        NULL,
        1,
        &pixelFormat,
        (UINT*) &num_pixel_formats);

    if (num_pixel_formats <= 0)
    {
      ldk_log_error("Unable to get a valid pixel format", 0);
      return NULL;
    }

    if (! SetPixelFormat(window->dc, pixelFormat, &pfd))
    {
      ldk_log_error("Unable to set a pixel format", 0);
      return NULL;
    }

    HGLRC rc = s_graphicsAPIInfo.gl.rc;

    if (! rc)
    {
      rc = s_graphicsAPIInfo.gl.wglCreateContextAttribsARB(
          window->dc,
          NULL,
          context_attrib_list
          );

      if (! rc)
      {
        return NULL;
      }

      s_graphicsAPIInfo.gl.rc = rc;
    }

    if (! wglMakeCurrent(window->dc, rc))
    {
      ldk_log_error("Unable to set OpenGL context current", 0);
      return NULL;
    }
  }

  if (s_oswin32.default_cursor == 0)
    s_oswin32.default_cursor = LoadCursor(NULL, IDC_ARROW);

  SetCursor(s_oswin32.default_cursor);
  u32 windowIndex = s_oswin32.windowCount++;
  s_oswin32.all_windows[windowIndex] = window;
  s_oswin32.all_hwnd[windowIndex] = window->handle;
  return window;
}

LDKWindow ldk_os_window_create(const char* title, i32 width, i32 height)
{
  LDKWindowFlags defaultFlags = LDK_WINDOW_FLAG_NORMAL;
  return ldk_os_window_create_with_flags(title, width, height, defaultFlags);
}

bool ldk_os_window_should_close(LDKWindow window)
{
  return ((LDKWin32Window*)window)->close_flag;
}

bool ldk_os_events_poll(LDKEvent* event)
{
  MSG msg;

  if (s_oswin32.events_count == 0)
  {
    // clean up changed bit for keyboard keys
    for(int keyCode = 0; keyCode < LDK_KEYBOARD_MAX_KEYS; keyCode++)
    {
      s_oswin32.keyboard_state.key[keyCode] &= ~LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
    }

    // clean up changed bit for mouse buttons
    for(int button = 0; button <  LDK_MOUSE_MAX_BUTTONS; button++)
    {
      s_oswin32.mouse_state.button[button] &= ~LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
    }

    // reset wheel delta
    s_oswin32.mouse_state.wheel_delta = 0;
    s_oswin32.mouse_state.cursor_relative.x = 0;
    s_oswin32.mouse_state.cursor_relative.y = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  // clean up changed bit for joystick buttons
  for (u32 joystickId = 0; joystickId < LDK_JOYSTICK_MAX; joystickId++)
  {
    for(int button = 0; button < LDK_MOUSE_MAX_BUTTONS; button++)
    {
      s_oswin32.joystick_state[joystickId].button[button] &= ~LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
    }
  }

  s_xinput_poll_events();

  // WindowProc might have enqueued some events...
  if (s_oswin32.events_poll_index < s_oswin32.events_count)
  {
    memcpy(event, &s_oswin32.events[s_oswin32.events_poll_index++], sizeof(LDKEvent));
    return true;
  }
  else
  {
    s_oswin32.events_count = 0;
    s_oswin32.events_poll_index = 0;
    event->type = LDK_EVENT_TYPE_NONE;
    return false;
  }
}

void ldk_os_window_buffers_swap(LDKWindow window)
{
  SwapBuffers(((LDKWin32Window*)window)->dc);
}

bool ldk_os_window_fullscreen_get(LDKWindow window)
{
  return ((LDKWin32Window*)window)->is_fullscreen;
}

bool ldk_os_window_fullscreen_set(LDKWindow window, bool fs)
{
  LDKWin32Window* win32Window = ((LDKWin32Window*)window);
  HWND hWnd = win32Window->handle;

  if (fs && !win32Window->is_fullscreen) // Enter full screen
  {
    // Get the monitor's handle
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    // Get the monitor's info
    MONITORINFO monitorInfo = { sizeof(monitorInfo) };
    GetMonitorInfo(hMonitor, &monitorInfo);
    // Save the window's current style and position
    win32Window->prev_style = GetWindowLongPtr(hWnd, GWL_STYLE);
    win32Window->prev_placement = win32Window->prev_placement;
    GetWindowPlacement(hWnd, &win32Window->prev_placement);

    // Set the window style to full screen
    SetWindowLongPtr(hWnd, GWL_STYLE, win32Window->prev_style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowPos(hWnd, NULL,
        monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.top,
        monitorInfo.rcMonitor.right,
        monitorInfo.rcMonitor.bottom,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Set the display settings to full screen
    DEVMODE dmScreenSettings = { 0 };
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = monitorInfo.rcMonitor.right;
    dmScreenSettings.dmPelsHeight = monitorInfo.rcMonitor.bottom;
    dmScreenSettings.dmBitsPerPel = 32;
    dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    LONG result = ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
    if (result != DISP_CHANGE_SUCCESSFUL)
    {
      //debugLogError("Failed to enter fullScreen mode");
      return false;
    }
    // Show the window in full screen mode
    ShowWindow(hWnd, SW_MAXIMIZE);
    win32Window->is_fullscreen = true;
  }
  else if (!fs && win32Window->is_fullscreen) // Exit full screen
  {
    // restore window previous style and location
    SetWindowLongPtr(hWnd, GWL_STYLE, win32Window->prev_style);
    SetWindowPlacement(hWnd, &win32Window->prev_placement);
    ShowWindow(hWnd, SW_RESTORE);
    win32Window->is_fullscreen = false;
  }
  return true;
}

LDKSize ldk_os_window_client_area_size_get(LDKWindow window)
{
  HWND handle = ((LDKWin32Window*)window)->handle;
  RECT rect;
  GetClientRect(handle, &rect);
  LDKSize size = { .w = rect.right, .h = rect.bottom };
  return size;
}

LDKPoint ldk_os_window_position_get(LDKWindow window)
{
  HWND handle = ((LDKWin32Window*)window)->handle;
  RECT rect;
  GetClientRect(handle, &rect);
  LDKPoint position = { .x = rect.right, .y = rect.bottom };
  return position;
}

void ldk_os_window_position_set(LDKWindow window, i32 x, i32 y)
{
  HWND window_handle = ((LDKWin32Window*)window)->handle;
  SetWindowPos(window_handle, NULL, x, y, 0, 0, SWP_NOSIZE);
}

void ldk_os_window_size_set(LDKWindow window, i32 width, i32 height)
{
  HWND window_handle = ((LDKWin32Window*)window)->handle;
  SetWindowPos(window_handle, NULL, 0, 0, width, height, SWP_NOMOVE);
}

LDKSize ldk_os_window_size_get(LDKWindow window)
{
  RECT rect;
  HWND handle = ((LDKWin32Window*)window)->handle;
  GetWindowRect(handle, &rect);
  LDKSize size = { rect.right - rect.left, rect.bottom - rect.top };
  return size;
}

void ldk_os_window_client_area_size_set(LDKWindow window, i32 width, i32 height)
{
  RECT clientArea = {(LONG)0,(LONG)0, (LONG)width, (LONG)height};
  if (!AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, FALSE))
    return;

  u32 windowWidth = clientArea.right - clientArea.left;
  u32 windowHeight = clientArea.bottom - clientArea.top;
  ldk_os_window_size_set(window, windowWidth, windowHeight);
}

void ldk_os_window_title_set(LDKWindow window, const char* title)
{
  HWND window_handle = ((LDKWin32Window*)window)->handle;
  SetWindowTextA(window_handle, title);
}

size_t ldk_os_window_title_get(LDKWindow window, XSmallstr* outTitle)
{
  size_t length = GetWindowTextLengthA(window);

  if (length >= X_FS_PAHT_MAX_LENGTH)
    return length;

  HWND window_handle = ((LDKWin32Window*)window)->handle;
  GetWindowTextA(window_handle, (char*) &outTitle->buf, X_FS_PAHT_MAX_LENGTH);
  return 0;
}

bool ldk_os_window_icon_set(LDKWindow window, const char* iconPath)
{
  HWND window_handle = ((LDKWin32Window*)window)->handle;
  HICON hIcon = (HICON)LoadImage(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

  if (hIcon == NULL) {
    ldk_log_error("Failed to load icon '%s'", iconPath);
    return FALSE;
  }

  SendMessage(window_handle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
  SendMessage(window_handle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
  return TRUE;
}

bool ldk_os_window_show(LDKWindow window, bool show)
{
  HWND window_handle = ((LDKWin32Window*)window)->handle;
  return ShowWindow(window_handle , show ? SW_RESTORE : SW_HIDE);
}

static LRESULT s_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  bool is_mouse_button_down_event = false;
  i32 mouse_button_id = -1;
  LRESULT return_value = FALSE;

  LDKWin32Window* window = (LDKWin32Window*) s_ldkwindow_from_win32_hwnd(hwnd);

  switch(uMsg) 
  {
    case WM_NCHITTEST:
      {
        // Get the default behaviour but set the arrow cursor if it's in the client area
        LRESULT result = DefWindowProc(hwnd, uMsg, wParam, lParam);
        if(result == HTCLIENT && window->cursor_changed)
        {
          window->cursor_changed = false;
          SetCursor(s_oswin32.default_cursor);
        }
        else
          window->cursor_changed = true;
        return result;
      }
      break;

    case WM_ACTIVATE:
      {
        bool activate = (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE);
        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_WINDOW;
        e->window_event.type = activate ? LDK_WINDOW_EVENT_ACTIVATE : LDK_WINDOW_EVENT_DEACTIVATE;
        e->window = window;
      }
      break;
    case WM_CHAR:
      {
        if (ldk_os_keyboard_key_is_pressed(&s_oswin32.keyboard_state, LDK_KEYCODE_CONTROL))
          break;
        LDKEvent* e = s_win32_event_new();
        e->type                = LDK_EVENT_TYPE_TEXT;
        e->window = window;
        e->text_event.character = (u32) wParam;
        e->text_event.type      = ((u32) wParam == VK_BACK) ? LDK_TEXT_EVENT_BACKSPACE: LDK_TEXT_EVENT_CHARACTER_INPUT;
      }
      break;

    case WM_SIZE:
      {
        //WM_SIZE is sent a lot of times in a row and would easily overflow our
        //event buffer. We just update the last event if it is a LDK_EVENT_TYPE_WINDOW instead of adding a new event
        LDKEvent* e = NULL;
        u32 lastEventIndex = s_oswin32.events_count - 1;
        if (s_oswin32.events_count > 0 && s_oswin32.events[lastEventIndex].type == LDK_EVENT_TYPE_WINDOW)
        {
          if(s_oswin32.events[lastEventIndex].window_event.type == LDK_WINDOW_EVENT_RESIZED)
            e = &s_oswin32.events[lastEventIndex];
        }

        if (e == NULL)
          e = s_win32_event_new();

        e->type               = LDK_EVENT_TYPE_WINDOW;
        e->window             = window;
        e->window_event.type   = wParam == SIZE_MAXIMIZED ? LDK_WINDOW_EVENT_MAXIMIZED :
          wParam == SIZE_MINIMIZED ? LDK_WINDOW_EVENT_MINIMIZED : LDK_WINDOW_EVENT_RESIZED;
        e->window_event.width  = LOWORD(lParam);
        e->window_event.height = HIWORD(lParam);
      }
      break;

    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        i32 isDown = !(lParam & (1 << 31)); // 0 = pressed, 1 = released
        i32 wasDown = (lParam & (1 << 30)) !=0;
        i32 state = (((isDown ^ wasDown) << 1) | isDown);
        i16 vkCode = (i16) wParam;
        s_oswin32.keyboard_state.key[vkCode] = (u8) state;

        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_KEYBOARD;
        e->window = window;
        e->keyboard_event.type = (wasDown && !isDown) ?
          LDK_KEYBOARD_EVENT_KEY_UP : (!wasDown && isDown) ?
          LDK_KEYBOARD_EVENT_KEY_DOWN : LDK_KEYBOARD_EVENT_KEY_HOLD;

        e->keyboard_event.keyCode      = vkCode;
        e->keyboard_event.ctrl_is_down   = s_oswin32.keyboard_state.key[LDK_KEYCODE_CONTROL];
        e->keyboard_event.shift_is_down  = s_oswin32.keyboard_state.key[LDK_KEYCODE_SHIFT];
        e->keyboard_event.alt_is_down    = s_oswin32.keyboard_state.key[LDK_KEYCODE_ALT];
      }
      break;

    case WM_MOUSEWHEEL:
      {
        // A wheel event may be sent to the focus window, not necessarily the window under the cursor.
        // Becasue of that we must to explicitly convert it to client space first.
        // Also because of that I lost hours chasing a stupid IMGUI bug. Thanks Microsoft.
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);

        i32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
        s_oswin32.mouse_state.wheel_delta = delta;

        // update cursor position
        i32 last_x = s_oswin32.mouse_state.cursor.x;
        i32 last_y = s_oswin32.mouse_state.cursor.y;
        s_oswin32.mouse_state.cursor.x = pt.x;
        s_oswin32.mouse_state.cursor.y = pt.y;
        s_oswin32.mouse_state.cursor_relative.x = s_oswin32.mouse_state.cursor.x - last_x;
        s_oswin32.mouse_state.cursor_relative.y = s_oswin32.mouse_state.cursor.y - last_y;

        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_MOUSE_WHEEL;
        e->window = window;
        e->mouse_event.wheel_delta = delta;
        e->mouse_event.type = delta >= 0 ? LDK_MOUSE_EVENT_WHEEL_FORWARD : LDK_MOUSE_EVENT_WHEEL_BACKWARD;
        e->mouse_event.cursorX = pt.x;
        e->mouse_event.cursorY = pt.y;
        e->mouse_event.xRel    = s_oswin32.mouse_state.cursor_relative.x;
        e->mouse_event.yRel    = s_oswin32.mouse_state.cursor_relative.y;

        ScreenToClient(hwnd, &pt);
      }
      break;

    case WM_MOUSEMOVE:
      {
        i32 last_x = s_oswin32.mouse_state.cursor.x;
        i32 last_y = s_oswin32.mouse_state.cursor.y;
        s_oswin32.mouse_state.cursor.x = GET_X_LPARAM(lParam);
        s_oswin32.mouse_state.cursor.y = GET_Y_LPARAM(lParam); 
        s_oswin32.mouse_state.cursor_relative.x = s_oswin32.mouse_state.cursor.x - last_x;
        s_oswin32.mouse_state.cursor_relative.y = s_oswin32.mouse_state.cursor.y - last_y;

        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_MOUSE_MOVE;
        e->mouse_event.xRel    = e->mouse_event.cursorX - last_x;
        e->mouse_event.yRel    = e->mouse_event.cursorY - last_y;
        e->window = window;
        e->mouse_event.type    = LDK_MOUSE_EVENT_MOVE;
        e->mouse_event.cursorX = GET_X_LPARAM(lParam); 
        e->mouse_event.cursorY = GET_Y_LPARAM(lParam); 
        e->mouse_event.xRel    = s_oswin32.mouse_state.cursor_relative.x;
        e->mouse_event.yRel    = s_oswin32.mouse_state.cursor_relative.y;
      }
      break;

    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
      if (mouse_button_id == -1)
        mouse_button_id = GET_XBUTTON_WPARAM (wParam) == XBUTTON1 ? LDK_MOUSE_BUTTON_EXTRA_0 : LDK_MOUSE_BUTTON_EXTRA_1;
      return_value = TRUE;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
      if (mouse_button_id == -1)
        mouse_button_id = LDK_MOUSE_BUTTON_LEFT;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      if (mouse_button_id == -1)
        mouse_button_id = LDK_MOUSE_BUTTON_MIDDLE;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
      {
        if (mouse_button_id == -1)
          mouse_button_id = LDK_MOUSE_BUTTON_RIGHT;

        is_mouse_button_down_event = uMsg == WM_XBUTTONDOWN || uMsg == WM_LBUTTONDOWN || uMsg == WM_MBUTTONDOWN || uMsg == WM_RBUTTONDOWN;

        unsigned char* buttonLeft   = (unsigned char*) &(s_oswin32.mouse_state.button[LDK_MOUSE_BUTTON_LEFT]);
        unsigned char* buttonRight  = (unsigned char*) &(s_oswin32.mouse_state.button[LDK_MOUSE_BUTTON_RIGHT]);
        unsigned char* buttonMiddle = (unsigned char*) &(s_oswin32.mouse_state.button[LDK_MOUSE_BUTTON_MIDDLE]);
        unsigned char* buttonExtra1 = (unsigned char*) &(s_oswin32.mouse_state.button[LDK_MOUSE_BUTTON_EXTRA_0]);
        unsigned char* buttonExtra2 = (unsigned char*) &(s_oswin32.mouse_state.button[LDK_MOUSE_BUTTON_EXTRA_1]);
        unsigned char isDown, wasDown;

        isDown        = (unsigned char) ((wParam & MK_LBUTTON) > 0);
        wasDown       = *buttonLeft;
        *buttonLeft   = (((isDown ^ wasDown) << 1) | isDown);

        isDown        = (unsigned char) ((wParam & MK_RBUTTON) > 0);
        wasDown       = *buttonRight;
        *buttonRight  = (((isDown ^ wasDown) << 1) | isDown);

        isDown        = (unsigned char) ((wParam & MK_MBUTTON) > 0);
        wasDown       = *buttonMiddle;
        *buttonMiddle = (((isDown ^ wasDown) << 1) | isDown);

        isDown        = (unsigned char) ((wParam & MK_XBUTTON1) > 0);
        wasDown       = *buttonExtra1;
        *buttonExtra1 = (((isDown ^ wasDown) << 1) | isDown);

        isDown        = (unsigned char) ((wParam & MK_XBUTTON2) > 0);
        wasDown       = *buttonExtra2;
        *buttonExtra2 = (((isDown ^ wasDown) << 1) | isDown);

        // update cursor position
        i32 last_x = s_oswin32.mouse_state.cursor.x;
        i32 last_y = s_oswin32.mouse_state.cursor.y;
        s_oswin32.mouse_state.cursor.x = GET_X_LPARAM(lParam);
        s_oswin32.mouse_state.cursor.y = GET_Y_LPARAM(lParam); 
        s_oswin32.mouse_state.cursor_relative.x = s_oswin32.mouse_state.cursor.x - last_x;
        s_oswin32.mouse_state.cursor_relative.y = s_oswin32.mouse_state.cursor.y - last_y;

        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_MOUSE_BUTTON;
        e->window = window;
        e->mouse_event.type = is_mouse_button_down_event ? LDK_MOUSE_EVENT_BUTTON_DOWN : LDK_MOUSE_EVENT_BUTTON_UP;
        e->mouse_event.mouse_button = mouse_button_id;
        e->mouse_event.cursorX = GET_X_LPARAM(lParam);
        e->mouse_event.cursorY = GET_Y_LPARAM(lParam);
        e->mouse_event.xRel    = s_oswin32.mouse_state.cursor_relative.x;
        e->mouse_event.yRel    = s_oswin32.mouse_state.cursor_relative.y;
      }
      break;

    case WM_CLOSE:
      {
        LDKEvent* e = s_win32_event_new();
        e->type = LDK_EVENT_TYPE_WINDOW;
        e->window_event.type = LDK_WINDOW_EVENT_CLOSE;
        e->window = window;
      }
      break;
    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return return_value;
}


// ---------------------------------------------------------------------------
// Graphics
// ---------------------------------------------------------------------------

LDKGCtx ldk_os_graphics_context_opengl_create(i32 version_major, i32 version_minor, i32 color_bits, i32 depth_bits)
{
  s_opengl_init(WIN32_GRAPHICS_API_OPENGL, version_major, version_minor, color_bits, depth_bits);
  return &s_graphicsAPIInfo;
}

LDKGCtx ldk_os_graphics_context_opengles_create(i32 version_major, i32 version_minor, i32 color_bits, i32 depth_bits)
{
  s_opengl_init(WIN32_GRAPHICS_API_OPENGLES, version_major, version_minor, color_bits, depth_bits);
  return &s_graphicsAPIInfo;
}

void ldk_os_graphics_context_make_current(LDKWindow window, LDKGCtx context)
{
  HDC dc = ((LDKWin32Window*)window)->dc;
  X_ASSERT(context == &s_graphicsAPIInfo);
  if (s_graphics_api_is_opengl(s_graphicsAPIInfo.api))
    wglMakeCurrent(dc, s_graphicsAPIInfo.gl.rc);
}

bool ldk_os_graphics_vsync_set(bool vsync)
{
  if (s_graphics_api_is_opengl(s_graphicsAPIInfo.api))
  {
    if (s_graphicsAPIInfo.gl.wglSwapIntervalEXT)
      return s_graphicsAPIInfo.gl.wglSwapIntervalEXT(vsync);
  }
  return false;
}

i32 ldk_os_graphics_vsync_get(void)
{
  if (s_graphics_api_is_opengl(s_graphicsAPIInfo.api))
  {
    if (s_graphicsAPIInfo.gl.wglSwapIntervalEXT)
      return s_graphicsAPIInfo.gl.wglGetSwapIntervalEXT();
  }
  return false;
}


// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void ldk_os_mouse_state_get(LDKMouseState* outState)
{
  memcpy(outState, &s_oswin32.mouse_state, sizeof(LDKMouseState));
}

bool ldk_os_mouse_button_is_pressed(LDKMouseState* state, LDKMouseButton button)
{
  return (state->button[button] & LDK_MOUSE_PRESSED_BIT) == LDK_MOUSE_PRESSED_BIT;
}

bool ldk_os_mouse_button_down(LDKMouseState* state, LDKMouseButton button)
{
  u32 mask = LDK_MOUSE_PRESSED_BIT | LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
  return (state->button[button] & mask) == mask;
}

bool ldk_os_mouse_button_up(LDKMouseState* state, LDKMouseButton button)
{
  return state->button[button] == LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
}

LDKPoint ldk_os_mouse_cursor(LDKMouseState* state)
{
  return state->cursor;
}

LDKPoint ldk_os_mouse_cursor_relative(LDKMouseState* state)
{
  return state->cursor_relative;
}

i32 ldk_os_mouse_wheel_delta(LDKMouseState* state)
{
  return state->wheel_delta;
}


// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

void ldk_os_keyboard_state_get(LDKKeyboardState* outState)
{
  memcpy(outState, &s_oswin32.keyboard_state, sizeof(LDKKeyboardState));
}

bool ldk_os_keyboard_key_is_pressed(LDKKeyboardState* state, LDKKeycode keycode)
{
  return (state->key[keycode] & LDK_KEYBOARD_PRESSED_BIT) == LDK_KEYBOARD_PRESSED_BIT;
}

bool ldk_os_keyboard_key_down(LDKKeyboardState* state, LDKKeycode keycode)
{
  u32 mask = LDK_KEYBOARD_PRESSED_BIT | LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
  return (state->key[keycode] & mask) == mask;
}

bool ldk_os_keyboard_key_up(LDKKeyboardState* state, LDKKeycode keycode)
{
  return state->key[keycode] == LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
}


// ---------------------------------------------------------------------------
// Joystick
// ---------------------------------------------------------------------------

void ldk_os_joystick_state_get(LDKJoystickState* outState, LDKJoystickID id)
{
  X_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  memcpy(outState, &s_oswin32.joystick_state[id], sizeof(LDKJoystickState));
}

bool ldk_os_joystick_button_is_pressed(LDKJoystickState* state, LDKJoystickButton key)
{
  return 	state->connected && (state->button[key] & LDK_JOYSTICK_PRESSED_BIT) == LDK_JOYSTICK_PRESSED_BIT;
}

bool ldk_os_joystick_button_down(LDKJoystickState* state, LDKJoystickButton key)
{
  u32 mask = LDK_JOYSTICK_PRESSED_BIT | LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
  return 	state->connected && (state->button[key] & mask) == mask;
}

bool ldk_os_joystick_button_up(LDKJoystickState* state, LDKJoystickButton key)
{
  return state->connected && state->button[key] == LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
}

float ldk_os_joystick_axis_get(LDKJoystickState* state, LDKJoystickAxis axis)
{
  if (!state->connected)
    return 0.0f;

  return state->axis[axis];
}

u32 ldk_os_joystick_count(void)
{
  u32 count = 0;
  for (u32 i = 0; i < LDK_JOYSTICK_MAX; i++)
  {
    if (s_oswin32.joystick_state[i].connected)
      count++;
  }
  return count;
}

u32 ldk_os_joystick_is_connected(LDKJoystickID id)
{
  const bool connected = (s_oswin32.joystick_state[id].connected);
  return connected;
}

void ldk_os_joystick_vibration_left_set(LDKJoystickID id, float speed)
{
  X_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  if (!s_oswin32.joystick_state[id].connected)
    return;

  XINPUT_VIBRATION vibration;
  if (speed < 0.0f) speed = 0.0f;
  if (speed > 1.0f) speed = 1.0f;

  // we store speed as floats
  s_oswin32.joystick_state[id].vibration_left = speed;

  // xinput wants them as short int
  WORD short_int_speed_left = (WORD) (0xFFFF * speed);
  WORD short_int_speed_right = (WORD) (s_oswin32.joystick_state[id].vibration_right * 0XFFFF);

  vibration.wLeftMotorSpeed = short_int_speed_left;
  vibration.wRightMotorSpeed = short_int_speed_right;
  s_oswin32.xinput_set_state(id, &vibration);
}

void ldk_os_joystick_vibration_right_set(LDKJoystickID id, float speed)
{
  X_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  if (!s_oswin32.joystick_state[id].connected)
    return;

  XINPUT_VIBRATION vibration;
  if (speed < 0.0f) speed = 0.0f;
  if (speed > 1.0f) speed = 1.0f;

  // we store speed as floats
  s_oswin32.joystick_state[id].vibration_right = speed;

  // xinput wants them as short int
  WORD short_int_speed_left = (WORD) (s_oswin32.joystick_state[id].vibration_left * 0XFFFF);
  WORD short_int_speed_right = (WORD) (0xFFFF * speed);

  vibration.wLeftMotorSpeed = short_int_speed_left;
  vibration.wRightMotorSpeed = short_int_speed_right;
  s_oswin32.xinput_set_state(id, &vibration);
}

float ldk_os_joystick_vibration_left_get(LDKJoystickID id)
{
  X_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  return s_oswin32.joystick_state[id].vibration_left;
}

float ldk_os_joystick_vibration_right_get(LDKJoystickID id)
{
  X_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  return s_oswin32.joystick_state[id].vibration_right;
}

