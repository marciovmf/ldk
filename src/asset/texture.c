#include "ldk/common.h"
#include "ldk/module/asset.h"
#include "ldk/asset/texture.h"
#include "ldk/asset/image.h"
#include "ldk/os.h"
#include <stdlib.h>
#include <string.h>

//TODO(marcio): Move this function to common
static char* internalSkipWhiteSpace(char* input)
{
  if (input == NULL)
    return input;

  char* p = input;
  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

LDKHTexture ldkAssetTextureLoadFunc(const char* path)
{
  // Defaults
  LDKTextureParam mipmapMode  = LDK_TEXTURE_MIPMAP_MODE_NONE;
  LDKTextureParam wrap        = LDK_TEXTURE_WRAP_CLAMP_TO_EDGE;
  LDKTextureParam filterMin   = LDK_TEXTURE_FILTER_LINEAR;
  LDKTextureParam filterMax   = LDK_TEXTURE_FILTER_LINEAR;
  LDKHImage image;

  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
    return LDK_HANDLE_INVALID;

  buffer[fileSize] = 0;
  char* context;
  int lineNumber = 0;
  bool error = false;

  char* line = strtok_s((char*) buffer, "\n\r", &context);

  while (line)
  {
    lineNumber++;
    line = internalSkipWhiteSpace(line);

    if (line[0] != '#')
    {
      char* entryContext;
      char* lhs = strtok_s(line, ":", &entryContext);
      char* rhs = strtok_s(NULL, ":", &entryContext);

      if (lhs == NULL || rhs == NULL)
      {
        ldkLogError("Error parsing material file '%s' at line %d: Invalid entry format.", path, lineNumber);
        error = true;
        break;
      }

      lhs = internalSkipWhiteSpace(lhs);
      rhs = internalSkipWhiteSpace(rhs);
      size_t rhsLen = strlen(rhs);

      if (strncmp("image", lhs, strlen(lhs)) == 0)
      {
        image = ldkAssetGet(rhs);
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
          error = true;
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
            error = true;
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
          error = true;
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
          error = true;
        }
      }
    }
    line = strtok_s(NULL, "\n\r", &context);
  }

  LDKImage* imagePtr = ldkAssetImageGetPointer(image);
  LDKHTexture texture = ldkTextureCreate(mipmapMode, wrap, filterMin, filterMax);
  ldkTextureData(texture, imagePtr->width, imagePtr->height, imagePtr->data, 32);

  return texture;
}

void ldkAssetTextureUnloadFunc(LDKHTexture handle)
{
}
