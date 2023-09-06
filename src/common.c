
#include "../include/ldk/common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

//
// Logging
//

static FILE* logFile_ = 0;

LDK_API bool ldkLogInitialize(const char* path)
{
  if (logFile_)
    return false;

  if (!path)
    path = "ldk.log";

  logFile_ = fopen(path, "a+");
  if (!logFile_)
    return false;

  return true;
}

LDK_API void ldkLogTerminate()
{
  if (logFile_)
    fclose(logFile_);
}

LDK_API void ldkLogPrint(const char* prefix, const char* file, int32 line, const char* function, const char* fmt, ...)
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
// Sting
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
  if (length >= LDK_SMALL_STRING_MAX_LEN)
  {
    ldkLogError("The string length (%d) exceeds the maximum size of a LDKSmallString (%d)", length, LDK_SMALL_STRING_MAX_LEN);
    return length;
  }

  strncpy((char *) &smallString->str, str, LDK_SMALL_STRING_MAX_LEN - 1);
  return 0;
}

size_t ldkSmallStringLength(LDKSmallStr* smallString)
{
  return smallString->length;
}

void ldkSmallStringClear(LDKSmallStr* smallString)
{
  memset(smallString->str, 0, LDK_SMALL_STRING_MAX_LEN * sizeof(char));
}

size_t ldkSmallStringFormat(LDKSmallStr* smallString, const char* fmt, ...)
{
  va_list argList;
  va_start(argList, fmt);
  smallString->length = vsnprintf((char*) &smallString->str, LDK_SMALL_STRING_MAX_LEN, fmt, argList);
  va_end(argList);

  bool success = smallString->length > 0 && smallString->length < LDK_SMALL_STRING_MAX_LEN;
  return success;
}

//
// Path
//

size_t ldkPathCreate(LDKPath* outPath, const char* path)
{
  size_t totalLen = strlen(path);

  // Buffer too small? return the necessary size
  if (totalLen >= LDK_PATH_MAX_LENGTH)
    return totalLen;

  strncpy(outPath->path, path, totalLen);
  outPath->path[totalLen] = 0;
  outPath->len = totalLen;
  return 0;
}

void ldkPathCopy(const LDKPath* path, LDKPath* outPath)
{
  memcpy((void*) outPath->path, path->path, sizeof(LDKPath));
}

size_t ldkPathLength(const LDKPath* path)
{
  return path->len;
}

LDKSubStr ldkPathFileNameGet(const char* path)
{
  LDKSubStr result;
  result.ptr = NULL;
  result.length = 0;

  if (path)
  {
    const char* file = strrchr(path, '/');
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

LDKSubStr ldkPathFileExtentionGet(const char* path)
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

size_t ldkPathAppend(LDKPath* path, const char* newPart)
{
  if (!path || !newPart)
  {
    return 0;
  }

  size_t currentLength = strlen(path->path);
  size_t newPartLength = strlen(newPart);

  // Buffer too small? return the necessary size
  if (currentLength + newPartLength >= LDK_PATH_MAX_LENGTH)
    return currentLength + newPartLength + 1; // +1 for the null terminator

  strcat(path->path, newPart);
  return 0; // Success
}

bool ldkPathIsAbsolute(LDKPath* path)
{
#ifdef LDK_OS_WINDOWS
  char* p = path->path;
  if (path->len >= 3)
    return (isalpha(p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) || (p[0] == '\\' && p[1] == '\\');
  else
    return (isalpha(p[0]) && p[1] == ':') || (p[0] == '\\' && p[1] == '\\');
#else
  return path->path[0] == '/';
#endif
}

bool ldkPathIsRelative(LDKPath* path)
{
  return ! ldkPathIsAbsolute(path);
}


