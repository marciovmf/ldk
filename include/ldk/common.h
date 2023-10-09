/**
 *
 * common
 * 
 * Common macros, functions and types used across the whole engine.
 *
 * Important defines:
 * LDK_EXPORT_API     :should be set when building the engine; not set when building games
 * LDK_AS_SHARED_LIB  :should be set when building LDK as a dll.
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
#include <stdio.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif  

#ifdef LDK_AS_SHARED_LIB
  #ifdef LDK_COMPILER_MSVC
    #ifdef LDK_EXPORT_API
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
typedef uintptr_t LDKHandle;
typedef uint16_t  LDKHandleType;
typedef void*     LDKWindow;


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

// LDKRect
typedef struct 
{
  int32 x;
  int32 y;
  int32 w;
  int32 h;
} LDKRect;

// LDKRectf
typedef struct 
{
  float x;
  float y;
  float w;
  float h;
} LDKRectf;

// LDKPoint
typedef struct
{
  int32 x;
  int32 y;
} LDKPoint;

// LDKPointf
typedef struct
{
  float x;
  float y;
} LDKPointf;

// LDKRGB
typedef struct
{
  int32 r;
  int32 g;
  int32 b;
} LDKRGB;

// LDKRGBA
typedef struct
{
  int32 r;
  int32 g;
  int32 b;
  int32 a;
} LDKRGBA;


//
// Logging
//

LDK_API bool ldkLogInitialize(const char* path, const char* initalMessage);
LDK_API void ldkLogTerminate();
LDK_API void ldkLogPrint(const char* prefix, const char* format, ...);
LDK_API void ldkLogPrintDetailed(const char* prefix, const char* file, int32 line, const char* function, const char* fmt, ...);
LDK_API void ldkLogPrintRaw(const char* fmt, ...);

#define ldkLogInfo(fmt, ...) ldkLogPrint("INFO",  fmt, __VA_ARGS__)
#define ldkLogError(fmt, ...) ldkLogPrintDetailed("ERROR", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#define ldkLogWarning(fmt, ...) ldkLogPrintDetailed("WARNING", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#ifdef LDK_DEBUG
#define ldkLogDebug(fmt, ...) ldkLogPrintDetailed("DEBUG", __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#else
#define ldkLogDebug(fmt, ...)
#endif

//
// Type system
//

typedef uint16 LDKTypeId;

#define typeid(type) ldkTypeId(LDK_STRINGFY(type), sizeof(type))
#define typename(type) ldkTypeName(typeid(type))
#define typesize(type) ldkTypeSize(typeid(type))

LDK_API LDKTypeId   ldkTypeId(const char* name, size_t size);
LDK_API const char* ldkTypeName(LDKTypeId typeId);
LDK_API size_t      ldkTypeSize(LDKTypeId typeId);

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
// Hash
//

typedef uint32 LDKHash;

LDK_API LDKHash ldkHash(const char*);
LDK_API LDKHash ldkHashXX(const void* input, size_t length, uint32_t seed);


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
// Events
//

typedef enum
{
  /* Event Types */
  LDK_EVENT_NONE                  = 0,
  LDK_EVENT_TYPE_GAME             = 1,
  LDK_EVENT_TYPE_WINDOW           = 1 << 1,
  LDK_EVENT_TYPE_TEXT             = 1 << 2,
  LDK_EVENT_TYPE_APPLICATION      = 1 << 3,
  LDK_EVENT_TYPE_KEYBOARD         = 1 << 4,
  LDK_EVENT_TYPE_MOUSE_MOVE       = 1 << 5,
  LDK_EVENT_TYPE_MOUSE_BUTTON     = 1 << 6,
  LDK_EVENT_TYPE_MOUSE_WHEEL      = 1 << 7,
  LDK_EVENT_TYPE_FRAME            = 1 << 8,
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

  /* Frame event types */
  LDK_FRAME_EVENT_BEFORE_RENDER   = 12,
  LDK_FRAME_EVENT_AFTER_RENDER    = 13,

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
  LDKEventType type;
  uint64 ticks;
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
