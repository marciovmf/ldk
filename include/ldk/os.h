/**
 *
 * os.h
 * 
 * Provides OS specific functionality for:
 * - Filesystem
 * - Memory
 * - Timer, Time and date
 * - Graphics API initialization
 * - Windowing
 *
 */

#ifndef LDK_OS_H
#define LDK_OS_H

#include "common.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* Filesytem */
  typedef void* LDKFile;

  typedef enum
  {
    FILE_OPEN_FLAG_READ		= 1,
    FILE_OPEN_FLAG_WRITE	= 1 << 1,
  } LDKFileOpenFlags;


  //
  // Initialization / Termination
  //

  LDK_API bool ldkOsInitialize();
  LDK_API void ldkOsTerminate();
  LDK_API void ldkOsStackTracePrint();

  //
  // Filesytem
  //
  LDK_API byte*   ldkOsFileRead(const char* path);
  LDK_API byte*   ldkOsFileReadOffset(const char* path, size_t* outFileSize, size_t extraSize, size_t offset);
  LDK_API bool    ldkOsFileCreate(const char* path, const byte* data);
  LDK_API bool    ldkOsFileDelete(const char* path);
  LDK_API LDKFile ldkOsFileOpen(const char* path, LDKFileOpenFlags flags);
  LDK_API bool    ldkOsFileClose(LDKFile file);
  LDK_API bool    ldkOsFileCopy(const char* file, const char* newFile);
  LDK_API bool    ldkOsFileRename(const char* file, const char* newFile);
  LDK_API bool    ldkOsDirectoryCreate(const char* path);
  LDK_API bool    ldkOsDirectoryCreateRecursive(const char* path); // path can be absolute or relative. If path does not end with a path separator, it's assumed to be a file path and the file portion will be ignored.
  LDK_API bool    ldkOsDirectoryDelete(const char* directory);

  //
  // Cwd
  //
  LDK_API size_t  ldkOsCwdGet(LDKPath* path);
  LDK_API size_t  ldkOsCwdSet(const char* path);
  LDK_API size_t  ldkOsCwdSetFromExecutablePath();

  //
  // Path
  //
  LDK_API bool    ldkOsPathIsFile(const char* path);
  LDK_API bool    ldkOsPathIsDirectory(const char* path);

  //
  // Memory
  //
  LDK_API void*   ldkOsMemoryAlloc(size_t size);
  LDK_API void    ldkOsMemoryFree(void* memory);
  LDK_API void*   ldkOsMemoryResize(void* memory, size_t size);

  //
  // Time
  //
  LDK_API uint64  ldkOsTimeTicksGet();
  LDK_API double  ldkOsTimeTicksIntervalGetSeconds(uint64 start, uint64 end);
  LDK_API double  ldkOsTimeTicksIntervalGetMilliseconds(uint64 start, uint64 end);
  LDK_API double  ldkOsTimeTicksIntervalGetNanoseconds(uint64 start, uint64 end);

  //
  // System Date and Time
  //
  typedef struct {
    uint16 year;
    uint16 month;
    uint16 dayOfWeek;
    uint16 day;
    uint16 hour;
    uint16 minute;
    uint16 Second;
    uint16 milliseconds;
  } LDKDateTime;

  LDK_API void ldkOsSystemDateTimeGet(LDKDateTime* outDateTime);

  //
  // Windowing
  //
  typedef enum
  {
    LDK_WINDOW_FLAG_NORMAL              = 1 << 1,
    LDK_WINDOW_FLAG_MAXIMIZED           = 1 << 2,
    LDK_WINDOW_FLAG_MINIMIZED           = 1 << 3,
    LDK_WINDOW_FLAG_NORESIZE            = 1 << 4,
  } LDKWindowFlags;

  LDK_API bool      ldkOsEventsPoll(LDKEvent* event);
  LDK_API LDKWindow ldkOsWindowCreateWithFlags(const char* title, int32 width, int32 height, LDKWindowFlags flags);
  LDK_API LDKWindow ldkOsWindowCreate(const char* title, int32 width, int32 height);
  LDK_API bool      ldkOsWindowShouldClose(LDKWindow window);
  LDK_API void      ldkOsWindowBuffersSwap(LDKWindow window);
  LDK_API void      ldkOsWindowDestroy(LDKWindow window);
  LDK_API bool      ldkOsWindowFullscreenGet(LDKWindow window);
  LDK_API bool      ldkOsWindowFullscreenSet(LDKWindow window, bool fs);
  LDK_API LDKSize   ldkOsWindowClientAreaSizeGet(LDKWindow window);
  LDK_API void      ldkOsWindowClientAreaSizeSet(LDKWindow window, int width, int height);
  LDK_API LDKPoint  ldkOsWindowPositionGet(LDKWindow window);
  LDK_API void      ldkOsWindowPositionSet(LDKWindow window, int32 x, int32 y);
  LDK_API void      ldkOsWindowSizeSet(LDKWindow window, int32 width, int32 height);
  LDK_API LDKSize   ldkOsWindowSizeGet(LDKWindow window);
  LDK_API void      ldkOsWindowTitleSet(LDKWindow window, const char* title);
  LDK_API size_t    ldkOsWindowTitleGet(LDKWindow window, LDKSmallStr* outTitle);
  LDK_API bool      ldkOsWindowIconSet(LDKWindow window, const char* iconPath);

  //
  // Graphics
  //
  typedef void* LDKGCtx;
  LDK_API LDKGCtx ldkOsGraphicsContextOpenglCreate(int32 versionMajor, int32 versionMinor, int32 colorBits, int32 depthBits);
  LDK_API LDKGCtx ldkOsGraphicsContextOpenglesCreate(int32 versionMajor, int32 versionMinor, int32 colorBits, int32 depthBits);
  LDK_API void    ldkOsGraphicsContextCurrent(LDKWindow window, LDKGCtx context);
  LDK_API void    ldkOsGraphicsContextDestroy(LDKGCtx context);
  LDK_API bool    ldkOsGraphicsVSyncSet(bool vsync);
  LDK_API int32   ldkOsGraphicsVSyncGet();

  //
  // Misc
  //
  LDK_API size_t ldkOsExecutablePathGet(LDKPath* ldkPath);
  LDK_API size_t ldkOsExecutablePathFileNameGet(LDKPath* ldkPath);

  //
  // Mouse
  //
  typedef enum
  {
    LDK_MOUSE_BUTTON_LEFT     = 0,
    LDK_MOUSE_BUTTON_RIGHT    = 1,
    LDK_MOUSE_BUTTON_MIDDLE   = 2,
    LDK_MOUSE_BUTTON_EXTRA_0  = 3,
    LDK_MOUSE_BUTTON_EXTRA_1  = 4,

    LDK_MOUSE_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_MOUSE_PRESSED_BIT     = 1,
    LDK_MOUSE_MAX_BUTTONS     = 5
  } LDKMouseButton;

  // LDKMouseState
  typedef struct 
  {
    int32 wheelDelta;
    LDKPoint cursor;
    unsigned char button[LDK_MOUSE_MAX_BUTTONS];
  } LDKMouseState;

  LDK_API void ldkOsMouseStateGet(LDKMouseState* outState);
  LDK_API bool ldkOsMouseButtonIsPressed(LDKMouseState* state, LDKMouseButton button);  // True while button is pressed
  LDK_API bool ldkOsMouseButtonDown(LDKMouseState* state, LDKMouseButton button);    // True in the frame the button was pressed
  LDK_API bool ldkOsMouseButtonUp(LDKMouseState* state, LDKMouseButton button);      // True in the frame the button was released

  //
  // Keyboard
  //
#define LDK_KEYBOARD_KEYCODES \
  X(LDK_KEYCODE_INVALID, "", 0x00) \
  X(LDK_KEYCODE_BACKSPACE, "BACK", 0x08) \
  X(LDK_KEYCODE_TAB, "TAB", 0x09) \
  X(LDK_KEYCODE_CLEAR, "CLEAR", 0x0C) \
  X(LDK_KEYCODE_RETURN, "RETURN", 0x0D) \
  X(LDK_KEYCODE_SHIFT, "SHIFT", 0x10) \
  X(LDK_KEYCODE_CONTROL, "CONTROL", 0x11) \
  X(LDK_KEYCODE_ALT, "ALT", 0x12) \
  X(LDK_KEYCODE_PAUSE, "PAUSE", 0x13) \
  X(LDK_KEYCODE_CAPITAL, "CAPITAL", 0x14) \
  X(LDK_KEYCODE_ESCAPE, "ESCAPE", 0x1B) \
  X(LDK_KEYCODE_CONVERT, "CONVERT", 0x1C) \
  X(LDK_KEYCODE_NONCONVERT, "NONCONVERT", 0x1D) \
  X(LDK_KEYCODE_ACCEPT, "ACCEPT", 0x1E) \
  X(LDK_KEYCODE_MODECHANGE, "MODECHANGE", 0x1F) \
  X(LDK_KEYCODE_SPACE, "SPACE", 0x20) \
  X(LDK_KEYCODE_PRIOR, "PRIOR", 0x21) \
  X(LDK_KEYCODE_NEXT, "NEXT", 0x22) \
  X(LDK_KEYCODE_END, "END", 0x23) \
  X(LDK_KEYCODE_HOME, "HOME", 0x24) \
  X(LDK_KEYCODE_LEFT, "LEFT", 0x25) \
  X(LDK_KEYCODE_UP, "UP", 0x26) \
  X(LDK_KEYCODE_RIGHT, "RIGHT", 0x27) \
  X(LDK_KEYCODE_DOWN, "DOWN", 0x28) \
  X(LDK_KEYCODE_SELECT, "SELECT", 0x29) \
  X(LDK_KEYCODE_PRINT, "PRINT", 0x2A) \
  X(LDK_KEYCODE_EXECUTE, "EXECUTE", 0x2B) \
  X(LDK_KEYCODE_SNAPSHOT, "SNAPSHOT", 0x2C) \
  X(LDK_KEYCODE_INSERT, "INSERT", 0x2D) \
  X(LDK_KEYCODE_DELETE, "DELETE", 0x2E) \
  X(LDK_KEYCODE_HELP, "HELP", 0x2F) \
  X(LDK_KEYCODE_0, "0", 0x30) \
  X(LDK_KEYCODE_1, "1", 0x31) \
  X(LDK_KEYCODE_2, "2", 0x32) \
  X(LDK_KEYCODE_3, "3", 0x33) \
  X(LDK_KEYCODE_4, "4", 0x34) \
  X(LDK_KEYCODE_5, "5", 0x35) \
  X(LDK_KEYCODE_6, "6", 0x36) \
  X(LDK_KEYCODE_7, "7", 0x37) \
  X(LDK_KEYCODE_8, "8", 0x38) \
  X(LDK_KEYCODE_9, "9", 0x39) \
  X(LDK_KEYCODE_A, "A", 0x41) \
  X(LDK_KEYCODE_B, "B", 0x42) \
  X(LDK_KEYCODE_C, "C", 0x43) \
  X(LDK_KEYCODE_D, "D", 0x44) \
  X(LDK_KEYCODE_E, "E", 0x45) \
  X(LDK_KEYCODE_F, "F", 0x46) \
  X(LDK_KEYCODE_G, "G", 0x47) \
  X(LDK_KEYCODE_H, "H", 0x48) \
  X(LDK_KEYCODE_I, "I", 0x49) \
  X(LDK_KEYCODE_J, "J", 0x4A) \
  X(LDK_KEYCODE_K, "K", 0x4B) \
  X(LDK_KEYCODE_L, "L", 0x4C) \
  X(LDK_KEYCODE_M, "M", 0x4D) \
  X(LDK_KEYCODE_N, "N", 0x4E) \
  X(LDK_KEYCODE_O, "O", 0x4F) \
  X(LDK_KEYCODE_P, "P", 0x50) \
  X(LDK_KEYCODE_Q, "Q", 0x51) \
  X(LDK_KEYCODE_R, "R", 0x52) \
  X(LDK_KEYCODE_S, "S", 0x53) \
  X(LDK_KEYCODE_T, "T", 0x54) \
  X(LDK_KEYCODE_U, "U", 0x55) \
  X(LDK_KEYCODE_V, "V", 0x56) \
  X(LDK_KEYCODE_W, "W", 0x57) \
  X(LDK_KEYCODE_X, "X", 0x58) \
  X(LDK_KEYCODE_Y, "Y", 0x59) \
  X(LDK_KEYCODE_Z, "Z", 0x5A) \
  X(LDK_KEYCODE_NUMPAD0, "NUMPAD0", 0x60) \
  X(LDK_KEYCODE_NUMPAD1, "NUMPAD1", 0x61) \
  X(LDK_KEYCODE_NUMPAD2, "NUMPAD2", 0x62) \
  X(LDK_KEYCODE_NUMPAD3, "NUMPAD3", 0x63) \
  X(LDK_KEYCODE_NUMPAD4, "NUMPAD4", 0x64) \
  X(LDK_KEYCODE_NUMPAD5, "NUMPAD5", 0x65) \
  X(LDK_KEYCODE_NUMPAD6, "NUMPAD6", 0x66) \
  X(LDK_KEYCODE_NUMPAD7, "NUMPAD7", 0x67) \
  X(LDK_KEYCODE_NUMPAD8, "NUMPAD8", 0x68) \
  X(LDK_KEYCODE_NUMPAD9, "NUMPAD9", 0x69) \
  X(LDK_KEYCODE_MULTIPLY, "MULTIPLY", 0x6A) \
  X(LDK_KEYCODE_ADD, "ADD", 0x6B) \
  X(LDK_KEYCODE_SEPARATOR, "SEPARATOR", 0x6C) \
  X(LDK_KEYCODE_SUBTRACT, "SUBTRACT", 0x6D) \
  X(LDK_KEYCODE_DECIMAL, "DECIMAL", 0x6E) \
  X(LDK_KEYCODE_DIVIDE, "DIVIDE", 0x6F) \
  X(LDK_KEYCODE_F1, "F1", 0x70) \
  X(LDK_KEYCODE_F2, "F2", 0x71) \
  X(LDK_KEYCODE_F3, "F3", 0x72) \
  X(LDK_KEYCODE_F4, "F4", 0x73) \
  X(LDK_KEYCODE_F5, "F5", 0x74) \
  X(LDK_KEYCODE_F6, "F6", 0x75) \
  X(LDK_KEYCODE_F7, "F7", 0x76) \
  X(LDK_KEYCODE_F8, "F8", 0x77) \
  X(LDK_KEYCODE_F9, "F9", 0x78) \
  X(LDK_KEYCODE_F10, "F10", 0x79) \
  X(LDK_KEYCODE_F11, "F11", 0x7A) \
  X(LDK_KEYCODE_F12, "F12", 0x7B) \
  X(LDK_KEYCODE_F13, "F13", 0x7C) \
  X(LDK_KEYCODE_F14, "F14", 0x7D) \
  X(LDK_KEYCODE_F15, "F15", 0x7E) \
  X(LDK_KEYCODE_F16, "F16", 0x7F) \
  X(LDK_KEYCODE_F17, "F17", 0x80) \
  X(LDK_KEYCODE_F18, "F18", 0x81) \
  X(LDK_KEYCODE_F19, "F19", 0x82) \
  X(LDK_KEYCODE_F20, "F20", 0x83) \
  X(LDK_KEYCODE_F21, "F21", 0x84) \
  X(LDK_KEYCODE_F22, "F22", 0x85) \
  X(LDK_KEYCODE_F23, "F23", 0x86) \
  X(LDK_KEYCODE_F24, "F24", 0x87) \
  X(LDK_KEYCODE_OEM1, "OEM1", 0xBA) \
  X(LDK_KEYCODE_OEM2, "OEM2", 0xBF) \
  X(LDK_KEYCODE_OEM3, "OEM3", 0xC0) \
  X(LDK_KEYCODE_OEM4, "OEM4", 0xDB) \
  X(LDK_KEYCODE_OEM5, "OEM5", 0xDC) \
  X(LDK_KEYCODE_OEM6, "OEM6", 0xDD) \
  X(LDK_KEYCODE_OEM7, "OEM7", 0xDE) \
  X(LDK_KEYCODE_OEM8, "OEM8", 0xDF) \
  X(LDK_NUM_KEYCODES, "", 0xE0)

#define X(keycode, name, value) keycode = value,
  typedef enum
  {
    LDK_KEYBOARD_KEYCODES
  } LDKKeycode;

  enum
  {
    LDK_KEYBOARD_PRESSED_BIT = 1,
    LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_KEYBOARD_MAX_KEYS = 256
  };

  // LDKKeyboardState
  typedef struct 
  {
    unsigned char key[LDK_KEYBOARD_MAX_KEYS];
  }LDKKeyboardState;

#undef X

  LDK_API void ldkOsKeyboardStateGet(LDKKeyboardState* outState);

  LDK_API bool ldkOsKeyboardKeyIsPressed(LDKKeyboardState* state, LDKKeycode keycode);   // True while the key is pressed
  LDK_API bool ldkOsKeyboardKeyDown(LDKKeyboardState* state, LDKKeycode keycode);     // True in the frame the key was pressed
  LDK_API bool ldkOsKeyboardKeyUp(LDKKeyboardState* state, LDKKeycode keycode);       // True in the frame the key was released


  //
  // Joystick
  //
#define LDK_JOYSTICK_BUTTONS \
  X(LDK_JOYSTICK_BUTTON_DPAD_UP,     "LDK_JOYSTICK_BUTTON_DPAD_UP",   0x00) \
  X(LDK_JOYSTICK_BUTTON_DPAD_DOWN,   "LDK_JOYSTICK_BUTTON_DPAD_DOWN", 0x01) \
  X(LDK_JOYSTICK_BUTTON_DPAD_LEFT,   "LDK_JOYSTICK_BUTTON_DPAD_LEFT", 0x02) \
  X(LDK_JOYSTICK_BUTTON_DPAD_RIGHT,  "LDK_JOYSTICK_BUTTON_DPAD_RIGHT",0x03) \
  X(LDK_JOYSTICK_BUTTON_START,       "LDK_JOYSTICK_BUTTON_START",     0x04) \
  X(LDK_JOYSTICK_BUTTON_FN1,         "LDK_JOYSTICK_BUTTON_FN1",       0x04) \
  X(LDK_JOYSTICK_BUTTON_BACK,        "LDK_JOYSTICK_BUTTON_BACK",      0x05) \
  X(LDK_JOYSTICK_BUTTON_FN2,         "LDK_JOYSTICK_BUTTON_FN2",       0x05) \
  X(LDK_JOYSTICK_BUTTON_LEFT_THUMB,  "LDK_JOYSTICK_BUTTON_LEFT_THUMB",    0x06) \
  X(LDK_JOYSTICK_BUTTON_RIGHT_THUMB, "LDK_JOYSTICK_BUTTON_RIGHT_THUMB",    0x07) \
  X(LDK_JOYSTICK_BUTTON_LEFT_SHOULDER, "LDK_JOYSTICK_BUTTON_LEFT_SHOULDER", 0x08) \
  X(LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER, "LDK_JOYSTICK_BUTTON_RIGHT_SHOULRDER", 0x09) \
  X(LDK_JOYSTICK_BUTTON_A, "LDK_JOYSTICK_BUTTON_A", 0x0A) \
  X(LDK_JOYSTICK_BUTTON_B, "LDK_JOYSTICK_BUTTON_B", 0x0B) \
  X(LDK_JOYSTICK_BUTTON_X, "LDK_JOYSTICK_BUTTON_X", 0x0C) \
  X(LDK_JOYSTICK_BUTTON_Y, "LDK_JOYSTICK_BUTTON_Y", 0x0D) \
  X(LDK_JOYSTICK_BUTTON_BTN1, "LDK_JOYSTICK_BUTTON1", 0x0A) \
  X(LDK_JOYSTICK_BUTTON_BTN2, "LDK_JOYSTICK_BUTTON2", 0x0B) \
  X(LDK_JOYSTICK_BUTTON_BTN3, "LDK_JOYSTICK_BUTTON3", 0x0C) \
  X(LDK_JOYSTICK_BUTTON_BTN4, "LDK_JOYSTICK_BUTTON4", 0x0D)

#define X(keycode, name, value) keycode = value,
  typedef enum
  {
    LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_JOYSTICK_PRESSED_BIT     = 1,
    LDK_JOYSTICK_NUM_BUTTONS     = 14, // NOTICE that some entry values are repeated
                                       // expand X macro ...
    LDK_JOYSTICK_BUTTONS
  } LDKJoystickButton;
#undef X

#define LDK_JOYSTICK_AXIS \
  X(LDK_JOYSTICK_AXIS_LX, "LDK_JOYSTICK_AXIS_LX", 0x00) \
  X(LDK_JOYSTICK_AXIS_LY, "LDK_JOYSTICK_AXIS_LY", 0x01) \
  X(LDK_JOYSTICK_AXIS_RX, "LDK_JOYSTICK_AXIS_RX", 0x02) \
  X(LDK_JOYSTICK_AXIS_RY, "LDK_JOYSTICK_AXIS_RY", 0x03) \
  X(LDK_JOYSTICK_AXIS_LTRIGGER, "LDK_JOYSTICK_AXIS_LTRIGGER", 0x04) \
  X(LDK_JOYSTICK_AXIS_RTRIGGER, "LDK_JOYSTICK_AXIS_RTRIGGER", 0x05) \

#define LDK_JOYSTICK_MAX 4

  typedef enum
  {
    LDK_JOYSTICK_0,
    LDK_JOYSTICK_1,
    LDK_JOYSTICK_2,
    LDK_JOYSTICK_3,
  } LDKJoystickID;

#define X(keycode, name, value) keycode = value,
  typedef enum
  {
    LDK_JOYSTICK_NUM_AXIS = 6,
    LDK_JOYSTICK_AXIS
  } LDKJoystickAxis;
#undef X

  typedef struct
  {
    uint32 button[LDK_JOYSTICK_NUM_BUTTONS];
    float axis[LDK_JOYSTICK_NUM_AXIS];
    bool connected;
    float vibrationLeft;
    float vibrationRight;
  } LDKJoystickState;


  LDK_API void ldkOsJoystickGetState(LDKJoystickState* outState, LDKJoystickID id);
  LDK_API bool ldkOsJoystickButtonIsPressed(LDKJoystickState* state, LDKJoystickButton key);
  LDK_API bool ldkOsJoystickButtonDown(LDKJoystickState* state, LDKJoystickButton key);
  LDK_API bool ldkOsJoystickButtonUp(LDKJoystickState* state, LDKJoystickButton key);
  LDK_API float ldkOsJoystickAxisGet(LDKJoystickState* state, LDKJoystickAxis key);
  LDK_API uint32 ldkOsJoystickCount();
  LDK_API uint32 ldkOsJoystickIsConnected(LDKJoystickID id);

  LDK_API void ldkOsJoystickVibrationLeftSet(LDKJoystickID id, float speed);
  LDK_API void ldkOsJoystickVibrationRightSet(LDKJoystickID id, float speed);

  LDK_API float ldkOsJoystickVibrationRightGet(LDKJoystickID id);
  LDK_API float ldkOsJoystickVibrationRightGet(LDKJoystickID id);

#ifdef __cplusplus
}
#endif

#endif	//LDK_OS_H
