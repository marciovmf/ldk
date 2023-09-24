#include "ldk/asset/material.h"
#include "ldk/common.h"
#include "ldk/module/asset.h"
#include "ldk/os.h"
#include <stdlib.h>
#include <string.h>

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
  while(*input)
  {
    if (*input < '0' || *input > '9')
    {
      ldkLogError("%s at line %d: Error parsing integer value", path, line);
      return false;
    }
    input++;
  }

  *out = atoi(input);
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

LDKHMaterial ldkMaterialLoadFunc(const char* path)
{
  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  buffer[fileSize] = 0;

  LDKHShader vs = LDK_HANDLE_INVALID;
  LDKHShader fs = LDK_HANDLE_INVALID;
  LDKHShader gs = LDK_HANDLE_INVALID;
  LDKHShaderProgram program = LDK_HANDLE_INVALID;
  LDKHMaterial material = LDK_HANDLE_INVALID;
  const char* SPACE_OR_TAB = "\t ";
  bool error = false;

  char* context;
  char* line = strtok_s((char*) buffer, "\n\r", &context);
  int lineNumber = 0;
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

      if (strncmp("vert-shader", lhs, strlen(lhs)) == 0)
      {
        vs = (LDKHShader) ldkAssetGet(rhs);
      }
      else if (strncmp("frag-shader", lhs, strlen(lhs)) == 0)
      {
        fs = (LDKHShader) ldkAssetGet(rhs);
      }
      else if (strncmp("geom-shader", lhs, strlen(lhs)) == 0)
      {
        gs = (LDKHShader) ldkAssetGet(rhs);
      }
      else
      {
        if (program == LDK_HANDLE_INVALID && error == false)
        {
          program = ldkShaderProgramCreate(vs, fs, gs);
          if (program == LDK_HANDLE_INVALID)
            error = true;

          material = ldkMaterialCreate(program);
          if (material == LDK_HANDLE_INVALID)
            error = true;
        }

        char* varNameAndTypeContext;
        char* varType = strtok_s(lhs,   SPACE_OR_TAB, &varNameAndTypeContext);
        char* varName = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);

        if (varName == NULL || strlen(varName) == 0)
        {
          ldkLogError("%s at line %d: Expected variable name.", path, lineNumber);
          return false;
        }

        if (strncmp("texture2d", varType, strlen(varType)) == 0)
        {
          char* a = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          ldkLogWarning("NOT IMPLEMENTED YET! texture2d variable %s = %s", varName, a);
        }
        else if (strncmp("texture3d", varType, strlen(varType)) == 0)
        {
          char* a = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          ldkLogWarning("NOT IMPLEMENTED YET! texture3d variable %s = %s", varName, a);
        }
        else if (strncmp("int", varType, strlen(varType)) == 0)
        {
          int val = 0;
          char* a = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          error &= internalParseInt(path, lineNumber, a, &val);
          error &= ldkMaterialParamSetInt(material, varName, val);
        }
        else if (strncmp("float", varType, strlen(varType)) == 0)
        {
          float val;
          char* a = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          error &= internalParseFloat(path, lineNumber, a, &val);
          error &= ldkMaterialParamSetFloat(material, varName, val);
        }
        else if (strncmp("vec2", varType, strlen(varType)) == 0)
        {
          Vec2 val;
          char* x = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          error &= internalParseFloat(path, lineNumber, x, &val.x);
          error &= internalParseFloat(path, lineNumber, y, &val.y);
          error &= ldkMaterialParamSetVec2(material, varName, val);
        }
        else if (strncmp("vec3", varType, strlen(varType)) == 0)
        {
          Vec3 val;
          char* x = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* z = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          error &= internalParseFloat(path, lineNumber, x, &val.x);
          error &= internalParseFloat(path, lineNumber, y, &val.y);
          error &= internalParseFloat(path, lineNumber, z, &val.z);
          error &= ldkMaterialParamSetVec3(material, varName, val);
        }
        if (strncmp("vec4", varType, strlen(varType)) == 0)
        {
          Vec4 val;
          char* x = strtok_s(rhs,   SPACE_OR_TAB, &varNameAndTypeContext);
          char* y = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* z = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          char* w = strtok_s(NULL,  SPACE_OR_TAB, &varNameAndTypeContext);
          error &= internalParseFloat(path, lineNumber, x, &val.x);
          error &= internalParseFloat(path, lineNumber, y, &val.y);
          error &= internalParseFloat(path, lineNumber, z, &val.z);
          error &= internalParseFloat(path, lineNumber, w, &val.w);
          error &= ldkMaterialParamSetVec4(material, varName, val);
        }

        char* nullToken = strtok_s(NULL,   "  \t", &varNameAndTypeContext);
        if(nullToken != NULL)
        {
          ldkLogError("Error parsing material file '%s' at line %d: Unexpected token '%s'.", path, lineNumber, nullToken);
          error = true;
        }
      }

    }
    line = strtok_s(NULL, "\n\r", &context);
  }

  ldkOsMemoryFree(buffer);

  return 0;
}

void ldkMaterialUnloadFunc(LDKHMaterial handle)
{
}
