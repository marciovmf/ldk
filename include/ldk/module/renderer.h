/**
 *
 * render.h
 * 
 * Provides a high-level rendering API for the engine.
 * Separates and encapsulates Graphics API-specific code, allowing the engine to
 * function without being tied to any particular Graphics API.
 */

#ifndef LDK_RENDER_H
#define LDK_RENDER_H

#include "../common.h"
#include "../entity/camera.h"
#include "../entity/staticobject.h"
#include "../asset/shader.h"
#include "../asset/config.h"
#include "../asset/material.h"
#include "../asset/mesh.h"
#include "../maths.h"

#ifdef __cplusplus
extern "C" {
#endif


  typedef enum
  {
    LDK_VERTEX_LAYOUT_NONE  = 0,  // No attributes enabled
    LDK_VERTEX_LAYOUT_PNU   = 1,  // Position, Normal, UV
    LDK_VERTEX_LAYOUT_PNTBU = 2,  // Position, Normal, Tangent, Binormal, UV
  } LDKVertexLayout;

  LDK_API bool ldkRendererInitialize(LDKConfig* config);
  LDK_API void ldkRendererResize(uint32 width, uint32 height);
  LDK_API void ldkRendererTerminate(void);

  //
  // Shader
  //

  LDK_API bool ldkShaderProgramCreate(const char* vs, const char* fs, const char* gs, LDKShader* out);
  LDK_API bool ldkFragmentShaderCreate(const char* source, LDKShader* shader);
  LDK_API bool ldkVertexShaderCreate(const char* source, LDKShader* shader);
  LDK_API bool ldkGeometryShaderCreate(const char* source, LDKShader* shader);

  LDK_API void ldkShaderDestroy(LDKShader* shader);
  LDK_API bool ldkShaderProgramBind(LDKShader* shaderAsset); // Passing in NULL will unbind current Shader program

  //
  // Material
  //
  LDK_API bool ldkMaterialCreate(LDKShader* handle, LDKMaterial* out);
  LDK_API void ldkMaterialDestroy(LDKMaterial* material);
  LDK_API bool ldkMaterialParamSetInt(LDKMaterial* material, const char* name, int value);
  LDK_API bool ldkMaterialParamSetFloat(LDKMaterial* material, const char* name, float value);
  LDK_API bool ldkMaterialParamSetVec2(LDKMaterial* material, const char* name, Vec2 value);
  LDK_API bool ldkMaterialParamSetVec3(LDKMaterial* material, const char* name, Vec3 value);
  LDK_API bool ldkMaterialParamSetVec4(LDKMaterial* material, const char* name, Vec4 value);
  LDK_API bool ldkMaterialParamSetMat4(LDKMaterial* material, const char* name, Mat4 value);
  LDK_API bool ldkMaterialParamSetTexture(LDKMaterial* handle, const char* name, LDKTexture* value);
  LDK_API bool ldkMaterialBind(LDKMaterial* material); // Passing in NULL will unbind current Material

  //
  // Texture
  //

  LDK_API bool ldkTextureCreate(LDKTextureParamMipmap mipmapModeParam, LDKTextureParamWrap wrapParam, LDKTextureParamFilter filterMinParam, LDKTextureParamFilter filterMaxParam, LDKTexture* texture);
  LDK_API bool ldkTextureData(LDKTexture* texture, uint32 width, uint32 height, void* data, uint32 bitsPerPixel);
  LDK_API void ldkTextureDestroy(LDKTexture* texture);


  //
  // VertexBuffer
  //
  LDK_API LDKVertexBuffer ldkVertexBufferCreate(LDKVertexLayout layout);
  LDK_API void ldkVertexBufferData(LDKVertexBuffer buffer, void* data, size_t size);
  LDK_API void ldkVertexBufferSubData(LDKVertexBuffer buffer, size_t offset, void* data, size_t size);
  LDK_API void ldkVertexBufferIndexData(LDKVertexBuffer buffer, void* data, size_t size);
  LDK_API void ldkVertexBufferIndexSubData(LDKVertexBuffer buffer, size_t offset, void* data, size_t size);
  LDK_API void ldkVertexBufferBind(LDKVertexBuffer buffer); // Pasing in NULL will unbind current vertexBuffer
  LDK_API void ldkVertexBufferDestroy(LDKVertexBuffer vertexBuffer);

  //
  // Rendering
  //
  LDK_API void ldkRenderMesh(LDKMesh* mesh);
  LDK_API void ldkRendererCameraSet(LDKCamera* camera);
  LDK_API void ldkRendererClearColor(LDKRGB color);

  LDK_API void ldkRendererAddStaticObject(LDKStaticObject* entity);
  LDK_API void ldkRendererRender(float deltaTime);


  //
  // Editor
  //

  // Call this function once per frame when detecting click over the scene.
  // This call is relatively slow.
  LDK_API LDKHandle ldkRendererSelectedEntity(void);

#ifdef __cplusplus
}
#endif

#endif //LDK_RENDER_H

