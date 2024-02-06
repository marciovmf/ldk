#include "ldk/module/renderer.h"
#include "ldk/entity/camera.h"
#include "ldk/module/graphics.h"
#include "ldk/asset/mesh.h"
#include "ldk/os.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"
#include <string.h>

extern const char* ldkOpenglTypeName(GLenum type);

enum
{
  LDK_SHADER_MAX_PARAM = 16,

  LDK_VERTEX_ATTRIBUTE_POSITION = 0,
  LDK_VERTEX_ATTRIBUTE_NORMAL   = 1,
  LDK_VERTEX_ATTRIBUTE_TANGENT  = 2,
  LDK_VERTEX_ATTRIBUTE_BINORMAL = 3,
  LDK_VERTEX_ATTRIBUTE_TEXCOORD = 4,
};

typedef struct 
{
  GLuint id;
  int32 isProgram;
} LDKGLShader;

typedef struct
{
  GLuint id;
} LDKGLTexture;

#ifndef LDK_MATERIAL_MAX_PARAMS
#define LDK_MATERIAL_MAX_PARAMS 8
#endif

#define LDK_MATERIAL_MAX_TEXTURES LDK_MATERIAL_MAX_PARAMS

typedef struct
{
  GLenum type;
  GLint size;
  LDKSmallStr name;
  GLuint location;
  union
  {
    uint32    textureIndexValue;
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
  LDKHTexture textures[LDK_MATERIAL_MAX_TEXTURES];
  uint32 numParam;
  uint32 numTextures;
}LDKGLMaterial;


#define LDK_GL_ERROR_LOG_SIZE 1024
static struct 
{
  bool initialized;
  LDKHList hlistTexture;
  LDKHList hlistShader;
  LDKHList hlistMaterial;
  char errorBuffer[LDK_GL_ERROR_LOG_SIZE];

  LDKCamera* camera;
} internal = { 0 };

//
// Renderer
//

bool ldkRendererInitialize()
{
  LDK_ASSERT(internal.initialized == false);
  internal.initialized = true;
  bool success = true;
  success &= ldkHListCreate(&internal.hlistTexture, typeid(LDKHTexture), sizeof(LDKGLTexture), 32);
  success &= ldkHListCreate(&internal.hlistShader, typeid(LDKHShader), sizeof(LDKGLShader), 32);
  success &= ldkHListCreate(&internal.hlistMaterial, typeid(LDKHMaterial), sizeof(LDKGLMaterial), 32);
  return success;
}

void ldkRendererTerminate()
{
  if (!internal.initialized)
    return;

  internal.initialized = false;
  ldkHListDestroy(&internal.hlistTexture);
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

bool ldkShaderProgramBind(LDKHShaderProgram handle)
{
  if (handle == 0)
  {
    glUseProgram(0);
    return true;
  }

  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, handle);
  if (!shader || !shader->isProgram)
    return false;

  glUseProgram(shader->id);
  return true;
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
  material->name        = NULL ? (const char*) "unnamed-material" : name;
  material->numParam    = numUniforms;
  material->program     = handle;
  material->numTextures = 0;
  memset(&material->param, 0, (sizeof(LDKMaterialParam) * LDK_MATERIAL_MAX_PARAMS));
  memset(&material->textures, 0, (sizeof(LDKHTexture) * LDK_MATERIAL_MAX_TEXTURES));

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
        && param->type != GL_FLOAT_MAT4
        && param->type != GL_SAMPLER_2D
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

bool ldkMaterialParamSetTexture(LDKHMaterial handle, const char* name, LDKHTexture value)
{
  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_SAMPLER_2D);
  if (!param) return false;

  if (material->numTextures >= LDK_MATERIAL_MAX_TEXTURES)
  {
    ldkLogError("Too many textures assigned to material. Maximum is %d", LDK_MATERIAL_MAX_TEXTURES);
    return false;
  }

  uint32 textureId = material->numTextures++;
  param->textureIndexValue = textureId; // this is the texture index in the Material texture array
  material->textures[textureId] = value;
  return true;
}

bool ldkMaterialBind(LDKHMaterial handle)
{
  if ((void*) handle == NULL)
  {
    glUseProgram(0);
    return true;
  }

  LDKGLMaterial* material = (LDKGLMaterial*) ldkHListLookup(&internal.hlistMaterial, handle);
  if (material == NULL)
    return false;

  LDKGLShader* shader = (LDKGLShader*) ldkHListLookup(&internal.hlistShader, material->program);
  if (!shader || !shader->isProgram)
    return false;

  glUseProgram(shader->id);

  //if (! ldkShaderProgramBind(material->program))
  //  return false;

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

      case GL_SAMPLER_2D:
        glActiveTexture(GL_TEXTURE0 + param->textureIndexValue);
        LDKHTexture hTexture = material->textures[param->textureIndexValue];
        LDKGLTexture* texture = (LDKGLTexture*) ldkHListLookup(&internal.hlistTexture, hTexture);
        glBindTexture(GL_TEXTURE_2D, texture->id);
      break;
    }
  }

  // set camera matrices
  GLint uniformView = glGetUniformLocation(shader->id, "mView");
  if (uniformView != -1)
  {
    Mat4 viewMatrix = ldkCameraViewMatrix(internal.camera);
    glUniformMatrix4fv(uniformView, 1, GL_TRUE, (float*) &viewMatrix);
  }

  GLint uniformProj = glGetUniformLocation(shader->id, "mProj");
  if (uniformProj != -1)
  {
    Mat4 projMatrix = ldkCameraProjectMatrix(internal.camera);
    glUniformMatrix4fv(uniformProj, 1, GL_TRUE, (float*) &projMatrix);
  }

  GLint uniformViewProj = glGetUniformLocation(shader->id, "mViewProj");
  if (uniformViewProj != -1)
  {
    Mat4 projViewMatrix = ldkCameraViewProjectMatrix(internal.camera);
    glUniformMatrix4fv(uniformViewProj, 1, GL_TRUE, (float*) &projViewMatrix);
  }

  return true;
}

//
// Texture
//

LDKHTexture ldkTextureCreate(LDKTextureParamMipmap mipmapModeParam, LDKTextureParamWrap wrapParam, LDKTextureParamFilter filterMinParam, LDKTextureParamFilter filterMaxParam)
{
  LDKHTexture handle = ldkHListReserve(&internal.hlistTexture);
  LDKGLTexture* texture = (LDKGLTexture*) ldkHListLookup(&internal.hlistTexture, handle);
  glGenTextures(1, &texture->id);

  glBindTexture(GL_TEXTURE_2D, texture->id);

  // Mipmap generation
  LDK_ASSERT(mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_NEAREST
      || mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_LINEAR
      || mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_NONE);

  if (mipmapModeParam != LDK_TEXTURE_MIPMAP_MODE_NONE)
    glGenerateMipmap(GL_TEXTURE_2D);


  // Wrapping 
  LDK_ASSERT(wrapParam == LDK_TEXTURE_WRAP_CLAMP_TO_EDGE
      || wrapParam == LDK_TEXTURE_WRAP_CLAMP_TO_BORDER
      || wrapParam == LDK_TEXTURE_WRAP_MIRRORED_REPEAT
      || wrapParam == LDK_TEXTURE_WRAP_REPEAT);

  GLenum flag = 0;
  switch(wrapParam)
  {
    case LDK_TEXTURE_WRAP_CLAMP_TO_EDGE:
      flag = GL_CLAMP_TO_EDGE;
    case LDK_TEXTURE_WRAP_CLAMP_TO_BORDER:
      flag = GL_CLAMP_TO_BORDER; break;
    case LDK_TEXTURE_WRAP_MIRRORED_REPEAT:
      flag = GL_MIRRORED_REPEAT; break;
    case LDK_TEXTURE_WRAP_REPEAT:
      flag = GL_REPEAT; break;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, flag);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, flag);

  // Filter min
  LDK_ASSERT(filterMinParam  == LDK_TEXTURE_FILTER_LINEAR || filterMinParam  == LDK_TEXTURE_FILTER_NEAREST);

  bool mipmap = mipmapModeParam != LDK_TEXTURE_MIPMAP_MODE_NONE;
  bool mipmapModeIsLinear = mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_LINEAR;

  if (mipmap)
  {
    if (mipmapModeIsLinear)
    {
      if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
        flag = GL_LINEAR_MIPMAP_LINEAR;
      else
        flag = GL_LINEAR_MIPMAP_NEAREST;
    }
    else
    {
      if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
        flag = GL_NEAREST_MIPMAP_LINEAR;
      else
        flag = GL_NEAREST_MIPMAP_NEAREST;
    }
  }
  else if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
  {
    flag = GL_LINEAR;
  }
  else
  {
    flag = GL_NEAREST;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flag);

  // Filter max
  LDK_ASSERT(filterMaxParam  == LDK_TEXTURE_FILTER_LINEAR || filterMaxParam  == LDK_TEXTURE_FILTER_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flag);

  glBindTexture(GL_TEXTURE_2D, 0);
  return (LDKHTexture) handle;
}

bool ldkTextureData(LDKHTexture handle, uint32 width, uint32 height, void* data, uint32 bitsPerPixel)
{
  LDKGLTexture *texture = (LDKGLTexture*) ldkHListLookup(&internal.hlistTexture, handle);
  if (texture == NULL)
    return false;

  //TODO(marcio): Handle SRGB here !
  bool srgb = true;

  GLenum textureFormat;
  GLenum textureType;

  if (bitsPerPixel == 32)
  {
    textureFormat = GL_RGBA;
    textureType = GL_UNSIGNED_BYTE;
  }
  else if (bitsPerPixel == 24)
  {
    textureFormat = GL_RGB;
    textureType = GL_UNSIGNED_BYTE;
  }
  else if (bitsPerPixel == 16)
  {
    textureFormat = GL_RGB;
    textureType = GL_UNSIGNED_SHORT_5_6_5;
  }
  else
  {
    ldkLogError("Images must have either 16, 24 or 32 bits per pixel. Colors might be wrong.");
    textureFormat = GL_RGBA;
    textureType = GL_UNSIGNED_BYTE;
  }

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB_ALPHA : GL_RGBA, width, height, 0, textureFormat, textureType, data);

  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

bool ldkTextureDestroy(LDKHTexture handle)
{
  LDKGLTexture* texture = (LDKGLTexture*) ldkHListLookup(&internal.hlistTexture, handle);
  if (texture == NULL)
    return false;

  glDeleteTextures(1, &texture->id);
  return true;
}

//
// VertexBuffer
//

typedef struct
{
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  size_t vertexBufferSizeBytes;
  size_t indexBufferSizeBytes;
  LDKVertexLayout layout;
} LDKVertexBufferInfo;

LDKVertexLayout ldkVertexBufferLayoutGet(LDKVertexBuffer buffer)
{
  LDKVertexBufferInfo* b = (LDKVertexBufferInfo*) buffer;
  return b->layout;
}

LDKVertexBuffer ldkVertexBufferCreate(LDKVertexLayout layout)
{
  LDKVertexBufferInfo* buffer = (LDKVertexBufferInfo*) ldkOsMemoryAlloc(sizeof(LDKVertexBufferInfo));
  memset(buffer, 0, sizeof(LDKVertexBufferInfo));
  buffer->layout = layout;

  // VAO
  glGenVertexArrays(1, &buffer->vao);
  glBindVertexArray(buffer->vao);

  // IBO
  glGenBuffers(1, &buffer->ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, (void*) NULL, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // VBO
  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  switch (layout)
  {
    case LDK_VERTEX_LAYOUT_PNU:
      {
        const GLsizei stride = 8 * sizeof(float);
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_POSITION, 3, GL_FLOAT, GL_FALSE, stride, (const void*) 0);
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_NORMAL,   3, GL_FLOAT, GL_FALSE, stride, (const void*) (3 * sizeof(float)));
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, stride, (const void*) (6 * sizeof(float)));
      }
      break;

    case LDK_VERTEX_LAYOUT_PNTBU:
      {
        const GLsizei stride = 14 * sizeof(float);
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_POSITION,  3, GL_FLOAT, GL_FALSE, stride, (const void*) 0);
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_NORMAL,    3, GL_FLOAT, GL_FALSE, stride, (const void*) (3 * sizeof(float)));
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_TANGENT,   3, GL_FLOAT, GL_FALSE, stride, (const void*) (6 * sizeof(float)));
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_BINORMAL,  3, GL_FLOAT, GL_FALSE, stride, (const void*) (9 * sizeof(float)));
        glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE_TEXCOORD,  2, GL_FLOAT, GL_FALSE, stride, (const void*) (12 * sizeof(float)));

        glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_POSITION);
        glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_NORMAL);
        glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TANGENT);
        glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_BINORMAL);
        glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TEXCOORD);
      }
      break;

    default:
      ldkLogError("Unsuported Buffer format %d", (int) layout);
      break;
  }

  glBufferData(GL_ARRAY_BUFFER, 0, (void*) NULL, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindVertexArray(0);

  glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_POSITION);
  glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_NORMAL);
  glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TANGENT);
  glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_BINORMAL);
  glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TEXCOORD);

  return buffer;
}

void ldkVertexBufferBind(LDKVertexBuffer buffer)
{
  LDKVertexBufferInfo* b = (LDKVertexBufferInfo*) buffer;

  if (b == NULL)
  {
    glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_POSITION);
    glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_NORMAL);
    glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TANGENT);
    glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_BINORMAL);
    glDisableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TEXCOORD);
    return;
  }

  GLuint vao = b->vao;
  GLuint vbo = b->vbo;
  GLuint ibo = b->ibo;

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

  if (b->layout == LDK_VERTEX_LAYOUT_PNU)
  {
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_POSITION);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_NORMAL);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TEXCOORD);
  }
  else if (b->layout == LDK_VERTEX_LAYOUT_PNTBU)
  {
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_POSITION);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_NORMAL);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TANGENT);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_BINORMAL);
    glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE_TEXCOORD);
  }
  else
  {
    LDK_ASSERT(false);
  }
}

void ldkVertexBufferDestroy(LDKVertexBuffer vertexBuffer)
{
  LDKVertexBufferInfo* buffer = (LDKVertexBufferInfo*) vertexBuffer;
  glDeleteVertexArrays(1, &buffer->vao);
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteBuffers(1, &buffer->ibo);
  ldkOsMemoryFree(buffer);
}

void ldkVertexBufferData(LDKVertexBuffer buffer, void* data, size_t size)
{
  ldkVertexBufferSubData(buffer, 0, data, size);
}

void ldkVertexBufferSubData(LDKVertexBuffer buffer, size_t offset, void* data, size_t size)
{
  LDKVertexBufferInfo* b = (LDKVertexBufferInfo*) buffer;
  glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
  size_t totalSize = offset + size;

  if (totalSize > b->vertexBufferSizeBytes)
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  else
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

  b->vertexBufferSizeBytes = totalSize;
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkVertexBufferIndexData(LDKVertexBuffer buffer, void* data, size_t size)
{
  ldkVertexBufferIndexSubData(buffer, 0, data, size);
}

void ldkVertexBufferIndexSubData(LDKVertexBuffer buffer, size_t offset, void* data, size_t size)
{
  LDKVertexBufferInfo* b = (LDKVertexBufferInfo*) buffer;
  size_t totalSize = offset + size;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b->ibo);

  if (totalSize > b->indexBufferSizeBytes)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  else
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);

  b->indexBufferSizeBytes = totalSize;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

//
// Rendering
//

void ldkRenderMesh(LDKHMesh hMesh, uint32 count, size_t start)
{
    LDKMesh* mesh = (LDKMesh*) hMesh;
    LDKVertexBuffer vBuffer = ldkAssetMeshGetVertexBuffer(hMesh);
    ldkVertexBufferBind(vBuffer);

    for(uint32 i = 0; i < mesh->numSurfaces; i++)
    {
      LDKSurface* surface = &mesh->surfaces[i];
      LDKHMaterial material = mesh->materials[surface->materialIndex];
      ldkMaterialBind(material);
      glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
    }
}

void ldkRendererCameraSet(LDKCamera* camera)
{
  internal.camera = camera;
}
