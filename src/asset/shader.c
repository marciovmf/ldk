#include "ldk/asset/shader.h"
#include "ldk/module/renderer.h"
#include "ldk/common.h"
#include "ldk/os.h"

bool ldkAssetShaderLoadFunc(const char* path, LDKAsset asset)
{
  bool success = true;
  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
  {
    ldkLogError("Unable to read shader file '%s'", path);
    return false;
  }

  buffer[fileSize] = 0;
  LDKShader* shader = (LDKShader*) asset;

  bool hasGeometryShader = false;
  if (ldkStringEndsWith(path, ".shader"))
  {
    if (ldkStringStartsWith((const char*) buffer, "//@use-geomety-shader"))
    {
      hasGeometryShader = true;
    }
    const char* src = (const char*) buffer;
    success &= ldkShaderProgramCreate(src, src, hasGeometryShader ? src : NULL, shader);
  }
  else
  {
    ldkLogError("Unknown shader file type '%s'", path);
    return LDK_HANDLE_INVALID;
  }

  ldkOsMemoryFree(buffer);
  return success;
}

void ldkAssetShaderUnloadFunc(LDKAsset asset)
{
  LDK_NOT_IMPLEMENTED();
}
