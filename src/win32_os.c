#include "ldk/os.h"
#include "ldk/gl.h"
#include "ldk/common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

#ifndef LDK_WIN32_MAX_EVENTS
#define LDK_WIN32_MAX_EVENTS 64
#endif

#ifndef LDK_WIN32_MAX_WINDOWS
#define LDK_WIN32_MAX_WINDOWS 64
#endif


//
// Extern
//
extern void* ldkWin32OpenglProcAddressGet(char* name);

extern void ldkOpenglFunctionPointersGet();


//
// Internal
//

static LRESULT internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
} win32GraphicsAPIInfo = {0};

static struct 
{
  LARGE_INTEGER frequency;
  bool timeInitialized;

  // Keyboard state
  LDKKeyboardState keyboardState;
  LDKMouseState mouseState;

  // Event queue
  LDKEvent events[LDK_WIN32_MAX_EVENTS];
  uint32 eventsCount;
  uint32 eventsPollIndex;

  // All windows
  LDKWindow allWindows[LDK_WIN32_MAX_WINDOWS];
  LDKWindow allHWND[LDK_WIN32_MAX_WINDOWS];
  uint32 windowCount;

  HCURSOR defaultCursor;

} internal = {0};

inline static bool internalGraphicsApiIsOpengl(Win32GraphicsAPI api)
{
  return (api == WIN32_GRAPHICS_API_OPENGL || api == WIN32_GRAPHICS_API_OPENGLES);
}

static LDKEvent* internalWin32EventNew()
{
  if (internal.eventsCount >= LDK_WIN32_MAX_EVENTS)
  {
    ldkLogError("Reached the maximum number of OS events %d.", LDK_WIN32_MAX_EVENTS);
    return NULL;
  }

  LDKEvent* eventPtr = &(internal.events[internal.eventsCount++]);
  return eventPtr;
}

inline static bool internalOpenglInit(Win32GraphicsAPI api, int32 glVersionMajor, int32 glVersionMinor, int32 colorBits, int32 depthBits)
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
  win32GraphicsAPIInfo.api = api;
  win32GraphicsAPIInfo.gl.sharedContext = 0;
  win32GraphicsAPIInfo.gl.versionMajor = glVersionMajor;
  win32GraphicsAPIInfo.gl.versionMinor = glVersionMinor;
  win32GraphicsAPIInfo.gl.wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC) wglGetProcAddress("wglChoosePixelFormatARB");
  win32GraphicsAPIInfo.gl.wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");
  win32GraphicsAPIInfo.gl.wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
  win32GraphicsAPIInfo.gl.wglGetSwapIntervalEXT      = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");

  memcpy(win32GraphicsAPIInfo.gl.pixelFormatAttribs, pixelFormatAttribList, sizeof(pixelFormatAttribList));
  memcpy(win32GraphicsAPIInfo.gl.contextAttribs, contextAttribs, sizeof(contextAttribs));


  // Get function pointers
  ldkOpenglFunctionPointersGet();
    
  wglMakeCurrent(0, 0);
  wglDeleteContext(rc);
  ldkOsWindowDestroy(dummyWindow);
  return true;
}

inline static LDKWindow internaLDKWindowFromWin32HWND(HWND hWnd)
{
  for (uint32 i =0; i < internal.windowCount; i++)
  {
    if ( internal.allHWND[i] == hWnd)
      return internal.allWindows[i];
  }

  return NULL;
}


//
// Filesystem
//

bool ldkOsFileCreate(char* path, const byte* data)
{
  HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  DWORD bytesWritten;
  bool success = WriteFile(hFile, data, (DWORD) strlen((const char*) data), &bytesWritten, NULL) != 0;
  CloseHandle(hFile);

  return success;
}

bool ldkOsFileDelete(char* path)
{
  return DeleteFile(path) != 0;
}

byte* ldkOsFileRead(char* path)
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

LDKFile ldkOsFileOpen(char* path, LDKFileOpenFlags flags)
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

bool ldkOsFileCopy(char* file, char* newFile)
{
  return CopyFile(file, newFile, FALSE) != 0;
}

bool ldkOsFileRename(char* file, char* newFile)
{
  return MoveFile(file, newFile) != 0;
}

bool ldkOsDirectoryCreate(char* path)
{
  return CreateDirectory(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ldkOsDirectoryCreateRecursive(char* path)
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

bool ldkOsDirectoryDelete(char* directory)
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

size_t ldkOsCwdSetFromExecutablePath()
{
  LDKPath programPath;
  size_t bytesCopied = ldkOsExecutablePathGet(&programPath);
  ldkOsCwdSet(programPath.path);
  return bytesCopied;
}

bool ldkOsPathIsFile(char* path)
{
  DWORD attributes = GetFileAttributes(path);
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool ldkOsPathIsDirectoryu(char* path)
{
  DWORD attributes = GetFileAttributes(path);
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}


//
// Memory
//

void* ldkOsMemoryAlloc(size_t size)
{
  return malloc(size);
}

void ldkOsMemoryFree(void* memory)
{
  free(memory);
}

void* ldkOsMemoryResize(void* memory, size_t size)
{
  return realloc(memory, size);
}


//
// Time
//

uint64 ldkOsTimeTicksGet()
{
  if (!internal.timeInitialized)
  {
    QueryPerformanceFrequency(&internal.frequency);
    internal.timeInitialized = true;
  }

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return counter.QuadPart;
}

double ldkOsTimeTicksIntervalGetSeconds(uint64 start, uint64 end)
{
  return ((end - start) / (double) internal.frequency.QuadPart);
}

double ldkOsTimeTicksIntervalGetMilliseconds(uint64 start, uint64 end)
{
  double difference = (double)((end - start) * 1000);
  return (difference / internal.frequency.QuadPart);
}

double ldkOsTimeTicksIntervalGetNanoseconds(uint64 start, uint64 end)
{
  double difference = (double)((end - start) * 1000000000);
  return (difference / internal.frequency.QuadPart);
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
  outDateTime->Second       = win32SysTime.wSecond;
  outDateTime->milliseconds = win32SysTime.wMilliseconds;
}


//
// Windowing
//

void LDK_API ldkOsGraphicsContextDestroy(LDKGCtx context)
{
  LDK_ASSERT(context == &win32GraphicsAPIInfo);

  bool isOpenGL = internalGraphicsApiIsOpengl(win32GraphicsAPIInfo.api);
  if (isOpenGL)
  {
    wglDeleteContext(win32GraphicsAPIInfo.gl.rc);
    win32GraphicsAPIInfo.api = WIN32_GRAPHICS_API_NONE;
  }
}

void ldkOsWindowDestroy(LDKWindow window)
{
  uint32 lastWindowIndex = internal.windowCount - 1;
  for (uint32 i =0; i < internal.windowCount; i++)
  {
    if (internal.allWindows[i] == window)
    {
      if (i != lastWindowIndex)
      {
        internal.allWindows[i] = internal.allWindows[lastWindowIndex];
        internal.allHWND[i] = internal.allHWND[lastWindowIndex];
      }

      internal.allWindows[lastWindowIndex] = NULL;
      internal.allHWND[lastWindowIndex] = NULL;
      internal.windowCount--;
      break;
    }
  }

  LDKWin32Window* ldkWindow = ((LDKWin32Window*)window);
  DeleteDC(ldkWindow->dc);
  DestroyWindow(ldkWindow->handle);
  ldkOsMemoryFree(window);
}

LDKWindow ldkOsWindowCreateWithFlags(char* title, int32 width, int32 height, LDKWindowFlags flags)
{
  if (internal.windowCount > LDK_WIN32_MAX_WINDOWS)
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
    wc.lpfnWndProc = internalWindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = ldkWindowClass;

    // Do not try registering the class multiple times
    if (! RegisterClassExA(&wc))
    {
      ldkLogError("Could not register window class", 0);
      return nullptr;
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
    return nullptr;
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

  bool isOpenGL = internalGraphicsApiIsOpengl(win32GraphicsAPIInfo .api);
  if(isOpenGL)
  {
    int pixelFormat;
    int numPixelFormats = 0;
    PIXELFORMATDESCRIPTOR pfd;

    const int* pixelFormatAttribList  = (const int*) win32GraphicsAPIInfo .gl.pixelFormatAttribs;
    const int* contextAttribList      = (const int*) win32GraphicsAPIInfo.gl.contextAttribs;

    win32GraphicsAPIInfo.gl.wglChoosePixelFormatARB(window->dc,
        pixelFormatAttribList,
        nullptr,
        1,
        &pixelFormat,
        (UINT*) &numPixelFormats);

    if (numPixelFormats <= 0)
    {
      ldkLogError("Unable to get a valid pixel format", 0);
      return nullptr;
    }

    if (! SetPixelFormat(window->dc, pixelFormat, &pfd))
    {
      ldkLogError("Unable to set a pixel format", 0);
      return nullptr;
    }

    HGLRC sharedContext = win32GraphicsAPIInfo.gl.sharedContext;
    HGLRC rc = win32GraphicsAPIInfo.gl.wglCreateContextAttribsARB(window->dc, sharedContext, contextAttribList);

    // The first context created will be used as a shared context for the rest
    // of the program execution
    bool mustGetGLFunctions = false;
    if (! sharedContext)
    {
      win32GraphicsAPIInfo.gl.sharedContext = rc;
      mustGetGLFunctions = true;
    }

    if (! rc)
    {
      ldkLogError("Unable to create a valid OpenGL context", 0);
      return nullptr;
    }

    win32GraphicsAPIInfo.gl.rc = rc;
    if (! wglMakeCurrent(window->dc, rc))
    {
      ldkLogError("Unable to set OpenGL context current", 0);
      return nullptr;
    }
  }

  if (internal.defaultCursor == 0)
    internal.defaultCursor = LoadCursor(NULL, IDC_ARROW);

  SetCursor(internal.defaultCursor);
  uint32 windowIndex = internal.windowCount++;
  internal.allWindows[windowIndex] = window;
  internal.allHWND[windowIndex] = window->handle;
  return window;
}

LDKWindow ldkOsWindowCreate(char* title, int32 width, int32 height)
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

  if (internal.eventsCount == 0)
  {
    // clean up changed bit for keyboard keys
    for(int keyCode = 0; keyCode < LDK_KEYBOARD_MAX_KEYS; keyCode++)
    {
      internal.keyboardState.key[keyCode] &= ~LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT;
    }

    // clean up changed bit for mouse buttons
    for(int button = 0; button <  LDK_MOUSE_MAX_BUTTONS; button++)
    {
      internal.mouseState.button[button] &= ~LDK_MOUSE_CHANGED_THIS_FRAME_BIT;
    }

    // reset wheel delta
    internal.mouseState.wheelDelta = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  // WindowProc might have enqueued some events...
  if (internal.eventsPollIndex < internal.eventsCount)
  {
    memcpy(event, &internal.events[internal.eventsPollIndex++], sizeof(LDKEvent));
    return true;
  }
  else
  {
    internal.eventsCount = 0;
    internal.eventsPollIndex = 0;
    event->type = LDK_EVENT_NONE;
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

static LRESULT internalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  bool isMouseButtonDownEvent = false;
  int32 mouseButtonId = -1;
  LRESULT returnValue = FALSE;

  LDKWin32Window* window = (LDKWin32Window*) internaLDKWindowFromWin32HWND(hwnd);

  switch(uMsg) 
  {
    case WM_NCHITTEST:
      {
        // Get the default behaviour but set the arrow cursor if it's in the client area
        LRESULT result = DefWindowProc(hwnd, uMsg, wParam, lParam);
        if(result == HTCLIENT && window->cursorChanged)
        {
          window->cursorChanged = false;
          SetCursor(internal.defaultCursor);
        }
        else
          window->cursorChanged = true;
        return result;
      }
      break;

    case WM_ACTIVATE:
      {
        bool activate = (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE);
        LDKEvent* e = internalWin32EventNew();
        e->type = LDK_EVENT_TYPE_WINDOW;
        e->windowEvent.type = activate ? LDK_WINDOW_EVENT_ACTIVATE : LDK_WINDOW_EVENT_DEACTIVATE;
        e->window = window;
      }
      break;
    case WM_CHAR:
      {
        LDKEvent* e = internalWin32EventNew();
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
        uint32 lastEventIndex = internal.eventsCount - 1;
        if (internal.eventsCount > 0 && internal.events[lastEventIndex].type == LDK_EVENT_TYPE_WINDOW)
        {
          if(internal.events[lastEventIndex].windowEvent.type == LDK_WINDOW_EVENT_RESIZED)
            e = &internal.events[lastEventIndex];
        }

        if (e == NULL)
          e = internalWin32EventNew();

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
        internal.keyboardState.key[vkCode] = (uint8) state;

        LDKEvent* e = internalWin32EventNew();
        e->type = LDK_EVENT_TYPE_KEYBOARD;
        e->window = window;
        e->keyboardEvent.type = (wasDown && !isDown) ?
          LDK_KEYBOARD_EVENT_KEY_UP : (!wasDown && isDown) ?
          LDK_KEYBOARD_EVENT_KEY_DOWN : LDK_KEYBOARD_EVENT_KEY_HOLD;

        e->keyboardEvent.keyCode      = vkCode;
        e->keyboardEvent.ctrlIsDown   = internal.keyboardState.key[LDK_KEYCODE_CONTROL];
        e->keyboardEvent.shiftIsDown  = internal.keyboardState.key[LDK_KEYCODE_SHIFT];
        e->keyboardEvent.altIsDown    = internal.keyboardState.key[LDK_KEYCODE_ALT];
      }
      break;

    case WM_MOUSEWHEEL:
      {
        int32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
        internal.mouseState.wheelDelta = delta;

        // update cursor position
        internal.mouseState.cursor.x = GET_X_LPARAM(lParam);
        internal.mouseState.cursor.y = GET_Y_LPARAM(lParam); 

        LDKEvent* e = internalWin32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_WHEEL;
        e->window = window;
        e->mouseEvent.wheelDelta = delta;
        e->mouseEvent.type = delta >= 0 ? LDK_MOUSE_EVENT_WHEEL_FORWARD : LDK_MOUSE_EVENT_WHEEL_BACKWARD;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam); 
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam); 
      }
      break;

    case WM_MOUSEMOVE:
      {
        // update cursor position
        internal.mouseState.cursor.x = GET_X_LPARAM(lParam);
        internal.mouseState.cursor.y = GET_Y_LPARAM(lParam);

        LDKEvent* e = internalWin32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_MOVE;
        e->window = window;
        e->mouseEvent.type = LDK_MOUSE_EVENT_MOVE;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam); 
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam); 
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

        unsigned char* buttonLeft   = (unsigned char*) &(internal.mouseState.button[LDK_MOUSE_BUTTON_LEFT]);
        unsigned char* buttonRight  = (unsigned char*) &(internal.mouseState.button[LDK_MOUSE_BUTTON_RIGHT]);
        unsigned char* buttonMiddle = (unsigned char*) &(internal.mouseState.button[LDK_MOUSE_BUTTON_MIDDLE]);
        unsigned char* buttonExtra1 = (unsigned char*) &(internal.mouseState.button[LDK_MOUSE_BUTTON_EXTRA_0]);
        unsigned char* buttonExtra2 = (unsigned char*) &(internal.mouseState.button[LDK_MOUSE_BUTTON_EXTRA_1]);
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
        internal.mouseState.cursor.x = GET_X_LPARAM(lParam);
        internal.mouseState.cursor.y = GET_Y_LPARAM(lParam);

        LDKEvent* e = internalWin32EventNew();
        e->type = LDK_EVENT_TYPE_MOUSE_BUTTON;
        e->window = window;
        e->mouseEvent.type = isMouseButtonDownEvent ? LDK_MOUSE_EVENT_BUTTON_DOWN : LDK_MOUSE_EVENT_BUTTON_UP;
        e->mouseEvent.mouseButton = mouseButtonId;
        e->mouseEvent.cursorX = GET_X_LPARAM(lParam);
        e->mouseEvent.cursorY = GET_Y_LPARAM(lParam);
      }
      break;

    case WM_CLOSE:
      {
        LDKEvent* e = internalWin32EventNew();
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
  internalOpenglInit(WIN32_GRAPHICS_API_OPENGL, versionMajor, versionMinor, colorBits, depthBits);
  return &win32GraphicsAPIInfo;
}

LDKGCtx ldkOsGraphicsContextOpenglesCreate(int32 versionMajor, int32 versionMinor, int32 colorBits, int32 depthBits)
{
  internalOpenglInit(WIN32_GRAPHICS_API_OPENGLES, versionMajor, versionMinor, colorBits, depthBits);
  return &win32GraphicsAPIInfo;
}

void ldkOsGraphicsContextCurrent(LDKWindow window, LDKGCtx context)
{
  HDC dc = ((LDKWin32Window*)window)->dc;
  LDK_ASSERT(context == &win32GraphicsAPIInfo);
  if (internalGraphicsApiIsOpengl(win32GraphicsAPIInfo.api))
    wglMakeCurrent(dc, win32GraphicsAPIInfo.gl.rc);
}

bool ldkOsGraphicsVSyncSet(bool vsync)
{
  if (internalGraphicsApiIsOpengl(win32GraphicsAPIInfo.api))
  {
    if (win32GraphicsAPIInfo.gl.wglSwapIntervalEXT)
      return win32GraphicsAPIInfo.gl.wglSwapIntervalEXT(vsync);
  }
  return false;
}

int32 ldkOsGraphicsVSyncGet()
{
  if (internalGraphicsApiIsOpengl(win32GraphicsAPIInfo.api))
  {
    if (win32GraphicsAPIInfo.gl.wglSwapIntervalEXT)
      return win32GraphicsAPIInfo.gl.wglGetSwapIntervalEXT();
  }
  return false;
}


//
// Keyboard
//

void ldkOsKeyboardStateGet(LDKKeyboardState* outState)
{
  memcpy(outState, &internal.keyboardState, sizeof(LDKKeyboardState));
}
