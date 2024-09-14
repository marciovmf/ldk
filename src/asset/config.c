#include "ldk/os.h"
#include "ldk/asset/config.h"
#include "ldk/hashmap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef LDK_OS_WINDOWS
#define strtok_r strtok_s
#endif

bool keyCompareFunc(const void* key1, const void* key2)
{
  return strncmp((const char*) key1, (const char*) key2, 128) == 0;
}

void internalDestroyEntry(void* key, void* value)
{
  ldkOsMemoryFree((void*) value);
}

static char* internalSkipWhiteSpace(char* input)
{
  if (input == NULL)
    return input;

  char* p = input;
  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

bool ldkAssetConfigLoadFunc(const char* path, LDKAsset asset)
{
  bool success = true;
  size_t fileSize;

  LDKConfig* config = (LDKConfig*) asset;
  config->map = NULL;
  config->buffer = (char*) ldkOsFileReadOffset(path, &fileSize, 0, 0);

  if (config->buffer == NULL)
  {
    return false;
  }

  // nullterminate the file buffer
  config->buffer[fileSize-1] = 0;

  config->map = ldkHashMapCreate((ldkHashMapHashFunc) ldkHashStr, keyCompareFunc);
  char* context;
  char* line = strtok_r((char*) config->buffer, "\n\r", &context);
  int lineNumber = 0;

  while (line)
  {
    lineNumber++;
    line = internalSkipWhiteSpace(line);
    LDKConfigVariable v;

    if (line[0] != '#')
    {
      char* entryContext;
      char* lhs = strtok_r(line, ":", &entryContext);
      char* rhs = strtok_r(NULL, ":", &entryContext);

      if (lhs == NULL || rhs == NULL)
      {
        ldkLogError("Error parsing config file '%s' at line %d: Invalid entry format.", path, lineNumber);
        break;
      }

      lhs = internalSkipWhiteSpace(lhs);
      rhs = internalSkipWhiteSpace(rhs);
      v.name = lhs;

      double f0, f1, f2, f3;
      char suffix;

      int64 i0;
      int matches = sscanf(rhs, "%lld%c", &i0, &suffix);
      if (matches == 2 && suffix == 'L')
      {
        v.type = LDK_CONFIG_VAR_TYPE_LONG;
        v.longValue = i0;
      }
      else
      {
        matches = sscanf(rhs, "%lf %lf %lf %lf", &f0, &f1, &f2, &f3);
        if (matches == 4)
        {
          v.type = LDK_CONFIG_VAR_TYPE_VEC4;
          v.vec4Value = vec4((float) f0, (float) f1, (float) f2, (float) f3);
        }
        else if (matches == 3)
        {
          v.type = LDK_CONFIG_VAR_TYPE_VEC3;
          v.vec3Value = vec3((float) f0, (float) f1, (float) f2);
        }
        else if (matches == 2)
        {
          v.type = LDK_CONFIG_VAR_TYPE_VEC2;
          v.vec2Value = vec2((float) f0, (float) f1);
        }
        else if (matches == 1)
        {
          v.type = LDK_CONFIG_VAR_TYPE_DOUBLE;
          v.doubleValue = (float) f0;
        }
        else if (matches == 0)
        {
          v.type = LDK_CONFIG_VAR_TYPE_STRING;
          v.strValue = rhs;
        }
      }

      LDKConfigVariable* mem = (LDKConfigVariable*) ldkOsMemoryAlloc(sizeof(LDKConfigVariable));
      memcpy((void*) mem, (void*) &v, sizeof(LDKConfigVariable));
      ldkHashMapInsert(config->map, (void*) mem->name, mem);
    }

    line = strtok_r(NULL, "\n\r", &context);
  }

  return success;
}

void ldkAssetConfigUnloadFunc(LDKAsset asset)
{
  LDKConfig* config = (LDKConfig*) asset;
  if (config->map)
  {
    ldkHashMapDestroyWith((config->map), internalDestroyEntry);
    config->map = NULL;
  }

  if(config->buffer)
  {
    ldkOsMemoryFree(config->buffer);
    config->buffer = NULL;
  }
}

int64 ldkConfigGetLong(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_LONG)
      return (int64) v->longValue;
    else if (v && v->type == LDK_CONFIG_VAR_TYPE_DOUBLE)
      return (int64) v->doubleValue;
  }

  ldkLogWarning("Unable to retrieve or cast long::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return 0;
}

int32 ldkConfigGetInt(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_LONG)
      return (int32) v->longValue;
    else if (v && v->type == LDK_CONFIG_VAR_TYPE_DOUBLE)
      return (int32) v->doubleValue;
  }

  ldkLogWarning("Unable to retrieve or cast int::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return 0;
}

double ldkConfigGetDouble(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);

    if (v && v->type == LDK_CONFIG_VAR_TYPE_LONG)
      return (double) v->longValue;
    else if (v && v->type == LDK_CONFIG_VAR_TYPE_DOUBLE)
      return v->doubleValue;
  }

  ldkLogWarning("Unable to retrieve or cast double::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return 0.0;
}

float ldkConfigGetFloat(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);

    if (v && v->type == LDK_CONFIG_VAR_TYPE_LONG)
      return (float) v->longValue;
    else if (v && v->type == LDK_CONFIG_VAR_TYPE_DOUBLE)
      return (float) v->doubleValue;
  }

  ldkLogWarning("Unable to retrieve or cast float::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return 0.0;
}

const char* ldkConfigGetString(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_STRING)
      return v->strValue;
  }

  ldkLogWarning("Unable to retrieve string::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return NULL;
}

Vec2 ldkConfigGetVec2(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_VEC2)
      return v->vec2Value;
  }

  ldkLogWarning("Unable to retrieve vec2::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return vec2Zero();
}

Vec3 ldkConfigGetVec3(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_VEC3)
      return v->vec3Value;
  }

  ldkLogWarning("Unable to retrieve vec3::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return vec3Zero();
}

Vec4 ldkConfigGetVec4(LDKConfig* config, const char* name)
{
  if (config)
  {
    LDKConfigVariable* v = ldkHashMapGet(config->map, (void*) name);
    if (v && v->type == LDK_CONFIG_VAR_TYPE_VEC4)
      return v->vec4Value;
  }

  ldkLogWarning("Unable to retrieve vec4::%s variable from config %s", name, config ? config->asset.path.path : "NULL");
  return vec4Zero();
}
