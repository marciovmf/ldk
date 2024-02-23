#include "ldk/asset/material.h"
#include "ldk/asset/texture.h"
#include "ldk/asset/image.h"
#include "ldk/common.h"
#include "ldk/module/asset.h"
#include "ldk/module/renderer.h"
#include "ldk/os.h"
#include <stdlib.h>
#include <string.h>

#ifdef LDK_OS_WINDOWS
#define strtok_r strtok_s
#endif

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

static bool internalParseInt(const char* path, int line, const char* input, int* out)
{
  const char* p = input;
  if ((*input == '-' || *input == '+') && *(input + 1) != 0)
    input++;

  while(*input)
  {
    if (*input < '0' || *input > '9')
    {
      ldkLogError("%s at line %d: Error parsing integer value", path, line);
      return false;
    }
    input++;
  }

  *out = atoi(p);
  return true;
}

static bool internalParseFloat(const char* path, int line, const char* input, float* out)
{
  int dotCount = 0;
  int len = 0;
  bool error = false;
  while(*input)
  {
    len++;
    if (*input == '.') { dotCount++; }

    if (dotCount > 1 || (*input != '.' && (*input < '0' || *input > '9' )))
    {
      error = true;
      break;
    }
    input++;
  }

  // Floats can not end with a dot or contain just a dot
  input -=len;
  error |= (len == 0 || input[len-1] == '.');
  error |= (len == 1 && input[0] == '.');

  if (error)
  {
    ldkLogError("%s at line %d: Error parsing float value", path, line);
    return false;
  }

  *out = (float) atof(input);
  return true;
}

bool ldkAssetMaterialLoadFunc(const char* path, LDKAsset asset)
{
  LDKMaterial* material = (LDKMaterial*) asset;
  LDKShader* program;
  bool success = true;

  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
  {
    if (ldkStringEndsWith(path, "default.material"))
      return LDK_HANDLE_INVALID;

    // Copy the default material settings over this material so the game does not break
    LDKMaterial* defaultMaterial = ldkAssetGet(LDKMaterial, "assets/default.material");
    LDKMaterial* material = (LDKMaterial*) asset;
    material->numParam = defaultMaterial->numParam;
    material->numTextures = defaultMaterial->numTextures;
    material->program = defaultMaterial->program;
    for (uint32 i = 0; i < material->numTextures; i++)
    {
      material->textures[i] = defaultMaterial->textures[i];
    }

    return material;
  }
  buffer[fileSize] = 0;

  const char* SPACE_OR_TAB = "\t ";
  bool created = false;

  char* context;
  char* line = strtok_r((char*) buffer, "\n\r", &context);
  int lineNumber = 0;
  while (line)
  {
    lineNumber++;
    line = internalSkipWhiteSpace(line);

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

      lhs = internalSkipWhiteSpace(lhs);
      rhs = internalSkipWhiteSpace(rhs);

      if (strncmp("shader", lhs, strlen(lhs)) == 0)
      {
        if (ldkStringEndsWith(rhs, ".shader"))
        {
          program = ldkAssetGet(LDKShader, rhs);
          ldkMaterialCreate(program , material);
          material->program = program->asset.handle;
          created = true;
        }
        else
        {
          ldkLogError("Error parsing material file '%s' at line %d: shader param '%s' must point to a .shader file", path, lineNumber, lhs);
        }
      }
      else
      {
        if (!created)
        {
          ldkLogError("Error parsing material file '%s' at line %d: shader param must appear begore any other material parameter", path, lineNumber);
          break;
        }

        char* varNameAndTypeContext;
        char* varType = strtok_r(lhs,   SPACE_OR_TAB, &varNameAndTypeContext);
        char* varName = strtok_r(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);

        if (varName == NULL || strlen(varName) == 0)
        {
          ldkLogError("%s at line %d: Expected variable name.", path, lineNumber);
          break;
        }

        if (strncmp("texture2d", varType, strlen(varType)) == 0)
        {
          if (ldkStringEndsWith(rhs, ".texture"))
          {
            LDKTexture* texture = ldkAssetGet(LDKTexture, rhs);
            ldkMaterialParamSetTexture(material, varName, texture);
          }
          else
          {
            ldkLogError("Error parsing material file '%s' at line %d: Texture param '%s' must point to a .texture file", path, lineNumber, varName);
            success = false;
          }
        }
        else if (strncmp("texture3d", varType, strlen(varType)) == 0)
        {
          char* a = strtok_r(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          ldkLogWarning("NOT IMPLEMENTED YET! texture3d variable %s = %s", varName, a);
        }
        else if (strncmp("int", varType, strlen(varType)) == 0)
        {
          int val = 0;
          char* a = strtok_r(rhs,  SPACE_OR_TAB, &varNameAndTypeContext);
          internalParseInt(path, lineNumber, a, &val);
          ldkMaterialParamSetInt(material, varName, val);
        }
        else if (strncmp("float", varType, strlen(varType)) == 0)
        {
          float val;
          char* a = strtok_r(rhs,  SPACE_OR_TAB, &varNameAndTypeContext);
          internalParseFloat(path, lineNumber, a, &val);
          ldkMaterialParamSetFloat(material, varName, val);
        }
        else if (strncmp("vec2", varType, strlen(varType)) == 0)
        {
          Vec2 val;
          char* x = strtok_r(rhs,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          internalParseFloat(path, lineNumber, x, &val.x);
          internalParseFloat(path, lineNumber, y, &val.y);
          ldkMaterialParamSetVec2(material, varName, val);
        }
        else if (strncmp("vec3", varType, strlen(varType)) == 0)
        {
          Vec3 val;
          char* x = strtok_r(rhs,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          char* z = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          internalParseFloat(path, lineNumber, x, &val.x);
          internalParseFloat(path, lineNumber, y, &val.y);
          internalParseFloat(path, lineNumber, z, &val.z);
          ldkMaterialParamSetVec3(material, varName, val);
        }
        else if (strncmp("vec4", varType, strlen(varType)) == 0)
        {
          Vec4 val;
          char* x = strtok_r(rhs,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          char* z = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          char* w = strtok_r(NULL, SPACE_OR_TAB, &varNameAndTypeContext);
          internalParseFloat(path, lineNumber, x, &val.x);
          internalParseFloat(path, lineNumber, y, &val.y);
          internalParseFloat(path, lineNumber, z, &val.z);
          internalParseFloat(path, lineNumber, w, &val.w);
          ldkMaterialParamSetVec4(material, varName, val);
        }
        else
        {
          ldkLogError("Error parsing material file '%s' at line %d: Unknow param type '%s'.", path, lineNumber, varType);
        }

        char* nullToken = strtok_r(NULL,   "  \t", &varNameAndTypeContext);
        if(nullToken != NULL)
        {
          ldkLogError("Error parsing material file '%s' at line %d: Unexpected token '%s'.", path, lineNumber, nullToken);
        }
      }
    }
    line = strtok_r(NULL, "\n\r", &context);
  }

  ldkOsMemoryFree(buffer);
  return success;
}

void ldkAssetMaterialUnloadFunc(LDKAsset handle)
{
  ldkMaterialDestroy(handle);
}
