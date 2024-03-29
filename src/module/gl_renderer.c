#include "ldk/module/renderer.h"
#include "ldk/module/graphics.h"
#include "ldk/module/asset.h"
#include "ldk/entity/camera.h"
#include "ldk/entity/staticobject.h"
#include "ldk/entity/instancedobject.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/texture.h"
#include "ldk/asset/material.h"
#include "ldk/asset/config.h"
#include "ldk/asset/shader.h"
#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/hlist.h"
#include "ldk/gl.h"
#include <stdbool.h>
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
  SHADER_MODE_DEFAULT = 0,
  SHADER_MODE_PICKING = 1,
  SHADER_MODE_HIGHLIGHT = 2,
} LDKShaderMode;

#define LDK_GL_ERROR_LOG_SIZE 1024
static struct 
{
  bool initialized;
  LDKConfig* config;
  LDKRGB clearColor;
  char errorBuffer[LDK_GL_ERROR_LOG_SIZE];
  LDKArena bucketROStaticMesh;
  LDKCamera* camera;
  LDKShaderMode shaderMode;
  float elapsedTime;

  // Picking
  GLuint fboPicking;
  GLuint texturePicking;
  GLuint textureDepthPicking;
  LDKHandle hShaderPicking;
  LDKHandle hShaderHighlight;
  LDKShader* shaderPicking;
  LDKShader* shaderHighlight;
  LDKHandle materialInstancedHandle;

  Vec3 higlightColor1;
  Vec3 higlightColor2;

  LDKHandle selectedEntity;
  uint32 selectedEntityIndex;
} internal = { 0 };

typedef enum
{
  LDK_RENDER_OBJECT_STATIC_OBJECT,
  LDK_RENDER_OBJECT_INSTANCED_OBJECT
}  LDKRenderObjectType;

typedef struct
{
  LDKRenderObjectType type; 
  union
  {
    LDKStaticObject* staticMesh;
    LDKInstancedObject* instancedMesh;
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
  internal.hShaderPicking   = ldkAssetGet(LDKShader, "assets/editor/picking.shader")->asset.handle;
  internal.hShaderHighlight = ldkAssetGet(LDKShader, "assets/editor/highlight.shader")->asset.handle;

  // Loads the intanced rendering shader
  internal.materialInstancedHandle = ldkAssetGet(LDKMaterial, "assets/instanced.material")->asset.handle;

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

#ifdef LDK_DEBUG
#define internalSetShaderUniformError(path, type, name) ldkLogWarning("Failed to set param '%s %s' shader '%s'. Param not found", type, name, path)
#else
#define internalSetShaderUniformError(path, type, name)
#endif

bool ldkShaderParamSetInt(LDKShader* shader, const char* name, int32 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "int", name);
    return false;
  }

  glUniform1i(location, value);
  return true;
}

bool ldkShaderParamSetUint(LDKShader* shader, const char* name, uint32 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "unsigned int", name);
    return false;
  }
  
  glUniform1ui(location, value);
  return true;
}

bool ldkShaderParamSetFloat(LDKShader* shader, const char* name, float value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "float", name);
    return false;
  }
  
  glUniform1f(location, value);
  return true;
}

bool ldkShaderParamSetVec2(LDKShader* shader, const char* name, Vec2 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "vec2", name);
    return false;
  }
  
  glUniform2f(location, value.x, value.y);
  return true;
}

bool ldkShaderParamSetVec3(LDKShader* shader, const char* name, Vec3 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "vec3", name);
    return false;
  }
  
  glUniform3f(location, value.x, value.y, value.z);
  return true;
}

bool ldkShaderParamSetVec4(LDKShader* shader, const char* name, Vec4 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "vec4", name);
    return false;
  }

  glUniform4f(location, value.x, value.y, value.z, value.w);
  return true;
}

bool ldkShaderParamSetMat4(LDKShader* shader, const char* name, Mat4 value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "mat4", name);
    return false;
  }

  glUniformMatrix4fv(location,  1, GL_TRUE, (float*)&value);
  return true;
}

bool ldkShaderParamSetTexture(LDKShader* shader, const char* name, LDKTexture* value)
{
  GLuint location = glGetUniformLocation(shader->gl.id, name);
  if (location < 0)
  {
    internalSetShaderUniformError(shader->asset.path, "texture", name);
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, value->gl.id);
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
  if (numUniforms > LDK_SHADER_MAX_PARAMS)
  {
    ldkLogError("Shader %s declares too many uniforms (%d). MAximum is %d.", program->asset.path, numUniforms, LDK_SHADER_MAX_PARAMS);
    return false;
  }

  int32 maxUniformNameLength = 0;
  glGetProgramiv(program->gl.id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);
  if (numUniforms > LDK_SHADER_MAX_PARAMS)
  {
    ldkLogError("Shader %s declares at least one uniform name longer than (%d)", program->asset.path, LDK_SMALL_STRING_MAX_LENGTH);
    return LDK_HANDLE_INVALID;
  }

  out->name        = (const char*) "unnamed-material";
  out->numParam    = numUniforms;
  out->program     = program->asset.handle;
  out->numTextures = 0;
  memset(&out->param, 0, (sizeof(LDKMaterialParam) * LDK_SHADER_MAX_PARAMS));
  memset(&out->textures, 0, (sizeof(LDKHandle) * LDK_MATERIAL_MAX_TEXTURES));

  GLuint uniformType;
  GLsizei uniformSize;

  // Fill up the list of material properties from the shader 
  for (int i = 0; i < numUniforms; i ++)
  {
    LDKMaterialParam* param = &out->param[i];

    glGetActiveUniform(program->gl.id, i,
        LDK_SMALL_STRING_MAX_LENGTH,
        (GLsizei*) &param->name.length,
        &uniformSize,
        &uniformType,
        (char*) &out->param[i].name);

    switch(uniformType)
    {
      case GL_INT:
        param->type = LDK_MATERIAL_PARAM_TYPE_INT;
        break;
      case GL_UNSIGNED_INT:
        param->type = LDK_MATERIAL_PARAM_TYPE_UINT;
        break;
      case GL_FLOAT:
        param->type = LDK_MATERIAL_PARAM_TYPE_FLOAT;
        break;
      case GL_FLOAT_VEC2:
        param->type = LDK_MATERIAL_PARAM_TYPE_VEC2;
        break;
      case GL_FLOAT_VEC3:
        param->type = LDK_MATERIAL_PARAM_TYPE_VEC3;
        break;
      case GL_FLOAT_VEC4:
        param->type = LDK_MATERIAL_PARAM_TYPE_VEC4;
        break;
      case GL_FLOAT_MAT4:
        param->type = LDK_MATERIAL_PARAM_TYPE_MAT4;
        break;
      case GL_SAMPLER_2D:
        param->type = LDK_MATERIAL_PARAM_TYPE_TEXTURE;
        break;
      default:
        ldkLogWarning("Material '%s' references param with unsupported type '%s %s'",
            out->asset.path, ldkOpenglTypeName(uniformType), (const char*) &param->name);
        break;

    }
  }

  return true;
}

void ldkMaterialDestroy(LDKMaterial* material)
{
  ldkAssetDispose(material);
}

LDKMaterialParam* internalMaterialParamGet(LDKMaterial* material, const char* name, LDKMaterialParamType type)
{
  for (uint32 i = 0; i < material->numParam; i++)
  {
    LDKMaterialParam* param = &material->param[i];
    if (strncmp(param->name.str, name, param->name.length) == 0 && param->type == type)
    {
      return param;
    }
  }
  return NULL;
}

bool ldkMaterialParamSetInt(LDKMaterial* material, const char* name, int value)
{
  if (!material) return false;
  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_INT);
  if (!param) return false;

  param->intValue = value;
  return true;
}

bool ldkMaterialParamSetUInt(LDKMaterial* material, const char* name, uint32 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_UINT);
  if (!param) return false;

  param->uintValue = value;
  return true;
}

bool ldkMaterialParamSetFloat(LDKMaterial* material, const char* name, float value)
{
  if (!material) return false;

  LDKMaterialParam* param =  internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_FLOAT);
  if (!param) return false;

  param->floatValue = value;
  return true;
}

bool ldkMaterialParamSetVec2(LDKMaterial* material, const char* name, Vec2 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_VEC2);
  if (!param) return false;

  param->vec2Value = value;
  return true;
}

bool ldkMaterialParamSetVec3(LDKMaterial* material, const char* name, Vec3 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_VEC3);
  if (!param) return false;

  param->vec3Value = value;
  return true;
}

bool ldkMaterialParamSetVec4(LDKMaterial* material, const char* name, Vec4 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_VEC3);
  if (!param) return false;

  param->vec4Value = value;
  return true;
}

bool ldkMaterialParamSetMat4(LDKMaterial* material, const char* name, Mat4 value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_MAT4);
  if (!param) return false;

  param->mat4Value = value;
  return true;
}

bool ldkMaterialParamSetTexture(LDKMaterial* material, const char* name, LDKTexture* value)
{
  if (!material) return false;

  LDKMaterialParam* param = internalMaterialParamGet(material, name, LDK_MATERIAL_PARAM_TYPE_TEXTURE);
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
    switch(param->type)
    {
      case LDK_MATERIAL_PARAM_TYPE_INT:
        ldkShaderParamSetInt(shader, param->name.str, param->intValue);
        break;

      case LDK_MATERIAL_PARAM_TYPE_UINT:
        ldkShaderParamSetUint(shader, param->name.str, param->uintValue);
        break;

      case LDK_MATERIAL_PARAM_TYPE_FLOAT:
        ldkShaderParamSetFloat(shader, param->name.str, param->floatValue);
        break;

      case LDK_MATERIAL_PARAM_TYPE_VEC2:
        ldkShaderParamSetVec2(shader, param->name.str, param->vec2Value);
        break;

      case LDK_MATERIAL_PARAM_TYPE_VEC3:
        ldkShaderParamSetVec3(shader, param->name.str, param->vec3Value);
        break;

      case LDK_MATERIAL_PARAM_TYPE_VEC4:
        ldkShaderParamSetVec4(shader, param->name.str, param->vec4Value);
        break;

      case LDK_MATERIAL_PARAM_TYPE_MAT4:
        ldkShaderParamSetMat4(shader, param->name.str, param->mat4Value);
        break;

      case LDK_MATERIAL_PARAM_TYPE_TEXTURE:
        glActiveTexture(GL_TEXTURE0 + param->textureIndexValue);
        LDKHandle hTexture = material->textures[param->textureIndexValue];
        LDKTexture* texture = ldkAssetLookup(LDKTexture, hTexture);
        ldkShaderParamSetTexture(shader, param->name.str, texture);
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

struct LDKInstanceBuffer_t
{
  GLuint  vbo;
  size_t  size;
  bool    isStream;
};

LDKRenderBuffer* ldkRenderBufferCreate(uint32 numVBOs)
{
  if (numVBOs == 0)
    return NULL;

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


static void internalVertexBufferAttributeSetGLType(GLenum glType, LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  LDK_ASSERT(count > 0 && count <= 4);
  LDK_ASSERT(semantic >= 0 && semantic <= LDK_VERTEX_ATTRIBUTE_MAX);
  GLboolean normalize = (flag & LDK_VERTEX_ATTRIB_NORMALIZE) > 0 ? 1 : 0;
  uint32 divisor = (flag & LDK_VERTEX_ATTRIB_INSTANCE) > 0 ? 1 : 0;
  rb->hasInstancedAttributes = divisor > 0;
  GLuint location = semantic;
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);
  glVertexAttribPointer(location, count, glType, normalize, stride, offset);
  glEnableVertexAttribArray(location);
  glVertexAttribDivisor(location, divisor);
}

void ldkRenderBufferAttributeSetMat4(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  LDK_ASSERT(semantic >= 0 && semantic <= LDK_VERTEX_ATTRIBUTE_MAX);
  GLboolean normalize = (flag & LDK_VERTEX_ATTRIB_NORMALIZE) > 0 ? 1 : 0;
  uint32 divisor = (flag & LDK_VERTEX_ATTRIB_INSTANCE) > 0 ? 1 : 0;
  rb->hasInstancedAttributes = divisor > 0;

  GLuint location = semantic;
  glBindBuffer(GL_ARRAY_BUFFER, rb->vbo[index]);

  glVertexAttribPointer(location + 0, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 0 * sizeof(float) * 4));
  glVertexAttribPointer(location + 1, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 1 * sizeof(float) * 4));
  glVertexAttribPointer(location + 2, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 2 * sizeof(float) * 4));
  glVertexAttribPointer(location + 3, 4, GL_FLOAT, normalize, stride, (const GLvoid *)((const char *)offset + 3 * sizeof(float) * 4));

  glEnableVertexAttribArray(location + 0);
  glEnableVertexAttribArray(location + 1);
  glEnableVertexAttribArray(location + 2);
  glEnableVertexAttribArray(location + 3);

  glVertexAttribDivisor(location + 0, divisor);
  glVertexAttribDivisor(location + 1, divisor);
  glVertexAttribDivisor(location + 2, divisor);
  glVertexAttribDivisor(location + 3, divisor);
}

void ldkRenderBufferAttributeSetFloat(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_FLOAT, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetInt(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_INT, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetUInt(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_UNSIGNED_INT, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetByte(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_BYTE, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetUByte(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_UNSIGNED_BYTE, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetDouble(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_DOUBLE, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetShort(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_SHORT, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferAttributeSetUShort(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag)
{
  internalVertexBufferAttributeSetGLType(GL_UNSIGNED_SHORT, rb, index, semantic, count, stride, offset, flag);
}

void ldkRenderBufferSetVertexData(const LDKRenderBuffer* rb, int index, size_t size, const void *data, bool stream)
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

void ldkRenderBufferSetVertexSubData(const LDKRenderBuffer* rb, int32 index, int32 offset, size_t size, const void *data)
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

void ldkRenderBufferSetIndexData(LDKRenderBuffer* rb, size_t size, const void *data, bool stream)
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

void ldkRenderBufferSetIndexSubData(LDKRenderBuffer* rb, int32 offset, size_t size, const void *data)
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
// Instance Buffer
//

LDKInstanceBuffer* ldkInstanceBufferCreate(void)
{
  LDKInstanceBuffer* ib = (LDKInstanceBuffer*) ldkOsMemoryAlloc(sizeof(LDKInstanceBuffer));
  glGenBuffers(1, &ib->vbo);
  ib->size = 0;
  ib->isStream = false;
  return ib;
}

void ldkInstanceBufferDestroy(LDKInstanceBuffer* ib)
{
  glDeleteBuffers(1, &ib->vbo);
  ldkOsMemoryFree(ib);
}

void ldkInstanceBufferBind(LDKInstanceBuffer* ib)
{
  if (!ib->vbo)
    return;

  glBindBuffer(GL_ARRAY_BUFFER, ib->vbo);

  const int32 stride = (int32) (16 * sizeof(float));
  GLuint location = LDK_VERTEX_ATTRIBUTE_MATRIX0;
  const char* addr = 0;

  glVertexAttribPointer(location + 0, 4, GL_FLOAT, false, stride, (const GLvoid *)(addr + 0 * sizeof(float) * 4));
  glVertexAttribPointer(location + 1, 4, GL_FLOAT, false, stride, (const GLvoid *)(addr + 1 * sizeof(float) * 4));
  glVertexAttribPointer(location + 2, 4, GL_FLOAT, false, stride, (const GLvoid *)(addr + 2 * sizeof(float) * 4));
  glVertexAttribPointer(location + 3, 4, GL_FLOAT, false, stride, (const GLvoid *)(addr + 3 * sizeof(float) * 4));

  glEnableVertexAttribArray(location + 0);
  glEnableVertexAttribArray(location + 1);
  glEnableVertexAttribArray(location + 2);
  glEnableVertexAttribArray(location + 3);

  glVertexAttribDivisor(location + 0, 1);
  glVertexAttribDivisor(location + 1, 1);
  glVertexAttribDivisor(location + 2, 1);
  glVertexAttribDivisor(location + 3, 1);
}

void ldkInstanceBufferUnbind(LDKInstanceBuffer* ib)
{
  if (!ib->vbo)
    return;

  GLuint location = LDK_VERTEX_ATTRIBUTE_MATRIX0;
  glDisableVertexAttribArray(location + 0);
  glDisableVertexAttribArray(location + 1);
  glDisableVertexAttribArray(location + 2);
  glDisableVertexAttribArray(location + 3);

  glVertexAttribDivisor(location + 0, 0);
  glVertexAttribDivisor(location + 1, 0);
  glVertexAttribDivisor(location + 2, 0);
  glVertexAttribDivisor(location + 3, 0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkInstanceBufferSetData(LDKInstanceBuffer* ib, size_t size, const void *data, bool stream)
{
  if (ib->vbo == 0)
  {
    glGenBuffers(1, &ib->vbo);
    ib->size = 0;
    ib->isStream = stream;
  }

  glBindBuffer(GL_ARRAY_BUFFER, ib->vbo);

  if (size > ib->size)
  {
    glBufferData(GL_ARRAY_BUFFER, size, data, stream ? GL_STREAM_DRAW : GL_STATIC_DRAW);
    ib->size = size;
  }
  else
  {
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ldkInstanceBufferSetSubData(LDKInstanceBuffer* ib, int32 offset, size_t size, const void *data)
{
  glBindBuffer(GL_ARRAY_BUFFER, ib->vbo);

  // Check if the buffer needs to be recreated with a larger size
  size_t dataSize = ib->size;
  if (size + offset > dataSize)
  {
    bool isForStreaming = ib->isStream;
    glBufferData(GL_ARRAY_BUFFER, size + offset, NULL, isForStreaming ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size + offset, data);
    ib->size = size + offset;
  }
  else
  {
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
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
  LDKRenderObject* ro = (LDKRenderObject*) ldkArenaAllocate(&internal.bucketROStaticMesh, LDKRenderObject);
  ro->type = LDK_RENDER_OBJECT_STATIC_OBJECT;
  ro->staticMesh = entity;
}

void ldkRendererAddInstancedObject(LDKInstancedObject* entity)
{
  LDKRenderObject* ro = (LDKRenderObject*) ldkArenaAllocate(&internal.bucketROStaticMesh, LDKRenderObject);
  ro->type = LDK_RENDER_OBJECT_INSTANCED_OBJECT;
  ro->instancedMesh = entity;
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
    if (internal.shaderMode == SHADER_MODE_PICKING)
    {
      ldkShaderParamSetUint(internal.shaderPicking, "surfaceIndex", i);
      ldkShaderParamSetMat4(internal.shaderPicking, "mModel", world);
    }
    else if (internal.shaderMode == SHADER_MODE_HIGHLIGHT)
    {
      ldkShaderProgramBind(internal.shaderHighlight);
      ldkShaderParamSetMat4(internal.shaderHighlight, "mModel", world);
      ldkShaderParamSetFloat(internal.shaderHighlight, "deltaTime", internal.elapsedTime);
      ldkShaderParamSetVec3(internal.shaderHighlight, "color1", internal.higlightColor1);
      ldkShaderParamSetVec3(internal.shaderHighlight, "color2", internal.higlightColor2);
      ldkShaderParamSetMat4(internal.shaderHighlight, "mView", ldkCameraViewMatrix(internal.camera));
      ldkShaderParamSetMat4(internal.shaderHighlight, "mProj", ldkCameraProjectMatrix(internal.camera));
    }
    else if (internal.shaderMode == SHADER_MODE_DEFAULT)
    {
      LDKHandle hMaterial = mesh->materials[surface->materialIndex];
      material = ldkAssetLookup(LDKMaterial, hMaterial);
      ldkMaterialBind(material);
      ldkMaterialParamSetMat4(material, "mModel", world);
    }

    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  ldkRenderBufferBind(NULL);
}

void internalRenderMeshInstanced(LDKInstancedObject* entity)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);

  if(!mesh)
    return;

  ldkRenderBufferBind(mesh->vBuffer);
  ldkInstanceBufferBind(entity->instanceBuffer);

  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    LDKMaterial* material = NULL;

    // if rendering with picking material, we set the surface index
    if (internal.shaderMode == SHADER_MODE_PICKING) 
    {
      ldkShaderParamSetUint(internal.shaderPicking, "surfaceIndex", i);
      Mat4* world = ((Mat4*) &entity->instanceWorldMatrixList) + i;
      ldkShaderParamSetMat4(internal.shaderPicking, "mModel", *world);
    }
    else if (internal.shaderMode == SHADER_MODE_HIGHLIGHT)
    {
      Mat4* world = ((Mat4*) &entity->instanceWorldMatrixList) + i;
      ldkShaderProgramBind(internal.shaderHighlight);
      ldkShaderParamSetMat4(internal.shaderHighlight, "mModel", *world);
      ldkShaderParamSetFloat(internal.shaderHighlight, "deltaTime", internal.elapsedTime);
      ldkShaderParamSetVec3(internal.shaderHighlight, "color1", internal.higlightColor1);
      ldkShaderParamSetVec3(internal.shaderHighlight, "color2", internal.higlightColor2);
      ldkShaderParamSetMat4(internal.shaderHighlight, "mView", ldkCameraViewMatrix(internal.camera));
      ldkShaderParamSetMat4(internal.shaderHighlight, "mProj", ldkCameraProjectMatrix(internal.camera));
    }
    else if (internal.shaderMode == SHADER_MODE_DEFAULT)
    {
      LDKHandle hMaterial = mesh->materials[surface->materialIndex];
      material = ldkAssetLookup(LDKMaterial, hMaterial);
      ldkMaterialBind(material);
    }

    uint32 numInstances = ldkInstancedObjectCount(entity);
    glDrawElementsInstanced(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)), numInstances);
  }

  ldkInstanceBufferUnbind(entity->instanceBuffer);
  ldkRenderBufferBind(NULL);
}

void ldkRendererRender(float deltaTime)
{
  internal.elapsedTime += deltaTime;

  uint32 count = (uint32) ldkArenaUsedGet(&internal.bucketROStaticMesh) / sizeof(LDKRenderObject);
  LDKRenderObject* ro = (LDKRenderObject*) ldkArenaDataGet(&internal.bucketROStaticMesh);
  internal.shaderMode = SHADER_MODE_DEFAULT;

  // Lookup fixed shaders
  internal.shaderPicking  = ldkAssetLookup(LDKShader, internal.hShaderPicking);
  internal.shaderHighlight = ldkAssetLookup(LDKShader, internal.hShaderHighlight);

  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); 
  glDepthFunc(GL_LEQUAL);
  glClearColor(internal.clearColor.r / 255.0f, internal.clearColor.g / 255.0f, internal.clearColor.b / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for(uint32 i = 0; i < count; i++)
  {
    switch (ro->type)
    {
      case LDK_RENDER_OBJECT_STATIC_OBJECT:
        internalRenderMesh(ro->staticMesh);
        break;

      case LDK_RENDER_OBJECT_INSTANCED_OBJECT:
        internalRenderMeshInstanced(ro->instancedMesh);
        break;

      default:
        LDK_ASSERT_BREAK();
        break;
    }
    ro++;
  }

#if 1

  //
  // Render the Picking buffer
  //

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, internal.fboPicking);

  internal.shaderMode = SHADER_MODE_PICKING;
  ldkShaderProgramBind(internal.shaderPicking);
  ldkShaderParamSetMat4(internal.shaderPicking, "mView", ldkCameraViewMatrix(internal.camera));
  ldkShaderParamSetMat4(internal.shaderPicking, "mProj", ldkCameraProjectMatrix(internal.camera));

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ro = (LDKRenderObject*) ldkArenaDataGet(&internal.bucketROStaticMesh);

  for(uint32 i = 0; i < count; i++)
  {
    ldkShaderParamSetUint(internal.shaderPicking, "objectIndex", i + 1);
    if (ro->type == LDK_RENDER_OBJECT_STATIC_OBJECT)
      internalRenderMesh(ro->staticMesh);
    else if (ro->type == LDK_RENDER_OBJECT_INSTANCED_OBJECT)
      internalRenderMeshInstanced(ro->instancedMesh);
    ro++;
  }

  //
  // Picking
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
      ldkLogInfo("Selected entity = %d", internal.selectedEntity);
    }
    else
    {
      internal.selectedEntity = LDK_HANDLE_INVALID; 
    }
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);


  //
  // Picking Highlight
  //

  if (internal.selectedEntity != LDK_HANDLE_INVALID)
  {
    LDKInstancedObject* io = NULL;
    LDKStaticObject* so = ldkEntityLookup(LDKStaticObject, internal.selectedEntity);
    if (!so)
    {
      io = ldkEntityLookup(LDKInstancedObject, internal.selectedEntity);
    }

    if (so || io)
    {
      // Turn on wireframe mode and highlight the selected entity
      internal.shaderMode = SHADER_MODE_HIGHLIGHT;
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glLineWidth(3.0);
      glDisable(GL_DEPTH_TEST);

      if (so)
        internalRenderMesh(so);
      else
        internalRenderMeshInstanced(io);

      glEnable(GL_DEPTH_TEST);
      glLineWidth(1.0);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      internal.shaderMode = SHADER_MODE_DEFAULT;
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
