#include "ldk/os.h"
#include "ldk/common.h"
#include "ldk/gl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

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


//
// Extern
//
extern void* ldkWin32OpenglProcAddressGet(char* name);
extern void ldkOpenglFunctionPointersGet();

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

typedef struct _XINPUT_VIBRATION {
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

DWORD s_xInputGetStateDUMMY(DWORD dwUserIndex, XINPUT_STATE *pState)
{
  ldkLogError("No XInput ...");
  return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD s_xInputSetStateDUMMY(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
{
  ldkLogError("No XInput ...");
  return ERROR_DEVICE_NOT_CONNECTED;
}

//
// Internal
//
typedef struct
{
  bool closeFlag;
  bool isFullscreen;
  bool cursorChanged;
  HWND handle;
  HDC dc;
  LONG_PTR prevStyle;
  WINDOWPLACEMENT prevPlacement;
} LDKWin32Window;

typedef struct 
{
  PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormatARB;
  PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
  PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT;
  PFNWGLGETSWAPINTERVALEXTPROC      wglGetSwapIntervalEXT;
  HGLRC   sharedContext;
  HGLRC   rc;
  uint32  versionMajor;
  uint32  versionMinor;
  int32   pixelFormatAttribs[16];
  int32   contextAttribs[16];
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
  LDKKeyboardState  keyboardState;
  LDKMouseState     mouseState;
  LDKJoystickState  joysickState[LDK_JOYSTICK_MAX];

  // Event queue
  LDKEvent events[LDK_WIN32_MAX_EVENTS];
  uint32 eventsCount;
  uint32 eventsPollIndex;

  // All windows
  LDKWindow allWindows[LDK_WIN32_MAX_WINDOWS];
  LDKWindow allHWND[LDK_WIN32_MAX_WINDOWS];
  uint32 windowCount;

  HCURSOR defaultCursor;
  XInputGetStateFunc XInputGetState;
  XInputSetStateFunc XInputSetState;
} s_oswin32;


static LRESULT s_windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static bool s_xInputInit(void)
{
  memset(&s_oswin32.joysickState, 0, sizeof(s_oswin32.joysickState));

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
    ldkLogError("could not initialize xinput. No valid xinput dll found");
    s_oswin32.XInputGetState = (XInputGetStateFunc) s_xInputGetStateDUMMY;
    s_oswin32.XInputSetState = (XInputSetStateFunc) s_xInputSetStateDUMMY;
    return false;
  }

  //get xinput function pointers
  s_oswin32.XInputGetState = (XInputGetStateFunc) GetProcAddress(hXInput, "XInputGetState");
  s_oswin32.XInputSetState = (XInputSetStateFunc) GetProcAddress(hXInput, "XInputSetState");

  if (!s_oswin32.XInputGetState)
    s_oswin32.XInputGetState = (XInputGetStateFunc) s_xInputGetStateDUMMY;

  if (!s_oswin32.XInputSetState)
    s_oswin32.XInputSetState = (XInputSetStateFunc) s_xInputSetStateDUMMY;
  return true;
}

static inline void s_XInputPollEvents(void)
{
  // get gamepad input
  for(int32 gamepadIndex = 0; gamepadIndex < LDK_JOYSTICK_MAX; gamepadIndex++)
  {
    XINPUT_STATE xinputState = {0};
    LDKJoystickState* joystickState = &s_oswin32.joysickState[gamepadIndex];

    // ignore unconnected controllers
    if (s_oswin32.XInputGetState(gamepadIndex, &xinputState) == ERROR_DEVICE_NOT_CONNECTED)
    {
      joystickState->connected = false;
      continue;
    }

    // digital buttons
    WORD buttons = xinputState.Gamepad.wButtons;
    uint8 isDown=0;
    uint8 wasDown=0;

    const uint32 pressedBit = LDK_JOYSTICK_PRESSED_BIT;

    // Buttons
    isDown = (buttons & XINPUT_GAMEPAD_DPAD_UP) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_UP] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_UP] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_LEFT] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_LEFT] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_RIGHT] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_DPAD_RIGHT] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_START) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_START] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_START] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_BACK) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_BACK] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_BACK] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_LEFT_THUMB) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_LEFT_THUMB] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_LEFT_THUMB] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_RIGHT_THUMB] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_RIGHT_THUMB] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_LEFT_SHOULDER] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_LEFT_SHOULDER] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_A) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_A] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_A] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_B) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_B] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_B] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_X) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_X] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_X] = ((isDown ^ wasDown) << 1) | isDown;

    isDown = (buttons & XINPUT_GAMEPAD_Y) > 0;
    wasDown = joystickState->button[LDK_JOYSTICK_BUTTON_Y] & pressedBit;
    joystickState->button[LDK_JOYSTICK_BUTTON_Y] = ((isDown ^ wasDown) << 1) | isDown;

#define GAMEPAD_AXIS_VALUE(value) (value/(float)(value < 0 ? XINPUT_MIN_AXIS_VALUE * -1: XINPUT_MAX_AXIS_VALUE))
#define GAMEPAD_AXIS_IS_DEADZONE(value, deadzone) ( value > -deadzone && value < deadzone)

    // Left thumb axis
    int32 axisX = xinputState.Gamepad.sThumbLX;
    int32 axisY = xinputState.Gamepad.sThumbLY;
    int32 deadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

    // TODO(marcio): Implement deadZone filtering correctly. This is not enough!
    joystickState->axis[LDK_JOYSTICK_AXIS_LX] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :
      GAMEPAD_AXIS_VALUE(axisX);

    joystickState->axis[LDK_JOYSTICK_AXIS_LY] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      GAMEPAD_AXIS_VALUE(axisY);

    // Right thumb axis
    axisX = xinputState.Gamepad.sThumbRX;
    axisY = xinputState.Gamepad.sThumbRY;
    deadZone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

    joystickState->axis[LDK_JOYSTICK_AXIS_RX] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :
      GAMEPAD_AXIS_VALUE(axisX);

    joystickState->axis[LDK_JOYSTICK_AXIS_RY] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      GAMEPAD_AXIS_VALUE(axisY);

    // Left trigger
    axisX = xinputState.Gamepad.bLeftTrigger;
    axisY = xinputState.Gamepad.bRightTrigger;
    deadZone = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

    joystickState->axis[LDK_JOYSTICK_AXIS_LTRIGGER] = GAMEPAD_AXIS_IS_DEADZONE(axisX, deadZone) ? 0.0f :	
      axisX/(float) XINPUT_MAX_TRIGGER_VALUE;

    joystickState->axis[LDK_JOYSTICK_AXIS_RTRIGGER] = GAMEPAD_AXIS_IS_DEADZONE(axisY, deadZone) ? 0.0f :	
      axisY/(float) XINPUT_MAX_TRIGGER_VALUE;

#undef GAMEPAD_AXIS_IS_DEADZONE
#undef GAMEPAD_AXIS_VALUE

    joystickState->connected = true;
  }
}

inline static bool s_GraphicsApiIsOpengl(Win32GraphicsAPI api)
{
  return (api == WIN32_GRAPHICS_API_OPENGL || api == WIN32_GRAPHICS_API_OPENGLES);
}

static LDKEvent* s_Win32EventNew(void)
{
  if (s_oswin32.eventsCount >= LDK_WIN32_MAX_EVENTS)
  {
    ldkLogError("Reached the maximum number of OS events %d.", LDK_WIN32_MAX_EVENTS);
    return NULL;
  }

  LDKEvent* eventPtr = &(s_oswin32.events[s_oswin32.eventsCount++]);
  return eventPtr;
}

inline static bool s_OpenglInit(Win32GraphicsAPI api, int32 glVersionMajor, int32 glVersionMinor, int32 colorBits, int32 depthBits)
{
  LDKWin32Window* dummyWindow =  ldkOsWindowCreate("", 0,0);
  bool isGLES = api == WIN32_GRAPHICS_API_OPENGLES;

  PIXELFORMATDESCRIPTOR pfd = { 
    sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    (BYTE)depthBits,
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0, 0, 0, 0,
    (BYTE)colorBits,
    0,
    0,
    PFD_MAIN_PLANE,
    0,
    0, 0, 0
  }; 

  int pixelFormat = ChoosePixelFormat(dummyWindow->dc, &pfd);
  if (! pixelFormat)
  {
    ldkLogError("Unable to allocate a pixel format");
    ldkOsWindowDestroy(dummyWindow);
    return false;
  }

  if (! SetPixelFormat(dummyWindow->dc, pixelFormat, &pfd))
  {
    ldkLogError("Unable to set a pixel format");
    ldkOsWindowDestroy(dummyWindow);
    return false;
  }

  HGLRC rc = wglCreateContext(dummyWindow->dc);
  if (! rc)
  {
    ldkLogError("Unable to create a valid OpenGL context");
    ldkOsWindowDestroy(dummyWindow);
    return false;
  }

  if (! wglMakeCurrent(dummyWindow->dc, rc))
  {
    ldkLogError("Unable to set OpenGL context current");
    ldkOsWindowDestroy(dummyWindow);
    return false;
  }

  const int pixelFormatAttribList[] =
  {
    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
    WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
    WGL_COLOR_BITS_ARB, colorBits,
    WGL_DEPTH_BITS_ARB, depthBits,
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

  const int contextAttribs[] =
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
  s_graphicsAPIInfo.gl.sharedContext = 0;
  s_graphicsAPIInfo.gl.versionMajor = glVersionMajor;
  s_graphicsAPIInfo.gl.versionMinor = glVersionMinor;
  s_graphicsAPIInfo.gl.wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC) wglGetProcAddress("wglChoosePixelFormatARB");
  s_graphicsAPIInfo.gl.wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");
  s_graphicsAPIInfo.gl.wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
  s_graphicsAPIInfo.gl.wglGetSwapIntervalEXT      = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");

  memcpy(s_graphicsAPIInfo.gl.pixelFormatAttribs, pixelFormatAttribList, sizeof(pixelFormatAttribList));
  memcpy(s_graphicsAPIInfo.gl.contextAttribs, contextAttribs, sizeof(contextAttribs));


  // Get function pointers
  ldkOpenglFunctionPointersGet();

  wglMakeCurrent(0, 0);
  wglDeleteContext(rc);
  ldkOsWindowDestroy(dummyWindow);
  return true;
}

inline static LDKWindow s_DKWindowFromWin32HWND(HWND hWnd)
{
  for (uint32 i =0; i < s_oswin32.windowCount; i++)
  {
    if ( s_oswin32.allHWND[i] == hWnd)
      return s_oswin32.allWindows[i];
  }

  return NULL;
}


//
// Initialization
//

bool ldkOsInitialize(void)
{
  //s_ = (LDKWin32s_*) ldkOsMemoryAlloc(sizeof(LDKWin32s_));
  //memset(s_oswin32, 0, sizeof(LDKWin32s_));
  QueryPerformanceFrequency(&s_oswin32.frequency);
  s_xInputInit();
  return true;
}

void ldkOsTerminate(void)
{
}

void ldkOsStackTracePrint(void)
{
#ifdef LDK_DEBUG
  static bool once = false;
  if(!once)
  {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    once = true;
  }

  const int maxStackTraceSize = LDK_WIN32_STACK_TRACE_SIZE_MAX;
  void* stackTrace[LDK_WIN32_STACK_TRACE_SIZE_MAX];
  USHORT frames = CaptureStackBackTrace(1, maxStackTraceSize, stackTrace, NULL);

  SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
  symbolInfo->MaxNameLen = 255;
  symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (USHORT i = 0; i < frames; i++)
  {
    SymFromAddr(GetCurrentProcess(), (DWORD64)(stackTrace[i]), 0, symbolInfo);

    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displacement;
    SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(stackTrace[i]), &displacement, &lineInfo);

#ifdef LDK_COMPILER_MSVC
    if (ldkStringEndsWith(symbolInfo->Name, "s_OnSignal") || ldkStringEndsWith(symbolInfo->Name, "seh_filter_exe"))
      continue;

    if (lineInfo.FileName && (ldkStringEndsWith(lineInfo.FileName, "vcstartup\\src\\startup\\exe_common.inl")
          || ldkStringEndsWith(lineInfo.FileName, "src\\vctools\\crt\\vcstartup\\src\\startup\\exe_main.cpp")
          || ldkStringEndsWith(lineInfo.FileName, "vctools\\crt\\vcstartup\\src\\rtc\\error.cpp")
          || ldkStringEndsWith(lineInfo.FileName, "vctools\\crt\\vcstartup\\src\\rtc\\stack.cpp")
          ))
    {
      continue;
    }
#endif
    // Print the stack trace information

    ldkLogPrintRaw("\tat %s(%s:%d)\n", symbolInfo->Name, (lineInfo.FileName ? lineInfo.FileName : "N/A"), lineInfo.LineNumber);
  }

  free(symbolInfo);
#endif
}


//
// Filesystem
//

bool ldkOsFileCreate(const char* path, const byte* data)
{
  HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  DWORD bytesWritten;
  bool success = WriteFile(hFile, data, (DWORD) strlen((const char*) data), &bytesWritten, NULL) != 0;
  CloseHandle(hFile);

  return success;
}

bool ldkOsFileDelete(const char* path)
{
  return DeleteFile(path) != 0;
}

byte* ldkOsFileRead(const char* path)
{
  HANDLE hFile = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
  {
    return NULL;
  }

  LARGE_INTEGER fileSize;
  GetFileSizeEx(hFile, &fileSize);

  byte* buffer = (byte*) ldkOsMemoryAlloc(fileSize.QuadPart);
  if (buffer == NULL)
  {
    CloseHandle(hFile);
    return NULL;
  }

  DWORD bytesRead;
  if (!ReadFile(hFile, buffer, (DWORD)fileSize.QuadPart, &bytesRead, NULL) || bytesRead != fileSize.QuadPart)
  {
    ldkOsMemoryFree(buffer);
    CloseHandle(hFile);
    return NULL;
  }

  CloseHandle(hFile);
  return buffer;
}

byte* ldkOsFileReadOffset(const char* path, size_t* outFileSize, size_t extraSize, size_t offset)
{
  HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
  {
    DWORD error = GetLastError();

    ldkLogError("Could not open file '%s': Error %d (0x%x)", path, error, error);
    return NULL;
  }

  LARGE_INTEGER fileSize;
  GetFileSizeEx(hFile, &fileSize);

  if (outFileSize != NULL)
    *outFileSize = fileSize.QuadPart;

  if((offset + fileSize.QuadPart) > (fileSize.QuadPart + extraSize))
  {
    ldkLogError("Could not read file '%s' into buffer at offset %lu because fize size (%lu) + offset would overflow the buffer size",
        path, offset, fileSize.QuadPart);
    CloseHandle(hFile);
    return NULL;
  }

  byte* buffer = (byte*) ldkOsMemoryAlloc(fileSize.QuadPart + extraSize);
  if (buffer == NULL)
  {
    CloseHandle(hFile);
    return NULL;
  }

  DWORD bytesRead;
  if (!ReadFile(hFile, buffer + offset, (DWORD)fileSize.QuadPart, &bytesRead, NULL) || bytesRead != fileSize.QuadPart)
  {
    ldkOsMemoryFree(buffer);
    CloseHandle(hFile);
    return NULL;
  }

  CloseHandle(hFile);
  return buffer;
}

LDKFile ldkOsFileOpen(const char* path, LDKFileOpenFlags flags)
{
  DWORD accessFlags = 0;
  DWORD shareMode = 0;

  if (flags & FILE_OPEN_FLAG_READ)
  {
    accessFlags |= GENERIC_READ;
    shareMode |= FILE_SHARE_READ;
  }

  if (flags & FILE_OPEN_FLAG_WRITE)
  {
    accessFlags |= GENERIC_WRITE;
    shareMode |= 0; // Don't share for writing
  }

  HANDLE hFile = CreateFile(path, accessFlags, shareMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  return (LDKFile)hFile;
}

bool ldkOsFileCLose(LDKFile file)
{
  return CloseHandle((HANDLE)file) != 0;
}

bool ldkOsFileCopy(const char* file, const char* newFile)
{
  return CopyFile(file, newFile, FALSE) != 0;
}

bool ldkOsFileRename(const char* file, const char* newFile)
{
  return MoveFile(file, newFile) != 0;
}

bool ldkOsDirectoryCreate(const char* path)
{
  return CreateDirectory(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ldkOsDirectoryCreateRecursive(const char* path)
{
  size_t length = strlen(path);
  if (length >= LDK_PATH_MAX_LENGTH)
  {
    ldkLogError("Path is too long (%d). Maximum size is %d. - '%s'", length, LDK_PATH_MAX_LENGTH, path);
    return false;
  }

  char tempPath[LDK_PATH_MAX_LENGTH];
  strncpy((char *) &tempPath, path, length);
  tempPath[length] = 0;

  char* p = (char*) &tempPath;

  // If the path is absolute, skip the drive letter and colon
  if (p[1] == ':') { p += 2; }

  while (*p)
  {
    if (*p == '\\' || *p == '/')
    {
      *p = '\0';
      if (!CreateDirectory((const char*) tempPath, NULL))
      {
        if (GetLastError() != ERROR_ALREADY_EXISTS) { return false; }
      }
      *p = '\\';
    }
    p++;
  }

  return true;
}

bool ldkOsDirectoryDelete(const char* directory)
{
  return RemoveDirectory(directory) != 0;
}

size_t ldkOsCwdGet(LDKPath* path)
{
  DWORD size = GetCurrentDirectory(LDK_PATH_MAX_LENGTH, path->path);
  return (size_t)size;
}

size_t ldkOsCwdSet(const char* path)
{
  return SetCurrentDirectory(path) != 0;
}

size_t ldkOsCwdSetFromExecutablePath(void)
{
  LDKPath programPath;
  size_t bytesCopied = ldkOsExecutablePathGet(&programPath);
  ldkOsCwdSet(programPath.path);
  return bytesCopied;
}

bool ldkOsPathIsFile(const char* path)
{
  DWORD attributes = GetFileAttributes(path);
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool ldkOsPathIsDirectory(char* path)
{
  DWORD attributes = GetFileAttributes(path);
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}


//
// Memory
//

void* ldkOsMemoryAlloc(size_t size)
{
  void* mem = malloc(size);
  if(mem == NULL)
  {
    ldkLogError("Memmory allocation failed");
    ldkOsStackTracePrint();
  }
  return mem;
}

void ldkOsMemoryFree(void* memory)
{
  free(memory);
}

void* ldkOsMemoryResize(void* memory, size_t size)
{
  void* mem = realloc(memory, size);
  if(mem == NULL)
  {
    ldkLogError("Memmory allocation failed");
    ldkOsStackTracePrint();
  }

  return mem;
}


//
// Time
//

uint64 ldkOsTimeTicksGet(void)
{
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return counter.QuadPart;
}

double ldkOsTimeTicksIntervalGetSeconds(uint64 start, uint64 end)
{
  return ((end - start) / (double) s_oswin32.frequency.QuadPart);
}

double ldkOsTimeTicksIntervalGetMilliseconds(uint64 start, uint64 end)
{
  double difference = (end - start) * 1000.0;
  return (difference / s_oswin32.frequency.QuadPart);
}

double ldkOsTimeTicksIntervalGetNanoseconds(uint64 start, uint64 end)
{
  double difference = (end - start) * 1000000000.0;
  return (difference / s_oswin32.frequency.QuadPart);
}

//
// System Date and Time
//

void ldkOsSystemDateTimeGet(LDKDateTime* outDateTime)
{
  SYSTEMTIME win32SysTime;
  GetSystemTime(&win32SysTime);
  outDateTime->year         = win32SysTime.wYear;
  outDateTime->month        = win32SysTime.wMonth;
  outDateTime->dayOfWeek    = win32SysTime.wDayOfWeek;
  outDateTime->day          = win32SysTime.wDay;
  outDateTime->hour         = win32SysTime.wHour;
  outDateTime->minute       = win32SysTime.wMinute;
  outDateTime->second       = win32SysTime.wSecond;
  outDateTime->milliseconds = win32SysTime.wMilliseconds;
}


//
// Windowing
//

void LDK_API ldkOsGraphicsContextDestroy(LDKGCtx context)
{
  LDK_ASSERT(context == &s_graphicsAPIInfo);

  bool isOpenGL = s_GraphicsApiIsOpengl(s_graphicsAPIInfo.api);
  if (isOpenGL)
  {
    wglDeleteContext(s_graphicsAPIInfo.gl.rc);
    s_graphicsAPIInfo.api = WIN32_GRAPHICS_API_NONE;
  }
}

void ldkOsWindowDestroy(LDKWindow window)
{
  uint32 lastWindowIndex = s_oswin32.windowCount - 1;
  for (uint32 i =0; i < s_oswin32.windowCount; i++)
  {
    if (s_oswin32.allWindows[i] == window)
    {
      if (i != lastWindowIndex)
      {
        s_oswin32.allWindows[i] = s_oswin32.allWindows[lastWindowIndex];
        s_oswin32.allHWND[i] = s_oswin32.allHWND[lastWindowIndex];
      }

      s_oswin32.allWindows[lastWindowIndex] = NULL;
      s_oswin32.allHWND[lastWindowIndex] = NULL;
      s_oswin32.windowCount--;
      break;
    }
  }

  LDKWin32Window* ldkWindow = ((LDKWin32Window*)window);
  DeleteDC(ldkWindow->dc);
  DestroyWindow(ldkWindow->handle);
  ldkOsMemoryFree(window);
}

LDKWindow ldkOsWindowCreateWithFlags(const char* title, int32 width, int32 height, LDKWindowFlags flags)
{
  if (s_oswin32.windowCount > LDK_WIN32_MAX_WINDOWS)
  {
    ldkLogError("Exceeded maximum number of windows %d", LDK_WIN32_MAX_WINDOWS);
    return NULL;
  }

  const char* ldkWindowClass = "LDK_WINDOW_CLASS";
  HINSTANCE hInstance = GetModuleHandleA(NULL);
  WNDCLASSEXA wc = {0};

  // Calculate total window size
  RECT clientArea = {(LONG)0,(LONG)0, (LONG)width, (LONG)height};
  if (!AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, FALSE))
  {
    ldkLogError("Could not calculate window size", 0);
  }

  if (! GetClassInfoExA(hInstance, ldkWindowClass, &wc))
  {
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = s_windowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = ldkWindowClass;

    // Do not try registering the class multiple times
    if (! RegisterClassExA(&wc))
    {
      ldkLogError("Could not register window class", 0);
      return NULL;
    }
  }

  DWORD windowStyle = WS_OVERLAPPEDWINDOW;
  if (flags & LDK_WINDOW_FLAG_NORESIZE)
    windowStyle &= ~WS_SIZEBOX;

  uint32 windowWidth = clientArea.right - clientArea.left;
  uint32 windowHeight = clientArea.bottom - clientArea.top;
  HWND windowHandle = CreateWindowExA(
      0,
      ldkWindowClass,
      title, 
      windowStyle,
      CW_USEDEFAULT, CW_USEDEFAULT,
      windowWidth, windowHeight,
      NULL, NULL,
      hInstance,
      NULL);

  if (! windowHandle)
  {
    ldkLogError("Could not create a window", 0);
    return NULL;
  }

  DWORD showFlag = (flags & LDK_WINDOW_FLAG_NORMAL) ? SW_SHOWNORMAL : 0;
  showFlag |= (flags & LDK_WINDOW_FLAG_MINIMIZED) ? SW_SHOWMINIMIZED : 0;
  showFlag |= (flags & LDK_WINDOW_FLAG_MAXIMIZED) ? SW_SHOWMAXIMIZED : 0;
  ShowWindow(windowHandle, showFlag);

  LDKWin32Window* window = ldkOsMemoryAlloc(sizeof(LDKWin32Window));
  window->handle = windowHandle;
  window->dc = GetDC(windowHandle);
  window->isFullscreen = false;
  window->closeFlag = false;

  bool isOpenGL = s_GraphicsApiIsOpengl(s_graphicsAPIInfo .api);
  if(isOpenGL)
  {
    int pixelFormat;
    int numPixelFormats = 0;
    PIXELFORMATDESCRIPTOR pfd;

    const int* pixelFormatAttribList  = (const int*) s_graphicsAPIInfo .gl.pixelFormatAttribs;
    const int* contextAttribList      = (const int*) s_graphicsAPIInfo.gl.contextAttribs;

    s_graphicsAPIInfo.gl.wglChoosePixelFormatARB(window->dc,
        pixelFormatAttribList,
        NULL,
        1,
        &pixelFormat,
        (UINT*) &numPixelFormats);

    if (numPixelFormats <= 0)
    {
      ldkLogError("Unable to get a valid pixel format", 0);
      return NULL;
    }

    if (! SetPixelFormat(window->dc, pixelFormat, &pfd))
    {
      ldkLogError("Unable to set a pixel format", 0);
      return NULL;
    }

    HGLRC sharedContext = s_graphicsAPIInfo.gl.sharedContext;
    HGLRC rc = s_graphicsAPIInfo.gl.wglCreateContextAttribsARB(window->dc, sharedContext, contextAttribList);

    // The first context created will be used as a shared context for the rest
    // of the program execution
    if (! sharedContext)
    {
      s_graphicsAPIInfo.gl.sharedContext = rc;
    }

    if (! rc)
    {
      ldkLogError("Unable to create a valid OpenGL context", 0);
      return NULL;
    }

    s_graphicsAPIInfo.gl.rc = rc;
    if (! wglMakeCurrent(window->dc, rc))
    {
      ldkLogError("Unable to set OpenGL context current", 0);
      return NULL;
    }
  }

  if (s_oswin32.defaultCursor == 0)
    s_oswin32.defaultCursor = LoadCursor(NULL, IDC_ARROW);

  SetCursor(s_oswin32.defaultCursor);
  uint32 windowIndex = s_oswin32.windowCount++;
  s_oswin32.allWindows[windowIndex] = window;
  s_oswin32.allHWND[windowIndex] = window->handle;
  return window;
}

LDKWindow ldkOsWindowCreate(const char* title, int32 width, int32 height)
{
  LDKWindowFlags defaultFlags = LDK_WINDOW_FLAG_NORMAL;
  return ldkOsWindowCreateWithFlags(title, width, height, defaultFlags);
}

bool ldkOsWindowShouldClose(LDKWindow window)
{
  return ((LDKWin32Window*)window)->closeFlag;
}

bool ldkOsEventsPoll(LDKEvent* event)
{
  MSG msg;

  if (s_oswin32.eventsCount == 0)
  {
    // clean up changed bit for keyboard keys
    for(int keyCode = 0; keyCode < LDK_KEYBOARD_MAX_KEYS; keyCode++)
    {
      s_oswin32.keyboardState.key[keyCode] &= ~LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
    }

    // clean up changed bit for mouse buttons
    for(int button = 0; button <  LDK_MOUSE_MAX_BUTTONS; button++)
    {
      s_oswin32.mouseState.button[button] &= ~LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
    }

    // reset wheel delta
    s_oswin32.mouseState.wheelDelta = 0;
    s_oswin32.mouseState.cursorRelative.x = 0;
    s_oswin32.mouseState.cursorRelative.y = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  // clean up changed bit for joystick buttons
  for (uint32 joystickId = 0; joystickId < LDK_JOYSTICK_MAX; joystickId++)
  {
    for(int button = 0; button < LDK_MOUSE_MAX_BUTTONS; button++)
    {
      s_oswin32.joysickState[joystickId].button[button] &= ~LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
    }
  }

  s_XInputPollEvents();

  // WindowProc might have enqueued some events...
  if (s_oswin32.eventsPollIndex < s_oswin32.eventsCount)
  {
    memcpy(event, &s_oswin32.events[s_oswin32.eventsPollIndex++], sizeof(LDKEvent));
    return true;
  }
  else
  {
    s_oswin32.eventsCount = 0;
    s_oswin32.eventsPollIndex = 0;
    event->type = LDK_EVENT_TYPE_NONE;
    return false;
  }
}

void ldkOsWindowBuffersSwap(LDKWindow window)
{
  SwapBuffers(((LDKWin32Window*)window)->dc);
}

bool ldkOsWindowFullscreenGet(LDKWindow window)
{
  return ((LDKWin32Window*)window)->isFullscreen;
}

bool ldkOsWindowFullscreenSet(LDKWindow window, bool fs)
{
  LDKWin32Window* win32Window = ((LDKWin32Window*)window);
  HWND hWnd = win32Window->handle;

  if (fs && !win32Window->isFullscreen) // Enter full screen
  {
    // Get the monitor's handle
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    // Get the monitor's info
    MONITORINFO monitorInfo = { sizeof(monitorInfo) };
    GetMonitorInfo(hMonitor, &monitorInfo);
    // Save the window's current style and position
    win32Window->prevStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
    win32Window->prevPlacement = win32Window->prevPlacement;
    GetWindowPlacement(hWnd, &win32Window->prevPlacement);

    // Set the window style to full screen
    SetWindowLongPtr(hWnd, GWL_STYLE, win32Window->prevStyle & ~(WS_CAPTION | WS_THICKFRAME));
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
    win32Window->isFullscreen = true;
  }
  else if (!fs && win32Window->isFullscreen) // Exit full screen
  {
    // restore window previous style and location
    SetWindowLongPtr(hWnd, GWL_STYLE, win32Window->prevStyle);
    SetWindowPlacement(hWnd, &win32Window->prevPlacement);
    ShowWindow(hWnd, SW_RESTORE);
    win32Window->isFullscreen = false;
  }
  return true;
}

LDKSize ldkOsWindowClientAreaSizeGet(LDKWindow window)
{
  HWND handle = ((LDKWin32Window*)window)->handle;
  RECT rect;
  GetClientRect(handle, &rect);
  LDKSize size = { .width = rect.right, .height = rect.bottom };
  return size;
}

LDKPoint ldkOsWindowPositionGet(LDKWindow window)
{
  HWND handle = ((LDKWin32Window*)window)->handle;
  RECT rect;
  GetClientRect(handle, &rect);
  LDKPoint position = { .x = rect.right, .y = rect.bottom };
  return position;
}

void ldkOsWindowPositionSet(LDKWindow window, int32 x, int32 y)
{
  HWND windowHandle = ((LDKWin32Window*)window)->handle;
  SetWindowPos(windowHandle, NULL, x, y, 0, 0, SWP_NOSIZE);
}

void ldkOsWindowSizeSet(LDKWindow window, int32 width, int32 height)
{
  HWND windowHandle = ((LDKWin32Window*)window)->handle;
  SetWindowPos(windowHandle, NULL, 0, 0, width, height, SWP_NOMOVE);
}

LDKSize ldkOsWindowSizeGet(LDKWindow window)
{
  RECT rect;
  HWND handle = ((LDKWin32Window*)window)->handle;
  GetWindowRect(handle, &rect);
  LDKSize size = { rect.right - rect.left, rect.bottom - rect.top };
  return size;
}

void ldkOsWindowClientAreaSizeSet(LDKWindow window, int32 width, int32 height)
{
  RECT clientArea = {(LONG)0,(LONG)0, (LONG)width, (LONG)height};
  if (!AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, FALSE))
    return;

  uint32 windowWidth = clientArea.right - clientArea.left;
  uint32 windowHeight = clientArea.bottom - clientArea.top;
  ldkOsWindowSizeSet(window, windowWidth, windowHeight);
}

void ldkOsWindowTitleSet(LDKWindow window, const char* title)
{
  HWND windowHandle = ((LDKWin32Window*)window)->handle;
  SetWindowTextA(windowHandle, title);
}

size_t ldkOsWindowTitleGet(LDKWindow window, LDKSmallStr* outTitle)
{
  size_t length = GetWindowTextLengthA(window);

  if (length >= LDK_SMALL_STRING_MAX_LENGTH)
    return length;

  HWND windowHandle = ((LDKWin32Window*)window)->handle;
  GetWindowTextA(windowHandle, (char*) &outTitle->str, LDK_SMALL_STRING_MAX_LENGTH);
  return 0;
}

bool ldkOsWindowIconSet(LDKWindow window, const char* iconPath)
{
  HWND windowHandle = ((LDKWin32Window*)window)->handle;
  HICON hIcon = (HICON)LoadImage(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

  if (hIcon == NULL) {
    ldkLogError("Failed to load icon '%s'", iconPath);
    return FALSE;
  }

  SendMessage(windowHandle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
  SendMessage(windowHandle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
  return TRUE;
}

size_t ldkOsExecutablePathGet(LDKPath* ldkPath)
{
  size_t len = GetModuleFileNameA(NULL, (char*) &ldkPath->path, LDK_PATH_MAX_LENGTH);
  char* end = ldkPath->path + len;

  while (end > (char*) &ldkPath->path && *end != '\\'  && *end != '/' )
  {
    *end = 0;
    len--;
    end--;
  }

  ldkPath->length = len;
  return len;
}


size_t ldkOsExecutablePathFileNameGet(LDKPath* ldkPath)
{
  size_t len = GetModuleFileNameA(NULL, (void*) &ldkPath->path, LDK_PATH_MAX_LENGTH);
  if(len > 0 && len < LDK_PATH_MAX_LENGTH)
    ldkPath->length = len;

  return len;
}

static LRESULT s_windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  bool isMouseButtonDownEvent = false;
  int32 mouseButtonId = -1;
  LRESULT returnValue = FALSE;

  LDKWin32Window* window = (LDKWin32Window*) s_DKWindowFromWin32HWND(hwnd);

  switch(uMsg) 
  {
    case WM_NCHITTEST:
      {
        // Get the default behaviour but set the arrow cursor if it's in the client area
        LRESULT result = DefWindowProc(hwnd, uMsg, wParam, lParam);
        if(result == HTCLIENT && window->cursorChanged)
        {
          window->cursorChanged = false;
          SetCursor(s_oswin32.defaultCursor);
        }
        else
          window->cursorChanged = true;
        return result;
      }
      break;

    case WM_ACTIVATE:
      {
        bool activate = (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE);
        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_WINDOW;
        e->windowEvent.type = activate ? LDK_WINDOW_EVENT_ACTIVATE : LDK_WINDOW_EVENT_DEACTIVATE;
        e->window = window;
      }
      break;
    case WM_CHAR:
      {
        LDKEvent* e = s_Win32EventNew();
        e->type                = LDK_EVENT_TYPE_TEXT;
        e->window = window;
        e->textEvent.character = (uint32) wParam;
        e->textEvent.type      = ((uint32) wParam == VK_BACK) ? LDK_TEXT_EVENT_BACKSPACE: LDK_TEXT_EVENT_CHARACTER_INPUT;
      }
      break;

    case WM_SIZE:
      {
        //WM_SIZE is sent a lot of times in a row and would easily overflow our
        //event buffer. We just update the last event if it is a LDK_EVENT_TYPE_WINDOW instead of adding a new event
        LDKEvent* e = NULL;
        uint32 lastEventIndex = s_oswin32.eventsCount - 1;
        if (s_oswin32.eventsCount > 0 && s_oswin32.events[lastEventIndex].type == LDK_EVENT_TYPE_WINDOW)
        {
          if(s_oswin32.events[lastEventIndex].windowEvent.type == LDK_WINDOW_EVENT_RESIZED)
            e = &s_oswin32.events[lastEventIndex];
        }

        if (e == NULL)
          e = s_Win32EventNew();

        e->type               = LDK_EVENT_TYPE_WINDOW;
        e->window             = window;
        e->windowEvent.type   = wParam == SIZE_MAXIMIZED ? LDK_WINDOW_EVENT_MAXIMIZED :
          wParam == SIZE_MINIMIZED ? LDK_WINDOW_EVENT_MINIMIZED : LDK_WINDOW_EVENT_RESIZED;
        e->windowEvent.width  = LOWORD(lParam);
        e->windowEvent.height = HIWORD(lParam);
      }
      break;

    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        int32 isDown = !(lParam & (1 << 31)); // 0 = pressed, 1 = released
        int32 wasDown = (lParam & (1 << 30)) !=0;
        int32 state = (((isDown ^ wasDown) << 1) | isDown);
        int16 vkCode = (int16) wParam;
        s_oswin32.keyboardState.key[vkCode] = (uint8) state;

        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_KEYBOARD;
        e->window = window;
        e->keyboardEvent.type = (wasDown && !isDown) ?
          LDK_KEYBOARD_EVENT_KEY_UP : (!wasDown && isDown) ?
          LDK_KEYBOARD_EVENT_KEY_DOWN : LDK_KEYBOARD_EVENT_KEY_HOLD;

        e->keyboardEvent.keyCode      = vkCode;
        e->keyboardEvent.ctrlIsDown   = s_oswin32.keyboardState.key[LDK_KEYCODE_CONTROL];
        e->keyboardEvent.shiftIsDown  = s_oswin32.keyboardState.key[LDK_KEYCODE_SHIFT];
        e->keyboardEvent.altIsDown    = s_oswin32.keyboardState.key[LDK_KEYCODE_ALT];
      }
      break;

    case WM_MOUSEWHEEL:
      {
        int32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
        s_oswin32.mouseState.wheelDelta = delta;

        // update cursor position
        int32 lastX = s_oswin32.mouseState.cursor.x;
        int32 lastY = s_oswin32.mouseState.cursor.y;
        s_oswin32.mouseState.cursor.x = GET_X_LPARAM(lParam);
        s_oswin32.mouseState.cursor.y = GET_Y_LPARAM(lParam); 
        s_oswin32.mouseState.cursorRelative.x = s_oswin32.mouseState.cursor.x - lastX;
        s_oswin32.mouseState.cursorRelative.y = s_oswin32.mouseState.cursor.y - lastY;

        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_WHEEL;
        e->window = window;
        e->mouseEvent.wheelDelta = delta;
        e->mouseEvent.type = delta >= 0 ? LDK_MOUSE_EVENT_WHEEL_FORWARD : LDK_MOUSE_EVENT_WHEEL_BACKWARD;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam); 
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam); 
        e->mouseEvent.xRel    = s_oswin32.mouseState.cursorRelative.x;
        e->mouseEvent.yRel    = s_oswin32.mouseState.cursorRelative.y;
      }
      break;

    case WM_MOUSEMOVE:
      {
        int32 lastX = s_oswin32.mouseState.cursor.x;
        int32 lastY = s_oswin32.mouseState.cursor.y;
        s_oswin32.mouseState.cursor.x = GET_X_LPARAM(lParam);
        s_oswin32.mouseState.cursor.y = GET_Y_LPARAM(lParam); 
        s_oswin32.mouseState.cursorRelative.x = s_oswin32.mouseState.cursor.x - lastX;
        s_oswin32.mouseState.cursorRelative.y = s_oswin32.mouseState.cursor.y - lastY;

        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_MOVE;
        e->mouseEvent.xRel    = e->mouseEvent.cursorX - lastX;
        e->mouseEvent.yRel    = e->mouseEvent.cursorY - lastY;
        e->window = window;
        e->mouseEvent.type = LDK_MOUSE_EVENT_MOVE;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam); 
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam); 
        e->mouseEvent.xRel    = s_oswin32.mouseState.cursorRelative.x;
        e->mouseEvent.yRel    = s_oswin32.mouseState.cursorRelative.y;
      }
      break;

    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
      if (mouseButtonId == -1)
        mouseButtonId = GET_XBUTTON_WPARAM (wParam) == XBUTTON1 ? LDK_MOUSE_BUTTON_EXTRA_0 : LDK_MOUSE_BUTTON_EXTRA_1;
      returnValue = TRUE;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
      if (mouseButtonId == -1)
        mouseButtonId = LDK_MOUSE_BUTTON_LEFT;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      if (mouseButtonId == -1)
        mouseButtonId = LDK_MOUSE_BUTTON_MIDDLE;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
      {
        if (mouseButtonId == -1)
          mouseButtonId = LDK_MOUSE_BUTTON_RIGHT;

        isMouseButtonDownEvent = uMsg == WM_XBUTTONDOWN || uMsg == WM_LBUTTONDOWN || uMsg == WM_MBUTTONDOWN || uMsg == WM_RBUTTONDOWN;

        unsigned char* buttonLeft   = (unsigned char*) &(s_oswin32.mouseState.button[LDK_MOUSE_BUTTON_LEFT]);
        unsigned char* buttonRight  = (unsigned char*) &(s_oswin32.mouseState.button[LDK_MOUSE_BUTTON_RIGHT]);
        unsigned char* buttonMiddle = (unsigned char*) &(s_oswin32.mouseState.button[LDK_MOUSE_BUTTON_MIDDLE]);
        unsigned char* buttonExtra1 = (unsigned char*) &(s_oswin32.mouseState.button[LDK_MOUSE_BUTTON_EXTRA_0]);
        unsigned char* buttonExtra2 = (unsigned char*) &(s_oswin32.mouseState.button[LDK_MOUSE_BUTTON_EXTRA_1]);
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
        int32 lastX = s_oswin32.mouseState.cursor.x;
        int32 lastY = s_oswin32.mouseState.cursor.y;
        s_oswin32.mouseState.cursor.x = GET_X_LPARAM(lParam);
        s_oswin32.mouseState.cursor.y = GET_Y_LPARAM(lParam); 
        s_oswin32.mouseState.cursorRelative.x = s_oswin32.mouseState.cursor.x - lastX;
        s_oswin32.mouseState.cursorRelative.y = s_oswin32.mouseState.cursor.y - lastY;

        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_BUTTON;
        e->window = window;
        e->mouseEvent.type = isMouseButtonDownEvent ? LDK_MOUSE_EVENT_BUTTON_DOWN : LDK_MOUSE_EVENT_BUTTON_UP;
        e->mouseEvent.mouseButton = mouseButtonId;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam);
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam);
        e->mouseEvent.xRel    = s_oswin32.mouseState.cursorRelative.x;
        e->mouseEvent.yRel    = s_oswin32.mouseState.cursorRelative.y;
      }
      break;

    case WM_CLOSE:
      {
        LDKEvent* e = s_Win32EventNew();
        e->type = LDK_EVENT_TYPE_WINDOW;
        e->windowEvent.type = LDK_WINDOW_EVENT_CLOSE;
        e->window = window;
      }
      break;
    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return returnValue;
}


//
// Graphics
//

LDKGCtx ldkOsGraphicsContextOpenglCreate(int32 versionMajor, int32 versionMinor, int32 colorBits, int32 depthBits)
{
  s_OpenglInit(WIN32_GRAPHICS_API_OPENGL, versionMajor, versionMinor, colorBits, depthBits);
  return &s_graphicsAPIInfo;
}

LDKGCtx ldkOsGraphicsContextOpenglesCreate(int32 versionMajor, int32 versionMinor, int32 colorBits, int32 depthBits)
{
  s_OpenglInit(WIN32_GRAPHICS_API_OPENGLES, versionMajor, versionMinor, colorBits, depthBits);
  return &s_graphicsAPIInfo;
}

void ldkOsGraphicsContextCurrent(LDKWindow window, LDKGCtx context)
{
  HDC dc = ((LDKWin32Window*)window)->dc;
  LDK_ASSERT(context == &s_graphicsAPIInfo);
  if (s_GraphicsApiIsOpengl(s_graphicsAPIInfo.api))
    wglMakeCurrent(dc, s_graphicsAPIInfo.gl.rc);
}

bool ldkOsGraphicsVSyncSet(bool vsync)
{
  if (s_GraphicsApiIsOpengl(s_graphicsAPIInfo.api))
  {
    if (s_graphicsAPIInfo.gl.wglSwapIntervalEXT)
      return s_graphicsAPIInfo.gl.wglSwapIntervalEXT(vsync);
  }
  return false;
}

int32 ldkOsGraphicsVSyncGet(void)
{
  if (s_GraphicsApiIsOpengl(s_graphicsAPIInfo.api))
  {
    if (s_graphicsAPIInfo.gl.wglSwapIntervalEXT)
      return s_graphicsAPIInfo.gl.wglGetSwapIntervalEXT();
  }
  return false;
}

//
// Mouse
//

void ldkOsMouseStateGet(LDKMouseState* outState)
{
  memcpy(outState, &s_oswin32.mouseState, sizeof(LDKMouseState));
}

bool ldkOsMouseButtonIsPressed(LDKMouseState* state, LDKMouseButton button)
{
  return (state->button[button] & LDK_MOUSE_PRESSED_BIT) == LDK_MOUSE_PRESSED_BIT;
}

bool ldkOsMouseButtonDown(LDKMouseState* state, LDKMouseButton button)
{
  uint32 mask = LDK_MOUSE_PRESSED_BIT | LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
  return (state->button[button] & mask) == mask;
}

bool ldkOsMouseButtonUp(LDKMouseState* state, LDKMouseButton button)
{
  return state->button[button] == LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
}

LDKPoint ldkOsMouseCursor(LDKMouseState* state)
{
  return state->cursor;
}

LDKPoint ldkOsMouseCursorRelative(LDKMouseState* state)
{
  return state->cursorRelative;
}

int32 ldkOsMouseWheelDelta(LDKMouseState* state)
{
  return state->wheelDelta;
}

//
// Keyboard
//

void ldkOsKeyboardStateGet(LDKKeyboardState* outState)
{
  memcpy(outState, &s_oswin32.keyboardState, sizeof(LDKKeyboardState));
}

bool ldkOsKeyboardKeyIsPressed(LDKKeyboardState* state, LDKKeycode keycode)
{
  return (state->key[keycode] & LDK_KEYBOARD_PRESSED_BIT) == LDK_KEYBOARD_PRESSED_BIT;
}

bool ldkOsKeyboardKeyDown(LDKKeyboardState* state, LDKKeycode keycode)
{
  uint32 mask = LDK_KEYBOARD_PRESSED_BIT | LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
  return (state->key[keycode] & mask) == mask;
}

bool ldkOsKeyboardKeyUp(LDKKeyboardState* state, LDKKeycode keycode)
{
  return state->key[keycode] == LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
}

//
// Joystick
//

void ldkOsJoystickGetState(LDKJoystickState* outState, LDKJoystickID id)
{
  LDK_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  memcpy(outState, &s_oswin32.joysickState[id], sizeof(LDKJoystickState));
}

bool ldkOsJoystickButtonIsPressed(LDKJoystickState* state, LDKJoystickButton key)
{
  return 	state->connected && (state->button[key] & LDK_JOYSTICK_PRESSED_BIT) == LDK_JOYSTICK_PRESSED_BIT;
}

bool ldkOsJoystickButtonDown(LDKJoystickState* state, LDKJoystickButton key)
{
  uint32 mask = LDK_JOYSTICK_PRESSED_BIT | LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
  return 	state->connected && (state->button[key] & mask) == mask;
}

bool ldkOsJoystickButtonUp(LDKJoystickState* state, LDKJoystickButton key)
{
  return state->connected && state->button[key] == LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT;
}

float ldkOsJoystickAxisGet(LDKJoystickState* state, LDKJoystickAxis axis)
{
  if (!state->connected)
    return 0.0f;

  return state->axis[axis];
}

uint32 ldkOsJoystickCount(void)
{
  uint32 count = 0;
  for (uint32 i = 0; i < LDK_JOYSTICK_MAX; i++)
  {
    if (s_oswin32.joysickState[i].connected)
      count++;
  }
  return count;
}

uint32 ldkOsJoystickIsConnected(LDKJoystickID id)
{
  const bool connected = (s_oswin32.joysickState[id].connected);
  return connected;
}

void ldkOsJoystickVibrationLeftSet(LDKJoystickID id, float speed)
{
  LDK_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  if (!s_oswin32.joysickState[id].connected)
    return;

  XINPUT_VIBRATION vibration;
  if (speed < 0.0f) speed = 0.0f;
  if (speed > 1.0f) speed = 1.0f;

  // we store speed as floats
  s_oswin32.joysickState[id].vibrationLeft = speed;

  // xinput wants them as short int
  WORD shortIntSpeedLeft = (WORD) (0xFFFF * speed);
  WORD shortIntSpeedRight = (WORD) (s_oswin32.joysickState[id].vibrationRight * 0XFFFF);

  vibration.wLeftMotorSpeed = shortIntSpeedLeft;
  vibration.wRightMotorSpeed = shortIntSpeedRight;
  s_oswin32.XInputSetState(id, &vibration);
}

void ldkOsJoystickVibrationRightSet(LDKJoystickID id, float speed)
{
  LDK_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  if (!s_oswin32.joysickState[id].connected)
    return;

  XINPUT_VIBRATION vibration;
  if (speed < 0.0f) speed = 0.0f;
  if (speed > 1.0f) speed = 1.0f;

  // we store speed as floats
  s_oswin32.joysickState[id].vibrationRight = speed;

  // xinput wants them as short int
  WORD shortIntSpeedLeft = (WORD) (s_oswin32.joysickState[id].vibrationLeft * 0XFFFF);
  WORD shortIntSpeedRight = (WORD) (0xFFFF * speed);

  vibration.wLeftMotorSpeed = shortIntSpeedLeft;
  vibration.wRightMotorSpeed = shortIntSpeedRight;
  s_oswin32.XInputSetState(id, &vibration);
}

float ldkOsJoystickVibrationLeftGet(LDKJoystickID id)
{
  LDK_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  return s_oswin32.joysickState[id].vibrationLeft;
}

float ldkOsJoystickVibrationRightGet(LDKJoystickID id)
{
  LDK_ASSERT( id == LDK_JOYSTICK_0 || id == LDK_JOYSTICK_1 || id == LDK_JOYSTICK_2 || id == LDK_JOYSTICK_3);
  return s_oswin32.joysickState[id].vibrationRight;
}

