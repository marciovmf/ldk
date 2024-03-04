#include "ldk/module/renderer.h"
#include "ldk/module/graphics.h"
#include "ldk/module/asset.h"
#include "ldk/entity/camera.h"
#include "ldk/entity/staticobject.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/texture.h"
#include "ldk/asset/material.h"
#include "ldk/asset/config.h"
#include "ldk/asset/shader.h"
#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"
#include <string.h>

extern const char* ldkOpenglTypeName(GLenum type);

typedef struct 
{
  float objectIndex;
  float surfaceIndex;
  float triangleIndex;
} LDKPickingPixelInfo;

typedef enum
{
  LDK_RENDER_STATIC_OBJECTS,
  LDK_RENDER_PHASE_PICKING,
} RenderPhase;

#define LDK_GL_ERROR_LOG_SIZE 1024
static struct 
{
  bool initialized;
  LDKConfig* config;
  LDKRGB clearColor;
  char errorBuffer[LDK_GL_ERROR_LOG_SIZE];
  LDKArena bucketROStaticMesh;
  LDKCamera* camera;
  LDKMaterial* fixedMaterial;
  float elapsedTime;

  // Picking
  GLuint fboPicking;
  GLuint texturePicking;
  GLuint textureDepthPicking;
  LDKHandle materialPickingHandle;

  LDKHandle materialHighlightHandle;

  Vec3 higlightColor1;
  Vec3 higlightColor2;

  LDKHandle selectedEntity;
  uint32 selectedEntityIndex;
} internal = { 0 };

typedef enum
{
  LDK_RENDER_OBJECT_STATIC_OBJECT
}  LDKRenderObjectType;

typedef struct
{
  LDKRenderObjectType type; 
  union
  {
    LDKStaticObject* staticMesh;
  };

} LDKRenderObject;


//
// Renderer
//

bool internalPickingFBOCreate(uint32 width, uint32 height)
{
  if (internal.fboPicking != 0)
    return false;

  bool success = false;
  // Create the FBO
  glGenFramebuffers(1, &internal.fboPicking);
  glBindFramebuffer(GL_FRAMEBUFFER, internal.fboPicking);

  // Create the texture object for the primitive information buffer
  glGenTextures(1, &internal.texturePicking);
  glBindTexture(GL_TEXTURE_2D, internal.texturePicking);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height,
      0, GL_RGB, GL_FLOAT, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      internal.texturePicking, 0);

  // Create the texture object for the depth buffer
  glGenTextures(1, &internal.textureDepthPicking);
  glBindTexture(GL_TEXTURE_2D, internal.textureDepthPicking);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height,
      0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
      internal.textureDepthPicking, 0);

  // Disable reading to avoid problems with older GPUs
  glReadBuffer(GL_NONE);

  glDrawBuffer(GL_COLOR_ATTACHMENT0);

  // Verify that the FBO is correct
  GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (Status != GL_FRAMEBUFFER_COMPLETE) {
    printf("FB error, status: 0x%x\n", Status);
    success = false;
  }

  // Loads the picking shader
  internal.materialPickingHandle = ldkAssetGet(LDKMaterial, "assets/editor/picking.material")->asset.handle;
  internal.materialHighlightHandle = ldkAssetGet(LDKMaterial, "assets/editor/highlight.material")->asset.handle;

  internal.higlightColor1 = ldkConfigGetVec3(internal.config, "editor.highlight-color1");
  internal.higlightColor2 = ldkConfigGetVec3(internal.config, "editor.highlight-color2");

  // Restore the default framebuffer
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return success;
}

void internalPickingFBODEstroy()
{
  // the display can be resized befor we get initialized...
  if (!internal.initialized)
    return;

  glDeleteTextures(1, &internal.texturePicking);
  glDeleteTextures(1, &internal.textureDepthPicking);
  glDeleteFramebuffers(1, &internal.fboPicking);
  internal.fboPicking = 0;
  internal.texturePicking = 0;
  internal.textureDepthPicking = 0;
}

bool ldkRendererInitialize(LDKConfig* config)
{
  LDK_ASSERT(internal.initialized == false);
  internal.initialized = true;
  bool success = true;
  internal.clearColor = (LDKRGB){0, 100, 120};
  internal.config = config;
  success &= ldkArenaCreate(&internal.bucketROStaticMesh, 64 * sizeof(LDKRenderObject));

  //
  // Picking Framebuffer setup
  //
  LDKSize viewport = ldkGraphicsViewportSizeGet();
  internalPickingFBOCreate(viewport.width, viewport.height);
  return success;
}

void ldkRendererResize(uint32 width, uint32 height)
{
  glViewport(0, 0, width, height);
  internalPickingFBODEstroy();
  internalPickingFBOCreate(width, height);
}

void ldkRendererTerminate(void)
{
  if (!internal.initialized)
    return;

  internal.initialized = false;
  ldkArenaDestroy(&internal.bucketROStaticMesh);
}


//
// Shader
//

static inline GLuint internalShaderSourceCompile(GLuint type, const char* source)
{
  const char* version = "#version 330 core\n"
    "#define LDK_VERTEX_ATTRIBUTE_POSITION    layout (location = 0) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_NORMAL      layout (location = 1) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_TANGENT     layout (location = 2) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_BINORMAL    layout (location = 3) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_TEXCOORD0   layout (location = 4) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_TEXCOORD1   layout (location = 5) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_CUSTOM0     layout (location = 6) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_CUSTOM1     layout (location = 7) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_MATRIX0     layout (location = 8) in\n"
    "#define LDK_VERTEX_ATTRIBUTE_MATRIX1     layout (location = 12) in\n"
    "#define LDK_VERTEX_ATTRIBUTE0            layout (location = 0) in\n"
    "#define LDK_VERTEX_ATTRIBUTE1            layout (location = 1) in\n"
    "#define LDK_VERTEX_ATTRIBUTE2            layout (location = 2) in\n"
    "#define LDK_VERTEX_ATTRIBUTE3            layout (location = 3) in\n"
    "#define LDK_VERTEX_ATTRIBUTE4            layout (location = 4) in\n"
    "#define LDK_VERTEX_ATTRIBUTE5            layout (location = 5) in\n"
    "#define LDK_VERTEX_ATTRIBUTE6            layout (location = 6) in\n"
    "#define LDK_VERTEX_ATTRIBUTE7            layout (location = 7) in\n"
    "#define LDK_VERTEX_ATTRIBUTE8            layout (location = 8) in\n"
    "#define LDK_VERTEX_ATTRIBUTE9            layout (location = 9) in\n"
    "#define LDK_VERTEX_ATTRIBUTE10           layout (location = 10) in\n"
    "#define LDK_VERTEX_ATTRIBUTE11           layout (location = 11) in\n"
    "#define LDK_VERTEX_ATTRIBUTE12           layout (location = 12) in\n"
    "#define LDK_VERTEX_ATTRIBUTE13           layout (location = 13) in\n"
    "#define LDK_VERTEX_ATTRIBUTE14           layout (location = 14) in\n"
    "#define LDK_VERTEX_ATTRIBUTE15           layout (location = 15) in\n";
  const char* macro;
  const char* typeName;
  if (type == GL_VERTEX_SHADER)
  {
    typeName = "VERTEX SHADER";
    macro = "#define LDK_COMPILE_VETEX_SHADER\n";
  }
  else if (type == GL_FRAGMENT_SHADER)
  {
    typeName = "FRAGMENT SHADER";
    macro = "#define LDK_COMPILE_FRAGMENT_SHADER\n";
  }
  else if (type == GL_GEOMETRY_SHADER)
  {
    typeName = "GEOMETRY SHADER";
    macro = "#define LDK_COMPILE_GEOMETRY_SHADER\n";
  }
  else
  {
    macro = "";
    typeName = "UNKNOW SHADER TYPE";
  }

  GLint status;
  GLsizei errorBufferLen = 0;
  GLuint shader = glCreateShader(type);
  const char* sources[] = {version, macro, source};
  glShaderSource(shader, 3, sources, 0);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (! status)
  {
    glGetShaderInfoLog(shader, LDK_GL_ERROR_LOG_SIZE, &errorBufferLen, internal.errorBuffer);
    ldkLogError("Compiling %s: %s\n", typeName, internal.errorBuffer);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

bool ldkShaderProgramCreate(const char* vs, const char* fs, const char* gs, LDKShader* out)
{
  GLuint vsId = 0;
  GLuint fsId = 0;
  GLuint gsId = 0;
  GLuint program = glCreateProgram();
  out->gl.id = program;

  if (vs)
  {
    vsId = internalShaderSourceCompile(GL_VERTEX_SHADER, vs);
    glAttachShader(program, vsId);
  }

  if(fs)
  {
    fsId = internalShaderSourceCompile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(program, fsId);
  }

  if (gs)
  {
    gsId = internalShaderSourceCompile(GL_GEOMETRY_SHADER, gs);
    glAttachShader(program, gsId);
  }

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

void ldkShaderDestroy(LDKShader* shader)
{
  if (!shader)
    return;

  glDeleteProgram(shader->gl.id);
  shader->gl.id = 0;
}

static LDKMaterialParam* internalMaterialParamGet(LDKMaterial* material, const char* name, GLenum type)
{
  for (uint32 i = 0; i < material->numParam; i++)
  {
    if (material->param[i].gl.type == type && strncmp(name, (const char*) &material->param[i].name, material->param[i].name.length) == 0)
      return &material->param[i];
  }
  ldkLogError("Material '%s' references unknown param '%s %s'", material->asset.path, ldkOpenglTypeName(type), name);
  return NULL;
}

bool ldkFragmentShaderCreate(const char* source, LDKShader* shaderAsset)
{
  shaderAsset->gl.id = internalShaderSourceCompile(GL_FRAGMENT_SHADER, source);
  bool success = shaderAsset->gl.id != 0;
  return success;
}

bool ldkVertexShaderCreate(const char* source, LDKShader* shaderAsset)
{
  shaderAsset->gl.id = internalShaderSourceCompile(GL_VERTEX_SHADER, source);
  bool success = shaderAsset->gl.id != 0;
  return success;
}

bool ldkGeometryShaderCreate(const char* source, LDKShader* shaderAsset)
{
  shaderAsset->gl.id = internalShaderSourceCompile(GL_GEOMETRY_SHADER, source);
  bool success = shaderAsset->gl.id != 0;
  return success;
}

bool ldkShaderProgramBind(LDKShader* shaderAsset)
{
  if (!shaderAsset)
  {
    glUseProgram(0);
    return true;
  }

  if (!shaderAsset)
    return false;

  glUseProgram(shaderAsset->gl.id);
  return true;
}


//
// Material
//

bool ldkMaterialCreate(LDKShader* program, LDKMaterial* out)
{
  if (!program)
    return false;

  int32 numUniforms = 0;
  glGetProgramiv(program->gl.id, GL_ACTIVE_UNIFORMS, &numUniforms);
  if (numUniforms > LDK_MATERIAL_MAX_PARAMS)
  {
    ldkLogError("Shader %s declares too many uniforms (%d). MAximum is %d.", program->asset.path, numUniforms, LDK_MATERIAL_MAX_PARAMS);
    return false;
  }

  int32 maxUniformNameLength = 0;
  glGetProgramiv(program->gl.id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);
  if (numUniforms > LDK_MATERIAL_MAX_PARAMS)
  {
    ldkLogError("Shader %s declares at least one uniform names longer than (%d)", program->asset.path, LDK_SMALL_STRING_MAX_LENGTH);
    return LDK_HANDLE_INVALID;
  }

  out->name        = (const char*) "unnamed-material";
  out->numParam    = numUniforms;
  out->program     = program->asset.handle;
  out->numTextures = 0;
  memset(&out->param, 0, (sizeof(LDKMaterialParam) * LDK_MATERIAL_MAX_PARAMS));
  memset(&out->textures, 0, (sizeof(LDKHandle) * LDK_MATERIAL_MAX_TEXTURES));

  for (int i = 0; i < numUniforms; i ++)
  {
    LDKMaterialParam* param = &out->param[i];
    glGetActiveUniform(program->gl.id, i,
        LDK_SMALL_STRING_MAX_LENGTH,
        (GLsizei*) &param->name.length,
        &param->gl.size,
        &param->gl.type,
        (char*) &out->param[i].name.str);

    if ( param->gl.type != GL_INT
        && param->gl.type != GL_UNSIGNED_INT
        && param->gl.type != GL_FLOAT
        && param->gl.type != GL_FLOAT_VEC2
        && param->gl.type != GL_FLOAT_VEC3
        && param->gl.type != GL_FLOAT_VEC4
        && param->gl.type != GL_FLOAT_MAT4
        && param->gl.type != GL_SAMPLER_2D
       )
    {
      ldkLogWarning("Material '%s' references param with unsupported type '%s %s'",
          out->asset.path,
          ldkOpenglTypeName(param->gl.type),
          (const char*) &out->param[i].name);
      continue;
    }
    out->param[i].gl.location = glGetUniformLocation(program->gl.id, (const char*) &out->param[i].name);
  }

  return true;
}

void ldkMaterialDestroy(LDKMaterial* material)
{
  ldkAssetDispose(material);
}

bool ldkMaterialParamSetInt(LDKMaterial* material, const char* name, int value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_INT);
  if (!param) return false;

  param->intValue = value;
  glUniform1i(param->gl.location, value);
  return true;
}


bool ldkMaterialParamSetUInt(LDKMaterial* material, const char* name, uint32 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_UNSIGNED_INT);
  if (!param) return false;

  param->uintValue = value;
  glUniform1ui(param->gl.location, value);
  return true;
}

bool ldkMaterialParamSetFloat(LDKMaterial* material, const char* name, float value)
{
  if (!material) return false;

  LDKMaterialParam* param =  internalMaterialParamGet(material, name, GL_FLOAT);
  if (!param) return false;

  param->floatValue = value;
  glUniform1f(param->gl.location, value);
  return true;
}

bool ldkMaterialParamSetVec2(LDKMaterial* material, const char* name, Vec2 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC2);
  if (!param) return false;

  param->vec2Value = value;
  glUniform2f(param->gl.location, value.x, value.y);
  return true;
}

bool ldkMaterialParamSetVec3(LDKMaterial* material, const char* name, Vec3 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC3);
  if (!param) return false;

  param->vec3Value = value;
  glUniform3f(param->gl.location, value.x, value.y, value.z);
  return true;
}

bool ldkMaterialParamSetVec4(LDKMaterial* material, const char* name, Vec4 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_VEC4);
  if (!param) return false;

  param->vec4Value = value;
  glUniform4f(param->gl.location, value.x, value.y, value.z, value.w);
  return true;
}

bool ldkMaterialParamSetMat4(LDKMaterial* material, const char* name, Mat4 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, GL_FLOAT_MAT4);
  if (!param) return false;

  param->mat4Value = value;
  glUniformMatrix4fv(param->gl.location, 1, GL_TRUE, (float*)&value);
  return true;
}

bool ldkMaterialParamSetTexture(LDKMaterial* material, const char* name, LDKTexture* value)
{
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
  material->textures[textureId] = value->asset.handle;
  return true;
}

bool ldkMaterialBind(LDKMaterial* material)
{
  if (!material)
  {
    glUseProgram(0);
    return true;
  }

  if (material == NULL)
    return false;

  LDKShader* shader = ldkAssetLookup(LDKShader,  material->program);

  if (!shader)
    return false;

  glUseProgram(shader->gl.id);

  for (uint32 i = 0; i < material->numParam; i++)
  {
    LDKMaterialParam* param = &material->param[i];
    switch(param->gl.type)
    {
      case GL_INT:
        ldkMaterialParamSetInt(material, param->name.str, param->intValue);
        break;

      case GL_UNSIGNED_INT:
        ldkMaterialParamSetUInt(material, param->name.str, param->intValue);
        break;

      case GL_FLOAT:
        ldkMaterialParamSetFloat(material, param->name.str, param->floatValue);
        break;

      case GL_FLOAT_VEC2:
        ldkMaterialParamSetVec2(material, param->name.str, param->vec2Value);
        break;

      case GL_FLOAT_VEC3:
        ldkMaterialParamSetVec3(material, param->name.str, param->vec3Value);
        break;

      case GL_FLOAT_VEC4:
        ldkMaterialParamSetVec4(material, param->name.str, param->vec4Value);
        break;

      case GL_SAMPLER_2D:
        glActiveTexture(GL_TEXTURE0 + param->textureIndexValue);
        LDKHandle hTexture = material->textures[param->textureIndexValue];
        LDKTexture* texture = ldkAssetLookup(LDKTexture, hTexture);
        glBindTexture(GL_TEXTURE_2D, texture->gl.id);
        break;
    }
  }

  // set camera matrices
  GLint uniformView = glGetUniformLocation(shader->gl.id, "mView");
  if (uniformView != -1)
  {
    Mat4 viewMatrix = ldkCameraViewMatrix(internal.camera);
    glUniformMatrix4fv(uniformView, 1, GL_TRUE, (float*) &viewMatrix);
  }

  GLint uniformProj = glGetUniformLocation(shader->gl.id, "mProj");
  if (uniformProj != -1)
  {
    Mat4 projMatrix = ldkCameraProjectMatrix(internal.camera);
    glUniformMatrix4fv(uniformProj, 1, GL_TRUE, (float*) &projMatrix);
  }

  GLint uniformViewProj = glGetUniformLocation(shader->gl.id, "mViewProj");
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

bool ldkTextureCreate(LDKTextureParamMipmap mipmapModeParam, LDKTextureParamWrap wrapParam, LDKTextureParamFilter filterMinParam, LDKTextureParamFilter filterMaxParam, LDKTexture* out)
{
  GLenum flagWrap = 0;
  GLenum flagMin = 0;
  GLenum flagMax = 0;
  bool useMipmap = mipmapModeParam != LDK_TEXTURE_MIPMAP_MODE_NONE;
  bool mipmapModeIsLinear = mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_LINEAR;

  // Mipmap generation
  LDK_ASSERT(mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_NEAREST
      || mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_LINEAR
      || mipmapModeParam == LDK_TEXTURE_MIPMAP_MODE_NONE);


  // Wrapping 
  LDK_ASSERT(wrapParam == LDK_TEXTURE_WRAP_CLAMP_TO_EDGE
      || wrapParam == LDK_TEXTURE_WRAP_CLAMP_TO_BORDER
      || wrapParam == LDK_TEXTURE_WRAP_MIRRORED_REPEAT
      || wrapParam == LDK_TEXTURE_WRAP_REPEAT);

  switch(wrapParam)
  {
    case LDK_TEXTURE_WRAP_CLAMP_TO_EDGE:
      flagWrap = GL_CLAMP_TO_EDGE;
    case LDK_TEXTURE_WRAP_CLAMP_TO_BORDER:
      flagWrap = GL_CLAMP_TO_BORDER; break;
    case LDK_TEXTURE_WRAP_MIRRORED_REPEAT:
      flagWrap = GL_MIRRORED_REPEAT; break;
    case LDK_TEXTURE_WRAP_REPEAT:
      flagWrap = GL_REPEAT; break;
  }

  // Filter min and mag
  LDK_ASSERT(filterMinParam  == LDK_TEXTURE_FILTER_LINEAR || filterMinParam  == LDK_TEXTURE_FILTER_NEAREST);
  LDK_ASSERT(filterMaxParam  == LDK_TEXTURE_FILTER_LINEAR || filterMaxParam  == LDK_TEXTURE_FILTER_NEAREST);

  if (useMipmap)
  {
    if (mipmapModeIsLinear)
    {
      if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
        flagMin = GL_LINEAR_MIPMAP_LINEAR;
      else
        flagMin = GL_LINEAR_MIPMAP_NEAREST;


      if (filterMaxParam == LDK_TEXTURE_FILTER_LINEAR)
        flagMax = GL_LINEAR_MIPMAP_LINEAR;
      else
        flagMax = GL_LINEAR_MIPMAP_NEAREST;

    }
    else
    {
      if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
        flagMin = GL_NEAREST_MIPMAP_LINEAR;
      else
        flagMin = GL_NEAREST_MIPMAP_NEAREST;


      if (filterMaxParam == LDK_TEXTURE_FILTER_LINEAR)
        flagMax = GL_NEAREST_MIPMAP_LINEAR;
      else
        flagMax = GL_NEAREST_MIPMAP_NEAREST;
    }
  }
  else
  {
    if (filterMinParam == LDK_TEXTURE_FILTER_LINEAR)
    {
      flagMin = GL_LINEAR;
    }
    else
    {
      flagMin = GL_NEAREST;
    }

    if (filterMaxParam == LDK_TEXTURE_FILTER_LINEAR)
    {
      flagMax = GL_LINEAR;
    }
    else
    {
      flagMax = GL_NEAREST;
    }
  }

  out->useMipmap = useMipmap;

  glGenTextures(1, &out->gl.id);
  glBindTexture(GL_TEXTURE_2D, out->gl.id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, flagWrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, flagWrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flagMin);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flagMax);

  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

bool ldkTextureData(LDKTexture* texture, uint32 width, uint32 height, void* data, uint32 bitsPerPixel)
{
  if (!texture)
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

  glBindTexture(GL_TEXTURE_2D, texture->gl.id);
  glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB_ALPHA : GL_RGBA, width, height, 0, textureFormat, textureType, data);

  if (texture->useMipmap)
    glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

void ldkTextureDestroy(LDKTexture* texture)
{
  if (!texture)
    return;

  glDeleteTextures(1, &texture->gl.id);
  texture->gl.id = 0;
  texture->useMipmap = false;
  return;
}

//
// VertexBuffer
//

struct LDKRenderBuffer_t
{
    GLuint  vao;
    GLuint* vbo;
    size_t* vboSize;
    bool*   vboIsStream;
    GLuint  ibo;
    size_t  iboSize;
    bool    iboIsStream;
    uint32  numIndices;
    uint32  numVBOs;
    bool    hasInstancedAttributes;
};

LDKRenderBuffer* ldkRenderBufferCreate(int numVBOs)
{
    // We allocate one chunck of memory for everything we need and split this chunk as we need.
    const size_t idListSize  = sizeof(GLuint) * numVBOs;
    const size_t sizeListSize = sizeof(size_t) * numVBOs;
    const size_t hintListSize = sizeof(bool) * numVBOs;
    const size_t totalSize = sizeof(LDKRenderBuffer) + idListSize + sizeListSize + hintListSize;

    char* memory = (char*) ldkOsMemoryAlloc(totalSize);
    memset(memory, 0, totalSize);

    LDKRenderBuffer* rb = (LDKRenderBuffer*) memory;
    rb->numVBOs = numVBOs;
    memory += sizeof(LDKRenderBuffer);

    rb->vbo           = (GLuint*) (memory + sizeof(LDKRenderBuffer));
    rb->vboSize       = (size_t*) (memory + sizeof(LDKRenderBuffer) + idListSize);
    rb->vboIsStream   = (bool*  ) (memory + sizeof(LDKRenderBuffer) + idListSize + sizeListSize);

    // Generate and bind VAO
    glGenVertexArrays(1, &rb->vao);
    glBindVertexArray(rb->vao);

    rb->vbo = (GLuint*) memory;
    rb->vboSize = (size_t*) (memory + idListSize);
    rb->vboIsStream = (bool*) (memory + idListSize + sizeListSize);
    glGenBuffers(numVBOs, rb->vbo);

    return rb;
}

void ldkRenderBufferDestroy(LDKRenderBuffer* rb)
{
  glDeleteVertexArrays(1, &rb->vao);
  glDeleteBuffers(rb->numVBOs, rb->vbo);
  glDeleteBuffers(1, &rb->ibo);
  ldkOsMemoryFree(rb);
}

void ldkRenderBufferBind(const LDKRenderBuffer* rb)
{
  if (rb == NULL)
  {
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return;
  }

  glBindVertexArray(rb->vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rb->ibo);
}

static void internalVertexBufferSetAttributeGLType(GLenum glType, const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  LDK_ASSERT(count > 0 && count <= 4);
  LDK_ASSERT(semantic >= 0 && semantic <= LDK_VERTEX_ATTRIBUTE_MAX);
  GLboolean normalize = (flag & LDK_VERTEX_ATTRIB_NORMALIZE);
  uint32 divisor = (flag & LDK_VERTEX_ATTRIB_INSTANCE) > 0 ? 1 : 0;
  GLuint location = semantic;
  glBindVertexArray(rb->vao);
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);
  glVertexAttribPointer(location, count, glType, normalize, stride, offset);
  glEnableVertexAttribArray(location);
  glVertexAttribDivisor(location, divisor);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkVertexBufferSetAttributeMat4(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  LDK_ASSERT(count > 0 && count <= 4);
  LDK_ASSERT(semantic >= 0 && semantic <= LDK_VERTEX_ATTRIBUTE_MAX);
  GLboolean normalize = (flag & LDK_VERTEX_ATTRIB_NORMALIZE);
  uint32 divisor = (flag & LDK_VERTEX_ATTRIB_INSTANCE) > 0 ? 1 : 0;
  GLuint location = semantic;
  glBindVertexArray(rb->vao);
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);

  glVertexAttribPointer(location + 0, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 0 * sizeof(float) * 4));
  glVertexAttribPointer(location + 1, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 1 * sizeof(float) * 4));
  glVertexAttribPointer(location + 2, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 2 * sizeof(float) * 4));
  glVertexAttribPointer(location + 3, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 4 * sizeof(float) * 4));

  glEnableVertexAttribArray(location + 0);
  glEnableVertexAttribArray(location + 1);
  glEnableVertexAttribArray(location + 2);
  glEnableVertexAttribArray(location + 3);

  glVertexAttribDivisor(location + 0, divisor);
  glVertexAttribDivisor(location + 1, divisor);
  glVertexAttribDivisor(location + 2, divisor);
  glVertexAttribDivisor(location + 3, divisor);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkVertexBufferSetAttributeFloat(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_FLOAT, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeInt(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_INT, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeUInt(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_UNSIGNED_INT, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeByte(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_BYTE, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeUByte(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_UNSIGNED_BYTE, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeDouble(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_DOUBLE, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeShort(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_SHORT, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetAttributeUShort(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferSetAttributeGLType(GL_UNSIGNED_SHORT, rb, index, semantic, count, stride, offset, flag);
}

void ldkVertexBufferSetData(const LDKRenderBuffer* rb, int index, size_t size, const void *data, bool stream)
{
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);

  // Check if the buffer needs to be recreated with a larger size
  size_t dataSize = rb->vboSize[index];
  if (size > dataSize)
  {
    glBufferData(GL_ARRAY_BUFFER, size, data, stream ? GL_STREAM_DRAW : GL_STATIC_DRAW);
    rb->vboSize[index] = size;
    rb->vboIsStream[index] = stream;
  }
  else
  {
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkVertexBufferSetSubData(const LDKRenderBuffer* rb, int32 index, int32 offset, size_t size, const void *data)
{
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);

  // Check if the buffer needs to be recreated with a larger size
  size_t dataSize = rb->vboSize[index];
  if (size + offset > dataSize)
  {
    bool isForStreaming = rb->vboIsStream[index];
    glBufferData(GL_ARRAY_BUFFER, size + offset, NULL, isForStreaming ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size + offset, data);
    rb->vboSize[index] = size + offset;
  }
  else
  {
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkIndexBufferSetData(LDKRenderBuffer* rb, size_t size, const void *data, bool stream)
{
  if (rb->ibo == 0)
  {
    glGenBuffers(1, &rb->ibo);
    rb->iboSize = 0;
    rb->iboIsStream = stream;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rb->ibo);

  if (size > rb->iboSize)
  {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, stream ? GL_STREAM_DRAW : GL_STATIC_DRAW);
    rb->iboSize = size;
  }
  else
  {
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, data);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ldkIndexBufferSetSubData(LDKRenderBuffer* rb, int32 offset, size_t size, const void *data)
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rb->ibo);

  // Check if the buffer needs to be recreated with a larger size
  size_t dataSize = rb->vboSize[rb->numVBOs];
  if (size + offset > dataSize)
  {
    bool isForStreaming = rb->iboIsStream;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size + offset, NULL, isForStreaming ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size + offset, data);
    rb->vboSize[rb->numVBOs] = size + offset;
  }
  else
  {
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


//
// Rendering
//

void ldkRendererClearColor(LDKRGB color)
{
  internal.clearColor = color;
}

void ldkRendererCameraSet(LDKCamera* camera)
{
  internal.camera = camera;
}

void ldkRendererAddStaticObject(LDKStaticObject* entity)
{
  LDKRenderObject* ro = (LDKRenderObject*) ldkArenaAllocate(&internal.bucketROStaticMesh, sizeof(LDKRenderObject));
  ro->type = LDK_RENDER_OBJECT_STATIC_OBJECT;
  ro->staticMesh = entity;
}

void internalRenderMesh(LDKStaticObject* entity)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);

  if(!mesh)
    return;

  ldkRenderBufferBind(mesh->vBuffer);

  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);
    LDKMaterial* material = NULL;

    // if rendering with picking material, we set the surface index
    if (internal.fixedMaterial) 
    {
      if (internal.fixedMaterial->asset.handle == internal.materialPickingHandle)
        ldkMaterialParamSetUInt(internal.fixedMaterial, "surfaceIndex", i);

      material = internal.fixedMaterial;
    }
    else
    {
      LDKHandle hMaterial = mesh->materials[surface->materialIndex];
      material = ldkAssetLookup(LDKMaterial, hMaterial);
      ldkMaterialBind(material);
    }

    ldkMaterialParamSetMat4(material, "mModel", world);
    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  ldkRenderBufferBind(NULL);
}

void ldkRendererRender(float deltaTime)
{
  internal.elapsedTime += deltaTime;

  uint32 count = (uint32) ldkArenaUsedGet(&internal.bucketROStaticMesh) / sizeof(LDKRenderObject);
  LDKRenderObject* ro = (LDKRenderObject*) ldkArenaDataGet(&internal.bucketROStaticMesh);
  internal.fixedMaterial = NULL;

  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); 
  glDepthFunc(GL_LEQUAL);
  glClearColor(internal.clearColor.r / 255.0f, internal.clearColor.g / 255.0f, internal.clearColor.b / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for(uint32 i = 0; i < count; i++)
  {
    LDK_ASSERT(ro->type == LDK_RENDER_OBJECT_STATIC_OBJECT);
    internalRenderMesh(ro->staticMesh);
    ro++;
  }

#if 1

  //
  // Picking
  //
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, internal.fboPicking);
  internal.fixedMaterial = ldkAssetLookup(LDKMaterial, internal.materialPickingHandle);
  ldkMaterialBind(internal.fixedMaterial);

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ro = (LDKRenderObject*) ldkArenaDataGet(&internal.bucketROStaticMesh);
  for(uint32 i = 0; i < count; i++)
  {
    ldkMaterialParamSetUInt(internal.fixedMaterial, "objectIndex", i + 1);
    internalRenderMesh(ro->staticMesh);
    ro++;
  }

  //
  // Selection highlight
  //

  ro = (LDKRenderObject*) ldkArenaDataGet(&internal.bucketROStaticMesh);
  LDKMouseState mouseState;

  ldkOsMouseStateGet(&mouseState);
  if (ldkOsMouseButtonDown(&mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, internal.fboPicking);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    int x = mouseState.cursor.x;
    int y = ldkGraphicsViewportSizeGet().height - mouseState.cursor.y;

    LDKPickingPixelInfo pixelInfo;

    glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, &pixelInfo);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    uint32 objectIndex = (uint32) pixelInfo.objectIndex;

    if (objectIndex > 0)
    {
      internal.selectedEntity = ro[objectIndex - 1].staticMesh->entity.handle; 
    }
    else
    {
      internal.selectedEntity = LDK_HANDLE_INVALID; 
    }
  }

  // Unbind the picking framebuffer
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  if (internal.selectedEntity != LDK_HANDLE_INVALID)
  {
    LDKStaticObject* staticObject = ldkEntityLookup(LDKStaticObject, internal.selectedEntity);
    if (staticObject)
    {
      // Turn on wireframe mode
      internal.fixedMaterial = ldkAssetLookup(LDKMaterial, internal.materialHighlightHandle);

      ldkMaterialBind(internal.fixedMaterial);
      ldkMaterialParamSetFloat(internal.fixedMaterial, "deltaTime", internal.elapsedTime);
      ldkMaterialParamSetVec3(internal.fixedMaterial, "color1", internal.higlightColor1);
      ldkMaterialParamSetVec3(internal.fixedMaterial, "color2", internal.higlightColor2);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glLineWidth(3.0);

      // Highlight the selected entity
      internalRenderMesh(staticObject);

      // Turn off wireframe mode
      internal.fixedMaterial = NULL;
      glLineWidth(1.0);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else
    {
      internal.selectedEntity = LDK_HANDLE_INVALID;
    }
  }

#endif

  ldkArenaReset(&internal.bucketROStaticMesh);

  ldkMaterialBind(0);
  ldkRenderBufferBind(NULL);
}

LDKHandle ldkRendererSelectedEntity(void)
{
  return internal.selectedEntity;
}
