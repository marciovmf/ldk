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
  // RenderBuffer
  //

  typedef enum
  {
    LDK_VERTEX_LAYOUT_NONE  = 0,  // No attributes enabled
    LDK_VERTEX_LAYOUT_PNU   = 1,  // Position, Normal, UV
    LDK_VERTEX_LAYOUT_PNTBU = 2,  // Position, Normal, Tangent, Binormal, UV
  } LDKVertexLayout;

  typedef enum
  {
    LDK_VERTEX_ATTRIB_NORMALIZE = 0x01 << 1,
    LDK_VERTEX_ATTRIB_INSTANCE  = 0x01 << 2
  } LDKVertexAttributeFlag;

  typedef enum
  {
    // suggested semantic names
    LDK_VERTEX_ATTRIBUTE_POSITION   = 0,
    LDK_VERTEX_ATTRIBUTE_NORMAL     = 1,
    LDK_VERTEX_ATTRIBUTE_TANGENT    = 2,
    LDK_VERTEX_ATTRIBUTE_BINORMAL   = 3,
    LDK_VERTEX_ATTRIBUTE_TEXCOORD0  = 4,
    LDK_VERTEX_ATTRIBUTE_TEXCOORD1  = 5,
    LDK_VERTEX_ATTRIBUTE_CUSTOM0    = 6,
    LDK_VERTEX_ATTRIBUTE_CUSTOM1    = 7,
    LDK_VERTEX_ATTRIBUTE_MATRIX0    = 8,  // it takes 4 locations
    LDK_VERTEX_ATTRIBUTE_MATRIX1    = 12, // it takes 4 locations

    // Non semantic
    LDK_VERTEX_ATTRIBUTE0           = 0,
    LDK_VERTEX_ATTRIBUTE1           = 1,
    LDK_VERTEX_ATTRIBUTE2           = 2,
    LDK_VERTEX_ATTRIBUTE3           = 3,
    LDK_VERTEX_ATTRIBUTE4           = 4,
    LDK_VERTEX_ATTRIBUTE5           = 5,
    LDK_VERTEX_ATTRIBUTE6           = 6,
    LDK_VERTEX_ATTRIBUTE7           = 7,
    LDK_VERTEX_ATTRIBUTE8           = 8,
    LDK_VERTEX_ATTRIBUTE9           = 9,
    LDK_VERTEX_ATTRIBUTE10          = 10,
    LDK_VERTEX_ATTRIBUTE11          = 11,
    LDK_VERTEX_ATTRIBUTE12          = 12,
    LDK_VERTEX_ATTRIBUTE13          = 13,
    LDK_VERTEX_ATTRIBUTE14          = 14,
    LDK_VERTEX_ATTRIBUTE15          = 15,

    LDK_VERTEX_ATTRIBUTE_MAX        = 15  // The higher attribute location guaranteed to work with openGL
  } LDKVertexAttributeSemantic;

  LDK_API LDKRenderBuffer* ldkRenderBufferCreate(int numVBOs);
  LDK_API void ldkRenderBufferDestroy(LDKRenderBuffer* rb);
  LDK_API void ldkRenderBufferBind(const LDKRenderBuffer* rb); // passing null unbinds the current buffer
  LDK_API void ldkVertexBufferSetAttributeMat4(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeFloat(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeInt(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeUInt(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeByte(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeUByte(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeDouble(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeShort(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetAttributeUShort(const LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkVertexBufferSetData(const LDKRenderBuffer* rb, int32 index, size_t size, const void *data, bool stream);
  LDK_API void ldkVertexBufferSetSubData(const LDKRenderBuffer* rb, int32 index, int32 offset, size_t size, const void *data);
  LDK_API void ldkIndexBufferSetData(LDKRenderBuffer* rb, size_t size, const void *data, bool stream);
  LDK_API void ldkIndexBufferSetSubData(LDKRenderBuffer* rb, int32 offset, size_t size, const void *data);


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

