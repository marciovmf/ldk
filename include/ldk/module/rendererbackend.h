/**
 *
 * renderbackend.h
 *
 * A minimal graphics API abstraction to be used by entities and assets to
 * avoid use of Graphics API specific code.
 *
 */

#ifndef LDK_RENDER_BACKEND_H
#define LDK_RENDER_BACKEND_H

#include "../asset/shader.h"
#include "../asset/material.h"
#include "../asset/texture.h"


typedef struct LDKRenderBuffer_t LDKRenderBuffer;
typedef struct LDKInstanceBuffer_t LDKInstanceBuffer;

#ifdef __cplusplus
extern "C" {
#endif


  //
  // Shader
  //

  LDK_API bool ldkShaderProgramCreate(const char* vs, const char* fs, const char* gs, LDKShader* out);
  LDK_API bool ldkFragmentShaderCreate(const char* source, LDKShader* shader);
  LDK_API bool ldkVertexShaderCreate(const char* source, LDKShader* shader);
  LDK_API bool ldkGeometryShaderCreate(const char* source, LDKShader* shader);
  LDK_API void ldkShaderDestroy(LDKShader* shader);
  LDK_API bool ldkShaderProgramBind(LDKShader* shaderAsset); // Passing in NULL will unbind current Shader program
  LDK_API bool ldkShaderParamSetBool(LDKShader* shader, const char* name, bool value);
  LDK_API bool ldkShaderParamSetInt(LDKShader* shader, const char* name, int32 value);
  LDK_API bool ldkShaderParamSetUint(LDKShader* shader, const char* name, uint32 value);
  LDK_API bool ldkShaderParamSetFloat(LDKShader* shader, const char* name, float value);
  LDK_API bool ldkShaderParamSetVec2(LDKShader* shader, const char* name, Vec2 value);
  LDK_API bool ldkShaderParamSetVec3(LDKShader* shader, const char* name, Vec3 value);
  LDK_API bool ldkShaderParamSetVec4(LDKShader* shader, const char* name, Vec4 value);
  LDK_API bool ldkShaderParamSetMat4(LDKShader* shader, const char* name, Mat4 value);
  LDK_API bool ldkShaderParamSetTexture(LDKShader* shader, const char* name, LDKTexture* value);

  //
  // Material
  //

  LDK_API bool ldkMaterialCreate(LDKShader* handle, LDKMaterial* out);
  LDK_API void ldkMaterialDestroy(LDKMaterial* material);
  LDK_API bool ldkMaterialParamSetBool(LDKMaterial* material, const char* name, bool value);
  LDK_API bool ldkMaterialParamSetInt(LDKMaterial* material, const char* name, int32 value);
  LDK_API bool ldkMaterialParamSetUint(LDKMaterial* material, const char* name, uint32 value);
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
  // A RenderBuffer is a collection of GPU buffers and vertex attributes.
  // It will also create an implicit Index buffer if any index data is added.
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

  LDK_API LDKRenderBuffer* ldkRenderBufferCreate(uint32 numVBOs);
  LDK_API void ldkRenderBufferDestroy(LDKRenderBuffer* rb);
  LDK_API void ldkRenderBufferBind(const LDKRenderBuffer* rb); // passing null unbinds the current buffer

  LDK_API void ldkRenderBufferAttributeSetMat4(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetFloat(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetInt(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetUInt(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetByte(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetUByte(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetDouble(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetShort(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferAttributeSetUShort(LDKRenderBuffer* rb, uint32 index, LDKVertexAttributeSemantic semantic, uint32 count, int32 stride, const void *offset, LDKVertexAttributeFlag flag);
  LDK_API void ldkRenderBufferSetVertexData(const LDKRenderBuffer* rb, int32 index, size_t size, const void *data, bool stream);
  LDK_API void ldkRenderBufferSetVertexSubData(const LDKRenderBuffer* rb, int32 index, int32 offset, size_t size, const void *data);
  LDK_API void ldkRenderBufferSetIndexData(LDKRenderBuffer* rb, size_t size, const void *data, bool stream);
  LDK_API void ldkRenderBufferSetIndexSubData(LDKRenderBuffer* rb, int32 offset, size_t size, const void *data);


  //
  // Instance Buffer
  //
  // An instance buffer is a gpu buffer bound to LDK_VERTEX_ATTRIBUTE_MATRIX0
  // meant to store world transformations (Mat4) per instance.
  // It can be used in conjunction with a RenderBuffer to render multiple
  // instances of Mesh in a single draw call
  //

  LDK_API LDKInstanceBuffer* ldkInstanceBufferCreate(void);
  LDK_API void ldkInstanceBufferDestroy(LDKInstanceBuffer* ib);
  LDK_API void ldkInstanceBufferBind(LDKInstanceBuffer* ib);
  LDK_API void ldkInstanceBufferUnbind(LDKInstanceBuffer* ib);
  LDK_API void ldkInstanceBufferSetData(LDKInstanceBuffer* ib, size_t size, const void *data, bool stream);
  LDK_API void ldkInstanceBufferSetSubData(LDKInstanceBuffer* ib, int32 offset, size_t size, const void *data);

#ifdef __cplusplus
}
#endif

#endif //LDK_RENDER_BACKEND_H

