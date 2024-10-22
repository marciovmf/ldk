#include "ldk/common.h"
#include "ldk/module/asset.h"
#include "ldk/asset/texture.h"
#include "ldk/asset/image.h"
#include "ldk/module/rendererbackend.h"
#include "ldk/os.h"
#include <stdlib.h>
#include <string.h>

#ifdef LDK_OS_WINDOWS
#define strtok_r strtok_s
#endif

//TODO(marcio): Move this function to common
static char* s_skipWhiteSpace(char* input)
{
  if (input == NULL)
    return input;

  char* p = input;
  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

bool ldkAssetTextureLoadFunc(const char* path, LDKAsset asset)
{
  // Defaults
  LDKTextureParamMipmap mipmapMode  = LDK_TEXTURE_MIPMAP_MODE_NONE;
  LDKTextureParamWrap wrap          = LDK_TEXTURE_WRAP_CLAMP_TO_EDGE;
  LDKTextureParamFilter filterMin   = LDK_TEXTURE_FILTER_LINEAR;
  LDKTextureParamFilter filterMax   = LDK_TEXTURE_FILTER_LINEAR;
  LDKImage* image;
  LDKTexture* texture = (LDKTexture*) asset;

  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
    return LDK_HANDLE_INVALID;

  buffer[fileSize] = 0;
  char* context;
  int lineNumber = 0;

  char* line = strtok_r((char*) buffer, "\n\r", &context);

  while (line)
  {
    lineNumber++;
    line = s_skipWhiteSpace(line);

    if (line[0] != '#')
    {
      char* entryContext;
      char* lhs = strtok_r(line, ":", &entryContext);
      char* rhs = strtok_r(NULL, ":", &entryContext);

      if (lhs == NULL || rhs == NULL)
      {
        ldkLogError("Error parsing material file '%s' at line %d: Invalid entry format.", path, lineNumber);
        break;
      }

      lhs = s_skipWhiteSpace(lhs);
      rhs = s_skipWhiteSpace(rhs);
      size_t rhsLen = strlen(rhs);

      if (strncmp("image", lhs, strlen(lhs)) == 0)
      {
        image = ldkAssetGet(LDKImage, rhs);
      }
      else if (strncmp("wrap", lhs, strlen(lhs)) == 0)
      {
        if (strncmp(rhs, "clamp-to-edge", rhsLen) == 0)
        {
          wrap = LDK_TEXTURE_WRAP_CLAMP_TO_EDGE;
        }
        else if (strncmp(rhs, "clamp-to-border", rhsLen) == 0)
        {
          wrap = LDK_TEXTURE_WRAP_CLAMP_TO_BORDER;
        }
        else if (strncmp(rhs, "repeat", rhsLen) == 0)
        {
          wrap = LDK_TEXTURE_WRAP_REPEAT;
        }
        else
        {
          ldkLogError("Error parsing texture file '%s' at line %d: Invalid texture wrap mode\nValid modes are: [clamp-to-edge, clamp-to-border, repeat].", path, lineNumber);
        }
      }
      else if (strncmp("filter-min", lhs, strlen(lhs)) == 0)
      {
        if (strncmp(rhs, "linear", rhsLen) == 0)
        {
          filterMin = LDK_TEXTURE_FILTER_LINEAR;
        }
        else 
          if (strncmp(rhs, "nearest", rhsLen) == 0)
          {
            filterMin = LDK_TEXTURE_FILTER_NEAREST;
          }
          else
          {
            ldkLogError("Error parsing texture file '%s' at line %d: Invalid filter-min mode\nValid modes are: [linear, nearest].", path, lineNumber);
          }
      }
      else if (strncmp("filter-max", lhs, strlen(lhs)) == 0)
      {
        if (strncmp(rhs, "linear", rhsLen) == 0)
        {
          filterMax = LDK_TEXTURE_FILTER_LINEAR;
        }
        else if (strncmp(rhs, "nearest", rhsLen) == 0)
        {
          filterMax = LDK_TEXTURE_FILTER_NEAREST;
        }
        else
        {
          ldkLogError("Error parsing texture file '%s' at line %d: Invalid filter-max mode\nValid modes are: [linear, nearest].", path, lineNumber);
        }
      }
      else if (strncmp("mipmap", lhs, strlen(lhs)) == 0)
      {
        if (strncmp(rhs, "none", rhsLen) == 0)
        {
          mipmapMode = LDK_TEXTURE_MIPMAP_MODE_NONE;
        }
        else if (strncmp(rhs, "linear", rhsLen) == 0)
        {
          mipmapMode = LDK_TEXTURE_MIPMAP_MODE_LINEAR;
        }
        else if (strncmp(rhs, "nearest", rhsLen) == 0)
        {
          mipmapMode = LDK_TEXTURE_MIPMAP_MODE_NEAREST;
        }
        else
        {
          ldkLogError("Error parsing texture file '%s' at line %d: Invalid mipmap mode\nValid modes are: [linear, nearest].", path, lineNumber);
        }
      }
    }
    line = strtok_r(NULL, "\n\r", &context);
  }

  bool success = ldkTextureCreate(mipmapMode, wrap, filterMin, filterMax, texture);
  success &= ldkTextureData(texture, image->width, image->height, image->data, 32);
  return success;
}

void ldkAssetTextureUnloadFunc(LDKAsset asset)
{
  LDKTexture* texture = (LDKTexture*) asset;
  ldkTextureDestroy(texture);
}
