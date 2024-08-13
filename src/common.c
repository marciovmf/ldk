#include "ldk/common.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>


LDKSize ldkSize(int32 width, int32 height)
{
  LDKSize size = {.width = width, .height = height}; 
  return size;
}

LDKSize ldkSizeZero()
{
  LDKSize size = {0};
  return size;
}

LDKSize ldkSizeOne()
{
  LDKSize size = {.width = 1, .height = 1}; 
  return size;
}

LDKRect ldkRect(int32 x, int32 y, int32 width, int32 height)
{
  LDKRect rect = {.x = x, .y = y, .w = width, .h = height};
  return rect;
}

LDKRectf ldkRectf(float x, float y, float width, float height)
{
  LDKRectf rectf = {.x = x, .y = y, .w = width, .h = height};
  return rectf;
}

LDKPoint ldkPoint(int32 x, int32 y)
{
  LDKPoint point = {.x = x, .y = y};
  return point;
}

LDKPointf ldkPointf(float x, float y)
{
  LDKPointf point = {.x = x, .y = y};
  return point;
}

LDKRGB ldkRGB(uint8 r, uint8 g, uint8 b)
{
  LDKRGB rgb = {.r = r, .g = g, .b = b};
  return rgb;
}

LDKRGBA ldkRGBA(uint8 r, uint8 g, uint8 b, uint8 a)
{
  LDKRGBA rgba = {.r = r, .g = g, .b = b, .a = a};
  return rgba;
}

//
// Logging
//

static FILE* logFile_ = 0;

bool ldkLogInitialize(const char* path, const char* initialMessage)
{
  if (logFile_)
    return false;

  if (!path)
    path = "ldk.log";

  logFile_ = fopen(path, "w+");
  if (!logFile_)
    return false;

  if (initialMessage)
  {
    printf("\n\n%s\n", initialMessage);
    fprintf(logFile_, "\n\n%s\n", initialMessage);
  }
  return true;
}

void ldkLogTerminate(void)
{
  if (logFile_)
    fclose(logFile_);
}

void ldkLogPrintRaw(const char* fmt, ...)
{
  va_list argList;
  va_start(argList, fmt);
  vfprintf(stdout, fmt, argList);
  if (logFile_)
  {
    vfprintf(logFile_, fmt, argList);
  }
  va_end(argList);
}

void ldkLogPrint(const char* prefix, const char* fmt, ...)
{
  va_list argList;
  va_start(argList, fmt);
  fprintf(stdout, (const char*) "\n[%s] ", prefix);
  vfprintf(stdout, fmt, argList);
  if (logFile_)
  {
    fprintf(logFile_, (const char*) "\n[%s] ", prefix);
    vfprintf(logFile_, fmt, argList);
  }
  va_end(argList);
}

void ldkLogPrintDetailed(const char* prefix, const char* file, int32 line, const char* function, const char* fmt, ...)
{
  va_list argList;
  va_start(argList, fmt);
  fprintf(stdout, (const char*) "\n[%s] (%s:%d @ %s) ", prefix, file, line, function);
  vfprintf(stdout, fmt, argList);
  if (logFile_)
  {
    fprintf(logFile_, (const char*) "\n[%s] (%s:%d @ %s) ", prefix, file, line, function);
    vfprintf(logFile_, fmt, argList);
  }
  va_end(argList);
}

//
// Hash
//

LDKHash ldkHashDJB2(const char* str)
{
  unsigned long hash = 5381;
  int c;
  while ((c = *str++))
  {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}


LDKHash ldkHashStr(const char* str)
{
  uint32_t hash = 2166136261u; // FNV offset basis
  while (*str)
  {
    hash ^= (uint32_t) *str++;
    hash *= 16777619u; // FNV prime
  }
  return hash;
}

LDKHash ldkHash(const void* data, size_t size)
{
  const char* p = (const char*) data; 
  uint32_t hash = 2166136261u; // FNV offset basis
  for (uint32 i = 0; i < size; i++)
  {
    hash ^= (uint32_t) p[i];
    hash *= 16777619u; // FNV prime
  }
  return hash;
}

LDKHash ldkHashXX(const void* input, size_t length, uint32_t seed)
{
  const uint32_t prime = 2654435761U;
  uint32_t hash = seed + prime;
  const char* data = (const char*)input;
  size_t remaining = length;

  while (remaining >= 4)
  {
    uint32_t value;
    memcpy(&value, data, sizeof(uint32_t));
    hash += value * prime;
    hash = (hash << 13) | (hash >> 19); // Rotate left by 13 bits
    data += 4;
    remaining -= 4;
  }

  while (remaining > 0)
  {
    hash += (*data++) * prime;
    hash = (hash << 13) | (hash >> 19);
    remaining--;
  }

  return hash;
}

//
// Type System
//

#ifndef LDK_TYPE_NAME_MAX_LENGTH
#define LDK_TYPE_NAME_MAX_LENGTH 64
#endif

#ifndef LDK_TYPE_SYSTEM_MAX_TYPES
#define LDK_TYPE_SYSTEM_MAX_TYPES 64
#endif

LDKHash ldkHashDJB2(const char* str);
typedef struct
{
  char name[LDK_TYPE_NAME_MAX_LENGTH];
  size_t size;
  LDKTypeId id;
  LDKHash hash;
} LDKTypeInfo;

static struct
{
  LDKTypeInfo type[LDK_TYPE_SYSTEM_MAX_TYPES];
  uint32 count;
} ldkTypeCatalog = {0};

LDKTypeId ldkTypeId(const char* name, size_t size)
{
  if (ldkTypeCatalog.count >= LDK_TYPE_SYSTEM_MAX_TYPES)
  {
    ldkLogError("Could not register type '%s' as the type catalog is full (%d entries)", LDK_TYPE_SYSTEM_MAX_TYPES);
    return LDK_TYPE_ID_UNKNOWN;
  }

#ifdef LDK_DEBUG
  size_t typeNameLen = strlen(name);
  if (typeNameLen >= (LDK_TYPE_NAME_MAX_LENGTH - 1))
  {
    ldkLogError("Could not register type '%s' as the type is too long. Maximum supportd is %d)", (LDK_TYPE_NAME_MAX_LENGTH - 1));
    return LDK_TYPE_ID_UNKNOWN;
  }
#endif

  uint32 hashIndex = ldkHashDJB2(name) % LDK_TYPE_SYSTEM_MAX_TYPES;

  for (uint32 i = 0; i < LDK_TYPE_SYSTEM_MAX_TYPES; i++)
  {
    uint32 index = (hashIndex + i) % LDK_TYPE_SYSTEM_MAX_TYPES;
    LDKTypeInfo* typeInfo = &ldkTypeCatalog.type[index];

    //Register if empty
    if (typeInfo->name[0] == '\0')
    {
      strncpy(typeInfo->name, name, LDK_TYPE_NAME_MAX_LENGTH);
      typeInfo->size = size;
      typeInfo->id = index + 1;
      ldkTypeCatalog.count++;
      return typeInfo->id;
    }
    else if (strncmp(typeInfo->name, name, LDK_TYPE_NAME_MAX_LENGTH) == 0)
    {
      // Return type id if exists
      return typeInfo->id;
    }
  }

  return LDK_TYPE_ID_UNKNOWN;
}

const char* ldkTypeName(LDKTypeId typeId)
{
  LDK_ASSERT(typeId != LDK_TYPE_ID_UNKNOWN);
  LDK_ASSERT(typeId < LDK_TYPE_SYSTEM_MAX_TYPES);
  LDK_ASSERT(ldkTypeCatalog.type[typeId -1].id == typeId);
  return ldkTypeCatalog.type[typeId -1].name;
}

size_t ldkTypeSize(LDKTypeId typeId)
{
  LDK_ASSERT(typeId != LDK_TYPE_ID_UNKNOWN);
  LDK_ASSERT(typeId < LDK_TYPE_SYSTEM_MAX_TYPES);
  LDK_ASSERT(ldkTypeCatalog.type[typeId -1].id == typeId);
  return ldkTypeCatalog.type[typeId -1].size;
}

//
// Handles
//

LDKHandleType ldkHandleType(LDKHandle handle)
{
  return (uint16)(handle & 0xFFFF);
}

uint32 ldkHandleIndex(LDKHandle handle)
{
  return (uint32_t)((handle >> 16) & 0xFFFFFFFF);
}

uint32 ldkHandleVersion(LDKHandle handle)
{
  return (uint32) ((handle >> 48) & 0xFFFF);
}

LDKHandle ldkMakeHandle(LDKTypeId type, int index, int version)
{
  return ((uint64_t) version << 48) | ((uint64_t) index << 16) | type;
}

//
// String
//

bool ldkStringEndsWith(const char* str, const char* suffix)
{
  const char* found = strstr(str, suffix);
  if (found == NULL || found + strlen(suffix) != str + strlen(str))
  {
    return false;
  }
  return true;
}

bool ldkStringStartsWith(const char* str, const char* prefix)
{
  const char* found = strstr(str, prefix);
  return found == str;
}

size_t ldkSmallString(LDKSmallStr* smallString, const char* str)
{
  size_t length = strlen(str);
  if (length >= LDK_SMALL_STRING_MAX_LENGTH)
  {
    ldkLogError("The string length (%d) exceeds the maximum size of a LDKSmallString (%d)", length, LDK_SMALL_STRING_MAX_LENGTH);
    return length;
  }

  strncpy((char *) &smallString->str, str, LDK_SMALL_STRING_MAX_LENGTH - 1);
  return 0;
}

size_t ldkSmallStringLength(LDKSmallStr* smallString)
{
  return smallString->length;
}

void ldkSmallStringClear(LDKSmallStr* smallString)
{
  memset(smallString->str, 0, LDK_SMALL_STRING_MAX_LENGTH * sizeof(char));
}

size_t ldkSmallStringFormat(LDKSmallStr* smallString, const char* fmt, ...)
{
  va_list argList;
  va_start(argList, fmt);
  smallString->length = vsnprintf((char*) &smallString->str, LDK_SMALL_STRING_MAX_LENGTH-1, fmt, argList);
  va_end(argList);

  bool success = smallString->length > 0 && smallString->length < LDK_SMALL_STRING_MAX_LENGTH;
  return success;
}

size_t ldkSubstringToSmallstring(LDKSubStr* substring, LDKSmallStr* outSmallString)
{
  if (substring->length >= (LDK_SMALL_STRING_MAX_LENGTH - 1))
    return substring->length;

  strncpy((char*) &outSmallString->str, substring->ptr, substring->length);
  outSmallString->str[substring->length] = 0;
  return 0;
}


//
// Path
//

LDKSubStr ldkPathFileNameGetSubstring(const char* path)
{
  LDKSubStr result;
  result.ptr = NULL;
  result.length = 0;

  if (path)
  {
    const char* file = strrchr(path, LDK_PATH_SEPARATOR);
    if (file)
    {
      result.ptr = (char*)(file + 1);
      result.length = strlen(result.ptr);
    }
    else
    {
      result.ptr = (char*)path;
      result.length = strlen(path);
    }
  }

  return result;
}

LDKSubStr ldkPathFileExtentionGetSubstring(const char* path)
{
  LDKSubStr result;
  result.ptr = NULL;
  result.length = 0;

  if (path)
  {
    const char* dot = strrchr(path, '.');
    if (dot)
    {
      result.ptr = (char*)(dot + 1);
      result.length = strlen(result.ptr);
    }
  }

  return result;
}

size_t ldkPathFileNameGet(const char* path, char* outBuffer, size_t bufferSize)
{
  LDKSubStr substr = ldkPathFileNameGetSubstring(path);
  if(substr.length <= 0 || substr.length >= bufferSize)
  {
    return substr.length;
  }

  strncpy(outBuffer, substr.ptr, substr.length);
  outBuffer[substr.length] = 0;
  return substr.length;
}

size_t ldkPathFileExtentionGet(const char* path, char* outBuffer, size_t bufferSize)
{
  LDKSubStr substr = ldkPathFileExtentionGetSubstring(path);
  if(substr.length <= 0 || substr.length >= bufferSize)
  {
    return substr.length;
  }

  strncpy(outBuffer, substr.ptr, substr.length);
  outBuffer[substr.length] = 0;
  return substr.length;
}

bool ldkPathIsAbsolute(const char* path)
{
#ifdef LDK_OS_WINDOWS
  const char* p = path;
  size_t length = strlen(path);
  if (length >= 3)
    return (isalpha(p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) || (p[0] == '\\' && p[1] == '\\');
  else
    return (isalpha(p[0]) && p[1] == ':') || (p[0] == '\\' && p[1] == '\\');
#else
  return path[0] == '/';
#endif
}

bool ldkPathIsRelative(const char* path)
{
  return ! ldkPathIsAbsolute(path);
}

bool ldkPath(LDKPath* outPath, const char* path)
{
  size_t length = strlen(path);
  if (length >= (LDK_PATH_MAX_LENGTH - 1))
  {
    ldkLogError("Path too long (%d). Maximum is %d. - '%s'", length, LDK_PATH_MAX_LENGTH, path);
    return false;
  }

  strncpy((char*) &outPath->path, path, length);
  outPath->path[length] = 0;
  outPath->length = length;
  return true;
}

void ldkPathClone(LDKPath* outPath, const LDKPath* path)
{
  memcpy(outPath, path, sizeof(LDKPath));
}

void ldkPathNormalize(LDKPath* ldkPath)
{
  char *path = ldkPath->path;
  size_t len = ldkPath->length;
  size_t resultIndex = 0;
  size_t i = 0;


  if (path[1] == ':') { i += 2; }

  // Skip leading slashes
  while (i < len && (path[i] == '/' || path[i] == '\\')) {
    i++;
  }

  for (; i < len; i++) {
    if (path[i] == '/' || path[i] == '\\') {
      // Skip multiple consecutive slashes
      while (i < len && (path[i] == '/' || path[i] == '\\')) {
        i++;
      }
      // Add a single slash to the result
      ldkPath->path[resultIndex++] = '/';
    } else if (path[i] == '.' && (i + 1 == len || path[i + 1] == '/' || path[i + 1] == '\\')) {
      // Skip "." or "./"
      i++;
    } else if (path[i] == '.' && path[i + 1] == '.' && (i + 2 == len || path[i + 2] == '/' || path[i + 2] == '\\')) {
      // Handle up-level reference ".."
      if (resultIndex > 0) {
        // Remove the last component from the result
        while (resultIndex > 0 && ldkPath->path[resultIndex - 1] != '/') {
          resultIndex--;
        }
        if (resultIndex > 0) {
          resultIndex--;
        }
      }
      i += 2;
    } else {
      // Copy characters to the result
      ldkPath->path[resultIndex++] = path[i];
    }
  }

  // Update the length and null-terminate the result
  ldkPath->length = resultIndex;
  ldkPath->path[resultIndex] = '\0';
}


//
// Light attenuation
//

// Attenuation values from https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
static uint32 rangeLimit[] = {7, 13, 20, 32, 50, 65, 100, 160, 200, 325, 600, 3250};
static float linear[]     = {0.7f,  0.35f,  0.22f,  0.14f,  0.09f,  0.07f,  0.045f,  0.027f,  0.022f,  0.014f,  0.007f,  0.0014f};
static float quadratic[]  = {1.8f,  0.44f,  0.20f,  0.07f,  0.032f,  0.017f,  0.0075f,  0.0028f,  0.0019f,  0.0007f,  0.0002f,  0.000007f};

void ldkLightAttenuationForDistance(LDKLightAttenuation* attenuation, float distance)
{
  if (attenuation == NULL)
    return;

  uint32 r = LDK_MAX(rangeLimit[0], (uint32) distance);
  uint32 presetIndex = 0;
  uint32 numPresets = sizeof(rangeLimit) / sizeof(uint32);
  for (uint32 i = 0; i < numPresets; i++)
  {
    if (r < rangeLimit[i])
      break;
    presetIndex = i;
  }

  attenuation->linear     = linear[presetIndex],
  attenuation->quadratic  = quadratic[presetIndex];
}
