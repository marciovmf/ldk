#include "ldk/module/renderer.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"

extern const char* ldkOpenglTypeName(GLenum type);

typedef struct 
{
  GLuint id;
  int32 isProgram;
} LDKGLShader;

#ifndef LDK_MATERIAL_MAX_PARAMS
#define LDK_MATERIAL_MAX_PARAMS 8
#endif

typedef struct
{
  GLenum type;
  GLint size;
  LDKSmallStr name;
  GLuint location;
  union
  {
    //LDKHandle textureValue;
    int       intValue;
    float     floatValue;
    Vec2      vec2Value;
    Vec3      vec3Value;
    Vec4      vec4Value;
  };
}LDKMaterialParam;

typedef struct
{
  const char* name;
  LDKHShaderProgram program;
  LDKMaterialParam param[LDK_MATERIAL_MAX_PARAMS];
  uint32 numParam;
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

LDKHShaderProgram ldkShaderProgramCreate(LDKHShader handleVs, LDKHShader handleFs, LDKHShader handleGs)
{
  LDKHShader handle = ldkHListReserve(&internal.hlistShader);
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  GLuint program = glCreateProgram();
  shader->id = program;
  shader->isProgram = 1;

  if (handleVs != LDK_HANDLE_INVALID)
  {
    LDKGLShader* vertShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handleVs);
    if (vertShader != NULL)
      glAttachShader(program, vertShader->id);
  }

  if (handleFs != LDK_HANDLE_INVALID)
  {
    LDKGLShader* fragShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handleFs);
    if (fragShader != NULL)
      glAttachShader(program, fragShader->id);
  }

  if (handleGs != LDK_HANDLE_INVALID)
  {
    LDKGLShader* geomShader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handleGs);
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

static LDKMaterialParam* internalMaterialParamGet(LDKGLMaterial* material, const char* name, GLenum type)
{
  for (uint32 i = 0; i < material->numParam; i++)
  {
    if (material->param[i].type == type && strncmp(name, (const char*) &material->param[i].name, material->param[i].name.length) == 0)
      return &material->param[i];
  }
  ldkLogError("Material '%s' references unknown param '%s %s'", material->name, ldkOpenglTypeName(type), name);
  return NULL;
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

void ldkShaderUnbind(LDKHShader handle)
{
  glUseProgram(0);
}

//
// Material
//

LDKHMaterial ldkMaterialCreate(LDKHShaderProgram handle, const char* name)
{
  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader->isProgram)
  {
    return LDK_HANDLE_INVALID;
  }

  int32 numUniforms = 0;
  glGetProgramiv(shader->id, GL_ACTIVE_UNIFORMS, &numUniforms);
  if (numUniforms > LDK_MATERIAL_MAX_PARAMS)
  {
    ldkLogError("Shader %x declares too many uniforms (%d). MAximum is %d.", handle, numUniforms, LDK_MATERIAL_MAX_PARAMS);
    return LDK_HANDLE_INVALID;
  }

  int32 maxUniformNameLength = 0;
  glGetProgramiv(shader->id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);
  if (numUniforms > LDK_MATERIAL_MAX_PARAMS)
  {
    ldkLogError("Shader %x declares at least one uniform names longer than (%d)", handle, LDK_SMALL_STRING_MAX_LENGTH);
    return LDK_HANDLE_INVALID;
  }

  LDKHMaterial handleMaterial = ldkHListReserve(&internal.hlistMaterial);
  LDKGLMaterial* material     = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handleMaterial);
  material->name      = NULL ? (const char*) "unnamed-material" : name;
  material->numParam  = numUniforms;
  material->program   = handle;
  memset(&material->param, 0, (sizeof(LDKMaterialParam) * LDK_MATERIAL_MAX_PARAMS));

  for (int i = 0; i < numUniforms; i ++)
  {
    LDKMaterialParam* param = &material->param[i];
    glGetActiveUniform(shader->id, i,
        LDK_SMALL_STRING_MAX_LENGTH,
        (GLsizei*) &param->name.length,
        &param->size,
        &param->type,
        (char*) &material->param[i].name.str);

    if ( param->type != GL_INT
        && param->type != GL_FLOAT
        && param->type != GL_FLOAT_VEC2
        && param->type != GL_FLOAT_VEC3
        && param->type != GL_FLOAT_VEC4
        //&&uniformType != GL_SAMPLER_2D 
       )
    {
      ldkLogWarning("Material '%s' references param with unsupported type '%s %s'",
          material->name,
          ldkOpenglTypeName(param->type),
          (const char*) &material->param[i].name);
      continue;
    }
    material->param[i].location = glGetUniformLocation(shader->id, (const char*) &material->param[i].name);
  }

  return handleMaterial;
}

bool ldkMaterialDestroy(LDKHMaterial handle)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material)
    return false;
  return true;
}

bool ldkMaterialParamSetInt(LDKHMaterial handle, const char* name, int value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_INT);
  if (!param) return false;

  param->intValue = value;
  glUniform1i(param->location, value);
  return true;
}

bool ldkMaterialParamSetFloat(LDKHMaterial handle, const char* name, float value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param =  internalMaterialParamGet(material, name, GL_FLOAT);
  if (!param) return false;

  param->floatValue = value;
  glUniform1f(param->location, value);
  return true;
}

bool ldkMaterialParamSetVec2(LDKHMaterial handle, const char* name, Vec2 value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC2);
  if (!param) return false;

  param->vec2Value = value;
  glUniform2f(param->location, value.x, value.y);
  return true;
}

bool ldkMaterialParamSetVec3(LDKHMaterial handle, const char* name, Vec3 value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC3);
  if (!param) return false;

  param->vec3Value = value;
  glUniform3f(param->location, value.x, value.y, value.z);
  return true;
}

bool ldkMaterialParamSetVec4(LDKHMaterial handle, const char* name, Vec4 value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC4);
  if (!param) return false;

  param->vec4Value = value;
  glUniform4f(param->location, value.x, value.y, value.z, value.w);
  return true;
}

bool ldkMaterialBind(LDKHMaterial handle)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (material == NULL)
    return false;

  if (! ldkShaderBind(material->program))
    return false;


  for (uint32 i = 0; i < material->numParam; i++)
  {
    LDKMaterialParam* param = &material->param[i];
    switch(param->type)
    {
      case GL_INT:
        ldkMaterialParamSetInt(handle, param->name.str, param->intValue);
        break;

      case GL_FLOAT:
        ldkMaterialParamSetFloat(handle, param->name.str, param->floatValue);
        break;

      case GL_FLOAT_VEC2:
        ldkMaterialParamSetVec2(handle, param->name.str, param->vec2Value);
        break;

      case GL_FLOAT_VEC3:
        ldkMaterialParamSetVec3(handle, param->name.str, param->vec3Value);
        break;
      
      case GL_FLOAT_VEC4:
        ldkMaterialParamSetVec4(handle, param->name.str, param->vec4Value);
        break;
    }
  }

  return true;
}

void ldkMaterialUnbind(LDKHMaterial handle)
{
  ldkShaderUnbind(handle);
}

