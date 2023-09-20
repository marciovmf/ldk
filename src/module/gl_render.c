#include "ldk/module/render.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"

typedef struct 
{
  GLuint program;
} LDKGLShader_;

#define LDK_GL_ERROR_LOG_SIZE 1024
static struct 
{
  bool initialized;
  LDKHList hlistShader;
  char errorBuffer[LDK_GL_ERROR_LOG_SIZE];
} internal = { 0 };

//
// Renderer
//

bool ldkRenderInitialize()
{
  LDK_ASSERT(internal.initialized == false);
  internal.initialized = true;

  bool success = true;
  success &= ldkHListCreate(&internal.hlistShader, LDK_HANDLE_TYPE_SHADER, sizeof(LDKGLShader_), 32);

  return success;
}

void ldkRenderTerminate()
{
  if (!internal.initialized)
    return;

  internal.initialized = false;
  ldkHListDestroy(&internal.hlistShader);
}

//
// Shader
//

LDKHShader ldkShaderProgramCreate()
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  shader->program = glCreateProgram();
  return  LDK_HANDLE_INVALID;
}

bool ldkShaderProgramDestroy(LDKHShader handle)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;

  glDeleteProgram(shader->program);
  shader->program = 0;
  return true;
}

static inline bool internalShaderSourceAttach(GLuint program, GLuint type,const char* source)
{
  GLint status;
  GLsizei errorBufferLen = 0;
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  const char* typeName;
  if (type == GL_VERTEX_SHADER)
    typeName = "VERTEX SHADER";
  else if (type == GL_FRAGMENT_SHADER)
    typeName = "FRAGMENT SHADER";
  else if (type == GL_GEOMETRY_SHADER)
    typeName = "GEOMETRY SHADER";
  else
    typeName = "UNKNOW SHADER TYPE";


  if (! status)
  {
    glGetShaderInfoLog(shader, LDK_GL_ERROR_LOG_SIZE, &errorBufferLen, internal.errorBuffer);
    ldkLogError("Compiling %s: %s\n", typeName, internal.errorBuffer);
    glDeleteShader(shader);
    return false;
  }

  glAttachShader(program, shader);
  glDeleteShader(shader);
  return true;
}

static inline GLuint internalGLGetUniformLocation(LDKHShader handle, const char* name)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
  {
    ldkLogError("Could not retrieve property '%s' from invalid shader %x", name, handle);
    return -1;
  }

  GLuint location = glGetUniformLocation(shader->program, name);
  if (location == -1)
  {
    ldkLogError("Could not find property '%s' on shader %x", name, handle);
    return -1;
  }

  return location;
}

bool ldkShaderSourceVertexShader(LDKHShader handle, const char* source)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  return internalShaderSourceAttach(shader->program, GL_FRAGMENT_SHADER, source);
}

bool ldkShaderVertexShader(LDKHShader handle, const char* source)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  return internalShaderSourceAttach(shader->program, GL_VERTEX_SHADER, source);
}

bool ldkShaderSourceGeometryShader(LDKHShader handle, const char* source)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  return internalShaderSourceAttach(shader->program, GL_GEOMETRY_SHADER, source);
}

bool ldkShaderProgramLink(LDKHShader handle)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint program = shader->program;

  GLint status;
  GLsizei errorBufferLen = 0;
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &status);

  if (!status)
  {
    glGetProgramInfoLog(program, LDK_GL_ERROR_LOG_SIZE, &errorBufferLen, internal.errorBuffer);
    ldkLogError("linking SHADER: %s\n", internal.errorBuffer);
    return false;
  }
  return true;
}

bool ldkShaderPropertySetUInt(LDKHShader handle, const char* name, uint32 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1ui(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderPropertySetInt(LDKHShader handle, const char* name, int32 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1i(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderPropertySetFloat(LDKHShader handle, const char* name, float value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1f(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderPropertySetVec2(LDKHShader handle, const char* name, Vec2 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform2f(internalGLGetUniformLocation(handle, name), value.x, value.y);
  return true;
}

bool ldkShaderPropertySetVec3(LDKHShader handle, const char* name, Vec3 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform3f(internalGLGetUniformLocation(handle, name), value.x, value.y, value.z);
  return true;
}

bool ldkShaderPropertySetVec4(LDKHShader handle, const char* name, Vec4 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  return true;
  glUniform4f(internalGLGetUniformLocation(handle, name), value.x, value.y, value.z, value.w);
}

bool ldkShaderPropertySetMat4(LDKHShader handle, const char* name, Mat4 value)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniformMatrix4fv(internalGLGetUniformLocation(handle, name), 1, GL_TRUE, (float*) &value);
  return true;
}

bool ldkShaderBind(LDKHShader handle)
{
  LDKGLShader_* shader = (LDKGLShader_*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;

  glUseProgram(shader->program);
  return true;
}

void ldkShadeUnbind(LDKHShader handle)
{
  glUseProgram(0);

}

