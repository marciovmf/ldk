/**
 * common.h
 * 
 * Common macros, functions and types used across the whole engine.
 *
 * Important defines:
 * LDK_ENGINE       :should be set when building the engine
 * LDK_SHAREDLIB    :should be set when building LDK as a dll.
 */

#ifndef LDK_COMMON_H
#define LDK_COMMON_H


#ifdef __cplusplus
extern "C" {
#endif

  // OS Detection macros

#if defined(_WIN32) || defined(_WIN64)
#define LDK_OS_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#elif defined(__linux__)
#define LDK_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define LDK_OS_OSX
#else
#define LDK_OS_UNKNOWN
#endif

  // Compiler detection macros

#if defined(__GNUC__) || defined(__GNUG__)
#define LDK_COMPILER_NAME "GCC"
#define LDK_COMPILER_VERSION __VERSION__
#define LDK_COMPILER_GCC
#elif defined(_MSC_VER)
#define LDK_COMPILER_NAME "Microsoft Visual C/C++ Compiler"
#define LDK_COMPILER_VERSION _MSC_FULL_VER
#define LDK_COMPILER_MSVC
#elif defined(__clang__)
#define LDK_COMPILER_NAME "Clang"
#define LDK_COMPILER_VERSION __clang_version__
#define LDK_COMPILER_CLANG
#else
#define LDK_COMPILER_NAME "Unknown"
#define LDK_COMPILER_VERSION "Unknown"
#define LDK_COMPILER_UNKNOWN
#endif

#if defined(LDK_OS_WINDOWS)
#define CRT_SECURE_NO_WARNINGS_
#endif

#ifndef LDK_BUILD_TYPE
#define LDK_BUILD_TYPE ""
#endif  // LDK_BUILD_TYPE

#define LDK_VERSION_MAJOR 0
#define LDK_VERSION_PATCH 0
#define LDK_VERSION_MINOR 1

#include <stdio.h>

  // Assertion macros

#define LDK_STATEMENT(S) do { S; }while(0)
#define LDK_ASSERT_BREAK() (*(volatile int*)0 = 0)
#define LDK_ASSERT(CONDITION) LDK_STATEMENT(\
    if (! (CONDITION)) {\
    printf("ASSERTION FAILED AT %s:%d\n", __FILE__, __LINE__);\
    LDK_ASSERT_BREAK();\
    })


#define LDK_NOT_IMPLEMENTED() LDK_STATEMENT( \
    ldkLogError("Not implemented"); \
    LDK_ASSERT(false))


  // Helper macros

#define LDK_STRINGFY_(S) #S
#define LDK_STRINGFY(S) LDK_STRINGFY_(S)

#define LDK_GLUE_(A,B) A##B
#define LDK_GLUE(A,B) LDK_GLUE_(A, B)

#define LDK_MIN(A,B) (((A)<(B))?(A):(B))
#define LDK_MAX(A,B) (((A)>(B))?(A):(B))
#define LDK_CLAMP(A, X, B) ((X)<(A)?(A):(X)>(B)?(B):(X))

#define LDK_KILOBYTE(value) (size_t) ((value) * 1024LL)
#define LDK_MEGABYTE(value) (size_t) (KILOBYTE(value) * 1024LL)
#define LDK_GIGABYTE(value) (size_t) (MEGABYTE(value) * 1024LL)

// Debug macros

#if defined(DEBUG) || defined(_DEBUG)
#ifndef LDK_DEBUG
#define LDK_DEBUG
#endif
#endif

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif  

#ifndef LDK_ENGINE
#define LDK_GAME
#endif

// @build-tweak
#ifdef LDK_SHAREDLIB
  #ifdef LDK_COMPILER_MSVC
    #ifdef LDK_ENGINE
      #define LDK_API __declspec(dllexport) 
    #else
      #define LDK_API __declspec(dllimport) 
    #endif
  #endif
#else
  #define LDK_API
#endif


  //
  // Global constants
  //
  enum
  {
    LDK_SMALL_STRING_MAX_LENGTH = 512,
    LDK_PATH_MAX_LENGTH         = 512,

    LDK_TYPE_ID_UNKNOWN         = 0xFFFF,
    LDK_HANDLE_INVALID          = 0
  };

  //
  // Common types
  //

  typedef int8_t    int8;
  typedef uint8_t   uint8;
  typedef uint8_t   byte;
  typedef int16_t   int16;
  typedef uint16_t  uint16;
  typedef int32_t   int32;
  typedef uint32_t  uint32;
  typedef int64_t   int64;
  typedef uint64_t  uint64;
  typedef uint64_t  LDKHandle;
  typedef void*     LDKWindow;

  //
  // Handles
  //

  typedef struct
  {
    LDKHandle value;
  } LDKHAsset;

  typedef struct
  {
    LDKHandle value;
  } LDKHEntity;

  #define LDK_HASSET_INVALID ((LDKHAsset) {.value = LDK_HANDLE_INVALID })
  #define LDK_HENTITY_INVALID ((LDKHEntity) {.value = LDK_HANDLE_INVALID })
  #define ldkHandleIsValid(h) ((h).value != LDK_HANDLE_INVALID)

  // Cast an LDKHandle to a specialized handle type
  #define ldkHandleTo(t, h) (t){.value = h}
  // Cast a specialized handle type to LDKHandle
  #define ldkHandleFrom(h) (LDKHandle)(h.value)

  #define ldkHandleEquals(a, b) ((a).value == (b).value)

  // SmallStr
  typedef struct
  {
    char str[LDK_SMALL_STRING_MAX_LENGTH];
    size_t length;
  } LDKSmallStr;

  // LDKSubstring
  typedef struct
  {
    char* ptr;
    size_t length;
  } LDKSubStr;

  // LDKSize
  typedef struct
  {
    int32 width;
    int32 height;
  } LDKSize;

  LDK_API LDKSize ldkSize(int32 width, int32 height);
  LDK_API LDKSize ldkSizeZero();
  LDK_API LDKSize ldkSizeOne();

  // LDKRect
  typedef struct 
  {
    int32 x;
    int32 y;
    int32 w;
    int32 h;
  } LDKRect;

  LDK_API LDKRect ldkRect(int32 x, int32 y, int32 width, int32 height);

  // LDKRectf
  typedef struct 
  {
    float x;
    float y;
    float w;
    float h;
  } LDKRectf;


  LDK_API LDKRectf ldkRectf(float x, float y, float width, float height);

  // LDKPoint
  typedef struct
  {
    int32 x;
    int32 y;
  } LDKPoint;

  LDK_API LDKPoint ldkPoint(int32 x, int32 y);

  // LDKPointf
  typedef struct
  {
    float x;
    float y;
  } LDKPointf;

  LDK_API LDKPointf ldkPointf(float x, float y);

  // LDKRGB
  typedef struct
  {
    uint8 r;
    uint8 g;
    uint8 b;
  } LDKRGB;

  LDK_API LDKRGB ldkRGB(uint8 r, uint8 g, uint8 b);

  // LDKRGBA
  typedef struct
  {
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 a;
  } LDKRGBA;

  LDK_API LDKRGBA ldkRRGA(uint8 r, uint8 g, uint8 b, uint8 a);

#define LDK_RGBA_WHITE       ldkRGBA(255, 255, 255, 255)
#define LDK_RGBA_BLACK       ldkRGBA(0, 0, 0, 255)
#define LDK_RGBA_RED         ldkRGBA(255, 0, 0, 255)
#define LDK_RGBA_GREEN       ldkRGBA(0, 255, 0, 255)
#define LDK_RGBA_BLUE        ldkRGBA(0, 0, 255, 255)
#define LDK_RGBA_YELLOW      ldkRGBA(255, 255, 0, 255)
#define LDK_RGBA_CYAN        ldkRGBA(0, 255, 255, 255)
#define LDK_RGBA_MAGENTA     ldkRGBA(255, 0, 255, 255)
#define LDK_RGBA_ORANGE      ldkRGBA(255, 165, 0, 255)
#define LDK_RGBA_PURPLE      ldkRGBA(128, 0, 128, 255)
#define LDK_RGBA_GRAY        ldkRGBA(128, 128, 128, 255)
#define LDK_RGBA_DARKGRAY    ldkRGBA(64, 64, 64, 255)
#define LDK_RGBA_LIGHTGRAY   ldkRGBA(192, 192, 192, 255)
#define LDK_RGBA_PINK        ldkRGBA(255, 192, 203, 255)
#define LDK_RGBA_BROWN       ldkRGBA(165, 42, 42, 255)
#define LDK_RGBA_GOLD        ldkRGBA(255, 215, 0, 255)
#define LDK_RGBA_SILVER      ldkRGBA(192, 192, 192, 255)
#define LDK_RGBA_LIME        ldkRGBA(0, 255, 0, 255)
#define LDK_RGBA_TURQUOISE   ldkRGBA(64, 224, 208, 255)
#define LDK_RGBA_TEAL        ldkRGBA(0, 128, 128, 255)
#define LDK_RGBA_INDIGO      ldkRGBA(75, 0, 130, 255)
#define LDK_RGBA_VIOLET      ldkRGBA(238, 130, 238, 255)
#define LDK_RGBA_CORAL       ldkRGBA(255, 127, 80, 255)
#define LDK_RGBA_CHOCOLATE   ldkRGBA(210, 105, 30, 255)
#define LDK_RGBA_IVORY       ldkRGBA(255, 255, 240, 255)
#define LDK_RGBA_BEIGE       ldkRGBA(245, 222, 179, 255)
#define LDK_RGBA_MINT        ldkRGBA(189, 252, 201, 255)
#define LDK_RGBA_LAVENDER    ldkRGBA(230, 230, 250, 255)
#define LDK_RGBA_PEACH       ldkRGBA(255, 229, 180, 255)
#define LDK_RGBA_SEASHELL    ldkRGBA(255, 228, 196, 255)
#define LDK_RGBA_SALMON      ldkRGBA(250, 128, 114, 255)
#define LDK_RGBA_PLUM        ldkRGBA(221, 160, 221, 255)
#define LDK_RGBA_MAROON      ldkRGBA(128, 0, 0, 255)
#define LDK_RGBA_OLIVE       ldkRGBA(128, 128, 0, 255)
#define LDK_RGBA_NAVY        ldkRGBA(0, 0, 128, 255)
#define LDK_RGBA_FUCHSIA     ldkRGBA(255, 0, 255, 255)
#define LDK_RGBA_KHAKI       ldkRGBA(240, 230, 140, 255)
#define LDK_RGBA_SLATEGRAY   ldkRGBA(112, 128, 144, 255)
#define LDK_RGBA_AQUA        ldkRGBA(0, 255, 255, 255)
#define LDK_RGBA_LIGHTBLUE   ldkRGBA(173, 216, 230, 255)
#define LDK_RGBA_DODGERBLUE  ldkRGBA(30, 144, 255, 255)

#define LDK_RGB_WHITE       ldkRGB(255, 255, 255)
#define LDK_RGB_BLACK       ldkRGB(0, 0, 0)
#define LDK_RGB_RED         ldkRGB(255, 0, 0)
#define LDK_RGB_GREEN       ldkRGB(0, 255, 0)
#define LDK_RGB_BLUE        ldkRGB(0, 0, 255)
#define LDK_RGB_YELLOW      ldkRGB(255, 255, 0)
#define LDK_RGB_CYAN        ldkRGB(0, 255, 255)
#define LDK_RGB_MAGENTA     ldkRGB(255, 0, 255)
#define LDK_RGB_ORANGE      ldkRGB(255, 165, 0)
#define LDK_RGB_PURPLE      ldkRGB(128, 0, 128)
#define LDK_RGB_GRAY        ldkRGB(128, 128, 128)
#define LDK_RGB_DARKGRAY    ldkRGB(64, 64, 64)
#define LDK_RGB_LIGHTGRAY   ldkRGB(192, 192, 192)
#define LDK_RGB_PINK        ldkRGB(255, 192, 203)
#define LDK_RGB_BROWN       ldkRGB(165, 42, 42)
#define LDK_RGB_GOLD        ldkRGB(255, 215, 0)
#define LDK_RGB_SILVER      ldkRGB(192, 192, 192)
#define LDK_RGB_LIME        ldkRGB(0, 255, 0)
#define LDK_RGB_TURQUOISE   ldkRGB(64, 224, 208)
#define LDK_RGB_TEAL        ldkRGB(0, 128, 128)
#define LDK_RGB_INDIGO      ldkRGB(75, 0, 130)
#define LDK_RGB_VIOLET      ldkRGB(238, 130, 238)
#define LDK_RGB_CORAL       ldkRGB(255, 127, 80)
#define LDK_RGB_CHOCOLATE   ldkRGB(210, 105, 30)
#define LDK_RGB_IVORY       ldkRGB(255, 255, 240)
#define LDK_RGB_BEIGE       ldkRGB(245, 222, 179)
#define LDK_RGB_MINT        ldkRGB(189, 252, 201)
#define LDK_RGB_LAVENDER    ldkRGB(230, 230, 250)
#define LDK_RGB_PEACH       ldkRGB(255, 229, 180)
#define LDK_RGB_SEASHELL    ldkRGB(255, 228, 196)
#define LDK_RGB_SALMON      ldkRGB(250, 128, 114)
#define LDK_RGB_PLUM        ldkRGB(221, 160, 221)
#define LDK_RGB_MAROON      ldkRGB(128, 0, 0)
#define LDK_RGB_OLIVE       ldkRGB(128, 128, 0)
#define LDK_RGB_NAVY        ldkRGB(0, 0, 128)
#define LDK_RGB_FUCHSIA     ldkRGB(255, 0, 255)
#define LDK_RGB_KHAKI       ldkRGB(240, 230, 140)
#define LDK_RGB_SLATEGRAY   ldkRGB(112, 128, 144)
#define LDK_RGB_AQUA        ldkRGB(0, 255, 255)
#define LDK_RGB_LIGHTBLUE   ldkRGB(173, 216, 230)
#define LDK_RGB_DODGERBLUE  ldkRGB(30, 144, 255)


  //
  // Logging
  //

  LDK_API bool ldkLogInitialize(const char* path, const char* initalMessage);
  LDK_API void ldkLogTerminate(void);
  LDK_API void ldkLogPrint(const char* prefix, const char* format, ...);
  LDK_API void ldkLogPrintDetailed(const char* prefix, const char* file, int32 line, const char* function, const char* fmt, ...);
  LDK_API void ldkLogPrintRaw(const char* fmt, ...);


#ifdef LDK_COMPILER_MSVC
#define ldkLogInfo(fmt, ...) ldkLogPrint("INFO",  fmt, __VA_ARGS__)
#define ldkLogError(fmt, ...) ldkLogPrintDetailed("ERROR", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#define ldkLogWarning(fmt, ...) ldkLogPrintDetailed("WARNING", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#ifdef LDK_DEBUG
#define ldkLogDebug(fmt, ...) ldkLogPrintDetailed("DEBUG", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#else
#define ldkLogDebug(fmt, ...)
#endif
#else
#define ldkLogInfo(fmt, ...) ldkLogPrint("INFO",  fmt, ##__VA_ARGS__)
#define ldkLogError(fmt, ...) ldkLogPrintDetailed("ERROR", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ldkLogWarning(fmt, ...) ldkLogPrintDetailed("WARNING", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#ifdef LDK_DEBUG
#define ldkLogDebug(fmt, ...) ldkLogPrintDetailed("DEBUG", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define ldkLogDebug(fmt, ...)
#endif

#endif

  //
  // Hash
  //

  typedef uint32 LDKHash;

  LDK_API LDKHash ldkHashDJB2(const char* str);
  LDK_API LDKHash ldkHashStr(const char*);
  LDK_API LDKHash ldkHashXX(const void* input, size_t length, uint32_t seed);
  LDK_API LDKHash ldkHash(const void* data, size_t length);



  //
  // Type system
  //

  typedef uint16 LDKTypeId;

#define typeid(type) ldkTypeId(LDK_STRINGFY(type), sizeof(type))
#define typename(id) ldkTypeName(id)
#define typesize(id) ldkTypeSize(id)

  LDK_API LDKTypeId   ldkTypeId(const char* name, size_t size);
  LDK_API const char* ldkTypeName(LDKTypeId typeId);
  LDK_API size_t      ldkTypeSize(LDKTypeId typeId);

  //
  // Handles
  //

  typedef LDKTypeId LDKHandleType;
  LDK_API LDKTypeId ldkHandleType(LDKHandle handle);
  LDK_API uint32 ldkHandleVersion(LDKHandle handle);
  LDK_API uint32 ldkHandleIndex(LDKHandle handle);
  LDK_API LDKHandle ldkMakeHandle(LDKTypeId type, int index, int version);

  //
  // String
  //

  LDK_API bool    ldkStringEndsWith(const char* str, const char* suffix);
  LDK_API bool    ldkStringStartsWith(const char* str, const char* prefix);
  LDK_API size_t  ldkSmallString(LDKSmallStr* smallString, const char* str);
  LDK_API size_t  ldkSmallStringLength(LDKSmallStr* smallString);
  LDK_API void    ldkSmallStringClear(LDKSmallStr* smallString);
  LDK_API size_t  ldkSmallStringFormat(LDKSmallStr* smallString, const char* fmt, ...);
  LDK_API size_t  ldkSubstringToSmallstring(LDKSubStr* substring, LDKSmallStr* outSmallString);

  //
  // Path
  //

  // LDKPath
  typedef struct
  {
    char path[LDK_PATH_MAX_LENGTH];
    size_t length;
  } LDKPath;

  LDK_API LDKSubStr ldkPathFileNameGetSubstring(const char* path);
  LDK_API LDKSubStr ldkPathFileExtentionGetSubstring(const char* path);

  LDK_API size_t    ldkPathFileNameGet(const char* path, char* outBuffer, size_t bufferSize);
  LDK_API size_t    ldkPathFileExtentionGet(const char* path, char* outBuffer, size_t bufferSize);

  LDK_API bool      ldkPathIsAbsolute(const char* path);
  LDK_API bool      ldkPathIsRelative(const char* path);

  LDK_API bool      ldkPath(LDKPath* outPath, const char* path);
  LDK_API void      ldkPathClone(LDKPath* outPath, const LDKPath* path);
  LDK_API void      ldkPathNormalize(LDKPath* path);


#ifdef LDK_OS_WINDOWS
#define LDK_PATH_SEPARATOR '\\'
#else
#define LDK_PATH_SEPARATOR '/'
#endif


  //
  // Light attenuation
  //

  typedef struct 
  {
    float linear;
    float quadratic;
  } LDKLightAttenuation;

  LDK_API void ldkLightAttenuationForDistance(LDKLightAttenuation* attenuation, float distance);

  //
  // Events
  //

  typedef enum
  {
    /* Event Types */
    LDK_EVENT_TYPE_NONE             = 0,
    LDK_EVENT_TYPE_GAME             = 1,
    LDK_EVENT_TYPE_WINDOW           = 1 << 1,
    LDK_EVENT_TYPE_TEXT             = 1 << 2,
    LDK_EVENT_TYPE_APPLICATION      = 1 << 3,
    LDK_EVENT_TYPE_KEYBOARD         = 1 << 4,
    LDK_EVENT_TYPE_MOUSE_MOVE       = 1 << 5,
    LDK_EVENT_TYPE_MOUSE_BUTTON     = 1 << 6,
    LDK_EVENT_TYPE_MOUSE_WHEEL      = 1 << 7,
    LDK_EVENT_TYPE_FRAME_BEFORE     = 1 << 8,
    LDK_EVENT_TYPE_FRAME_AFTER      = 1 << 9,
    LDK_EVENT_TYPE_ANY              = 0xFFFFFFFF,

    /* Keyboard Event types */
    LDK_KEYBOARD_EVENT_KEY_DOWN     = 1,
    LDK_KEYBOARD_EVENT_KEY_UP       = 2,
    LDK_KEYBOARD_EVENT_KEY_HOLD     = 3,

    /* Text Event types */
    LDK_TEXT_EVENT_CHARACTER_INPUT  = 4,
    LDK_TEXT_EVENT_BACKSPACE        = 5,
    LDK_TEXT_EVENT_DEL              = 6,

    /* Mouse event types */
    LDK_MOUSE_EVENT_MOVE            = 7,
    LDK_MOUSE_EVENT_BUTTON_DOWN     = 8,
    LDK_MOUSE_EVENT_BUTTON_UP       = 9,
    LDK_MOUSE_EVENT_WHEEL_FORWARD   = 10,
    LDK_MOUSE_EVENT_WHEEL_BACKWARD  = 11,

    /* Window event types */
    LDK_WINDOW_EVENT_RESIZED        = 14,
    LDK_WINDOW_EVENT_CLOSE          = 15,
    LDK_WINDOW_EVENT_ACTIVATE       = 16,
    LDK_WINDOW_EVENT_DEACTIVATE     = 17,
    LDK_WINDOW_EVENT_MINIMIZED      = 18,
    LDK_WINDOW_EVENT_MAXIMIZED      = 19,

  } LDKEventType;

  // LDKTextEvent
  typedef struct
  {
    LDKEventType type;
    uint32 character;
  } LDKTextEvent;

  // LDKWindowEvent
  typedef struct
  {
    LDKEventType type;
    uint32 width;
    uint32 height;
  } LDKWindowEvent;

  // LDKKeyboardEvent
  typedef struct
  {
    LDKEventType type;
    uint32 keyCode; 
    bool ctrlIsDown;
    bool shiftIsDown;
    bool altIsDown;
  } LDKKeyboardEvent;

  // LDKMouseEvent
  typedef struct
  {
    LDKEventType type;
    int32 cursorX;
    int32 cursorY;
    int32 xRel;
    int32 yRel;

    union
    {
      int32 wheelDelta;
      int32 mouseButton;
    };

  } LDKMouseEvent;

  // LDKFrameEvent
  typedef struct
  {
    uint64 ticks;
    float deltaTime;
  } LDKFrameEvent;

  // LDKEvent
  typedef struct
  {
    LDKWindow window;
    LDKEventType type;
    union
    {
      LDKTextEvent        textEvent;
      LDKWindowEvent      windowEvent;
      LDKKeyboardEvent    keyboardEvent;
      LDKMouseEvent       mouseEvent;
      LDKFrameEvent       frameEvent;
    };
  } LDKEvent;

#ifdef __cplusplus
}
#endif


#endif //LDK_COMMON_H
