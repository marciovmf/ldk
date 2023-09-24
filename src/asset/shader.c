#include "ldk/asset/shader.h"
#include "ldk/module/render.h"
#include "ldk/common.h"
#include "ldk/os.h"

LDKHShader ldkShaderLoadFunc(const char* path)
{
  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
  {
    ldkLogError("Unable to read shader file '%s'", path);
    return LDK_HANDLE_INVALID;
  }

  buffer[fileSize] = 0;
  LDKHShader handle = LDK_HANDLE_INVALID;

  if (ldkStringEndsWith(path, ".fs"))
  {
    handle = ldkFragmentShaderCreate((const char*) buffer);
  }
  else if(ldkStringEndsWith(path, ".vs"))
  {
    handle = ldkVertexShaderCreate((const char*) buffer);
  }
  else if(ldkStringEndsWith(path, ".gs"))
  {
    handle = ldkGeometryShaderCreate((const char*) buffer);
  }
  else {
    ldkLogError("Unknown shader file type '%s'", path);
    return LDK_HANDLE_INVALID;
  }

  ldkOsMemoryFree(buffer);
  return handle;
}

void ldkShaderUnloadFunc(LDKHShader handle)
{
  ldkShaderDestroy(handle);
}
