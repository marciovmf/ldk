#include "ldk/common.h"
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
  smallString->length = vsnprintf((char*) &smallString->str, LDK_SMALL_STRING_MAX_LENGTH, fmt, argList);
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

