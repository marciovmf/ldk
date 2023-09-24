#include "ldk/module/render.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"

typedef struct 
{
  GLuint id;
  int32 isProgram;
} LDKGLShader;

#ifndef LDK_MATERIAL_MAX_PROPERTIES
#define LDK_MATERIAL_MAX_PROPERTIES 8
#endif

typedef struct
{
  LDKTypeId typeId;
  LDKSmallStr name;
  GLuint location;
  union
  {
    LDKHandle textureValue;
    int       intValue;
    float     floatValue;
    Vec2      vec2Value;
    Vec3      vec3Value;
    Vec4      vec4Value;
  };
}LDKMaterialParam;

typedef struct
{
  LDKHShaderProgram program;
  LDKMaterialParam property[LDK_MATERIAL_MAX_PROPERTIES];
  uint32 numProperties;

}LDKGLMaterial;


#define LDK_GL_ERROR_LOG_SIZE 1024
static struct 
{
  bool initialized;
  LDKHList hlistShader;
  LDKHList hlistMaterial;
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
  success &= ldkHListCreate(&internal.hlistShader, typeid(LDKHShader), sizeof(LDKGLShader), 32);
  success &= ldkHListCreate(&internal.hlistMaterial, typeid(LDKHMaterial), sizeof(LDKGLMaterial), 32);
  return success;
}

void ldkRenderTerminate()
{
  if (!internal.initialized)
    return;

  internal.initialized = false;
  ldkHListDestroy(&internal.hlistShader);
  ldkHListDestroy(&internal.hlistMaterial);
}

//
// Shader
//

LDKHShaderProgram ldkShaderProgramCreate(LDKHShader hVertex, LDKHShader hFragment, LDKHShader hGeometry)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint program = glCreateProgram();
  shader->id = program;
  shader->isProgram = 1;

  if (hVertex != LDK_HANDLE_INVALID)
  {
    LDKGLShader* vertShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, hVertex);
    if (vertShader != NULL)
      glAttachShader(program, vertShader->id);
  }

  if (hFragment != LDK_HANDLE_INVALID)
  {
    LDKGLShader* fragShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, hFragment);
    if (fragShader != NULL)
      glAttachShader(program, fragShader->id);
  }

  if (hGeometry != LDK_HANDLE_INVALID)
  {
    LDKGLShader* geomShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, hGeometry);
    if (geomShader != NULL)
      glAttachShader(program, geomShader->id);
  }

  GLint status;
  GLsizei errorBufferLen = 0;
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &status);

  if (!status)
  {
    glGetProgramInfoLog(program, LDK_GL_ERROR_LOG_SIZE, &errorBufferLen, internal.errorBuffer);
    ldkLogError("linking SHADER: %s\n", internal.errorBuffer);
    return  LDK_HANDLE_INVALID;
  }
  return handle;
}

bool ldkShaderProgramDestroy(LDKHShaderProgram handle)
{
  return ldkShaderDestroy(handle);
}

bool ldkShaderDestroy(LDKHShader handle)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;

  glDeleteProgram(shader->id);
  shader->id = 0;
  shader->isProgram = 0;
  return true;
}

static inline GLuint internalShaderSourceCompile(GLuint type, const char* source)
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
    return 0;
  }

  return shader;
}

static inline GLuint internalGLGetUniformLocation(LDKHShaderProgram handle, const char* name)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
  {
    ldkLogError("Could not retrieve property '%s' from invalid shader %x", name, handle);
    return -1;
  }

  GLuint location = glGetUniformLocation(shader->id, name);
  if (location == -1)
  {
    ldkLogError("Could not find property '%s' on shader %x", name, handle);
    return -1;
  }

  return location;
}

LDKHShader ldkFragmentShaderCreate(const char* source)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint id = internalShaderSourceCompile(GL_FRAGMENT_SHADER, source);
  shader->id = id;
  shader->isProgram = 0;
  return handle;
}

LDKHShader ldkVertexShaderCreate(const char* source)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint id = internalShaderSourceCompile(GL_VERTEX_SHADER, source);
  shader->id = id;
  shader->isProgram = 0;
  return handle;
}

LDKHShader ldkGeometryShaderCreate(const char* source)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint id = internalShaderSourceCompile(GL_GEOMETRY_SHADER, source);
  shader->id = id;
  shader->isProgram = 0;
  return handle;
}

bool ldkShaderParamSetUInt(LDKHShader handle, const char* name, uint32 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1ui(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderParamSetInt(LDKHShader handle, const char* name, int32 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1i(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderParamSetFloat(LDKHShader handle, const char* name, float value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform1f(internalGLGetUniformLocation(handle, name), value);
  return true;
}

bool ldkShaderParamSetVec2(LDKHShader handle, const char* name, Vec2 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform2f(internalGLGetUniformLocation(handle, name), value.x, value.y);
  return true;
}

bool ldkShaderParamSetVec3(LDKHShader handle, const char* name, Vec3 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniform3f(internalGLGetUniformLocation(handle, name), value.x, value.y, value.z);
  return true;
}

bool ldkShaderParamSetVec4(LDKHShader handle, const char* name, Vec4 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  return true;
  glUniform4f(internalGLGetUniformLocation(handle, name), value.x, value.y, value.z, value.w);
}

bool ldkShaderParamSetMat4(LDKHShader handle, const char* name, Mat4 value)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;
  glUniformMatrix4fv(internalGLGetUniformLocation(handle, name), 1, GL_TRUE, (float*) &value);
  return true;
}

bool ldkShaderBind(LDKHShader handle)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader)
    return false;

  if (!shader->isProgram)
    return false;

  glUseProgram(shader->id);
  return true;
}

void ldkShadeUnbind(LDKHShader handle)
{
  glUseProgram(0);

}

//
// Material
//

static inline LDKHShaderProgram internalShaderProgramFromMaterial(LDKHMaterial hMaterial)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, hMaterial);
  if (!material)
    return false;

  return material->program;
}

LDKHMaterial ldkMaterialCreate(LDKHShaderProgram hProgram)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistMaterial);
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  material->numProperties = 0;
  material->program = hProgram;

  return handle;
}

bool ldkMaterialDestroy(LDKHMaterial hMaterial)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, hMaterial);
  if (!material)
    return false;
  return true;
}

bool ldkMaterialParamSetInt(LDKHMaterial hMaterial, const char* name, int value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, hMaterial);
  if (!material)
    return false;

  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, material->program);
  if (!shader)
    return false;

  return ldkShaderParamSetInt(shader->id, name, value);
}

bool ldkMaterialParamSetFloat(LDKHMaterial hMaterial, const char* name, float value)
{
  LDKHShaderProgram program = internalShaderProgramFromMaterial(hMaterial);
  if (program == LDK_HANDLE_INVALID)
  {
    ldkLogError("Failed to set material parameter 'int %s'. Material is invalid or has no valid shader.", name);
    return false;
  }
  return ldkShaderParamSetFloat(program, name, value);
}

bool ldkMaterialParamSetVec2(LDKHMaterial hMaterial, const char* name, Vec2 value)
{
  LDKHShaderProgram program = internalShaderProgramFromMaterial(hMaterial);
  if (program == LDK_HANDLE_INVALID)
  {
    ldkLogError("Failed to set material parameter 'vec2 %s'. Material is invalid or has no valid shader.", name);
    return false;
  }
  return ldkShaderParamSetVec2(program, name, value);
}

bool ldkMaterialParamSetVec3(LDKHMaterial hMaterial, const char* name, Vec3 value)
{
  LDKHShaderProgram program = internalShaderProgramFromMaterial(hMaterial);
  if (program == LDK_HANDLE_INVALID)
  {
    ldkLogError("Failed to set material parameter 'vec3 %s'. Material is invalid or has no valid shader.", name);
    return false;
  }
  return ldkShaderParamSetVec3(program, name, value);
}

bool ldkMaterialParamSetVec4(LDKHMaterial hMaterial, const char* name, Vec4 value)
{
  LDKHShaderProgram program = internalShaderProgramFromMaterial(hMaterial);
  if (program == LDK_HANDLE_INVALID)
  {
    ldkLogError("Failed to set material parameter 'vec4 %s'. Material is invalid or has no valid shader.", name);
    return false;
  }
  return ldkShaderParamSetVec4(program, name, value);
}

