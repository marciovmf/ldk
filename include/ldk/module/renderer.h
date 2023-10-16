/**
 *
 * render.h
 * 
 * Provides a high-level rendering API for the engine.
 * Separates and encapsulates Graphics API-specific code, allowing the engine to
 *  function without being tied to any particular Graphics API.
 */

#ifndef LDK_RENDER_H
#define LDK_RENDER_H

#include "../common.h"
#include "../maths.h"

typedef LDKHandle LDKHMaterial;
typedef LDKHandle LDKHShader;
typedef LDKHandle LDKHShaderProgram;
typedef LDKHandle LDKHMesh;
typedef LDKHandle LDKHTexture;

typedef void* LDKVertexBuffer;
typedef void* LDKIndexBuffer;

typedef enum
{
  LDK_VERTEX_LAYOUT_NONE  = 0,  // No attributes enabled
  LDK_VERTEX_LAYOUT_PNU   = 1,  // Position, Normal, UV
  LDK_VERTEX_LAYOUT_PNTBU = 2,  // Position, Normal, Tangent, Binormal, UV
} LDKVertexLayout;

LDK_API bool ldkRendererInitialize();
LDK_API void ldkRendererTerminate();

//
// Shader
//
LDK_API LDKHShaderProgram ldkShaderProgramCreate(LDKHShader handleVs, LDKHShader handleFs, LDKHShader handleGs);
LDK_API LDKHShader ldkFragmentShaderCreate(const char* source);
LDK_API LDKHShader ldkVertexShaderCreate(const char* source);
LDK_API LDKHShader ldkGeometryShaderCreate(const char* source);

LDK_API bool ldkShaderDestroy(LDKHShader handle);
LDK_API bool ldkShaderProgramDestroy(LDKHShaderProgram handle);
LDK_API bool ldkShaderProgramBind(LDKHShaderProgram handle); // Passing in NULL will unbind current Shader program

//
// Material
//
LDK_API LDKHMaterial ldkMaterialCreate(LDKHShaderProgram handle, const char* name);
LDK_API bool ldkMaterialDestroy(LDKHMaterial handle);
LDK_API bool ldkMaterialParamSetInt(LDKHMaterial handle, const char* name, int value);
LDK_API bool ldkMaterialParamSetFloat(LDKHMaterial handle, const char* name, float value);
LDK_API bool ldkMaterialParamSetVec2(LDKHMaterial handle, const char* name, Vec2 value);
LDK_API bool ldkMaterialParamSetVec3(LDKHMaterial handle, const char* name, Vec3 value);
LDK_API bool ldkMaterialParamSetVec4(LDKHMaterial handle, const char* name, Vec4 value);
LDK_API bool ldkMaterialParamSetTexture(LDKHMaterial handle, const char* name, LDKHTexture value);
LDK_API bool ldkMaterialBind(LDKHMaterial handle); // Passing in NULL will unbind current Material

//
// Texture
//

typedef enum
{
 LDK_TEXTURE_WRAP_CLAMP_TO_EDGE   = 0,
 LDK_TEXTURE_WRAP_CLAMP_TO_BORDER = 1,
 LDK_TEXTURE_WRAP_MIRRORED_REPEAT = 2,
 LDK_TEXTURE_WRAP_REPEAT          = 3,
} LDKTextureParamWrap;

typedef enum
{
 LDK_TEXTURE_FILTER_LINEAR        = 0,
 LDK_TEXTURE_FILTER_NEAREST       = 1,
} LDKTextureParamFilter;

typedef enum
{
 LDK_TEXTURE_MIPMAP_MODE_NONE     = 0,
 LDK_TEXTURE_MIPMAP_MODE_NEAREST  = 1,
 LDK_TEXTURE_MIPMAP_MODE_LINEAR   = 2,
} LDKTextureParamMipmap;

LDK_API LDKHTexture ldkTextureCreate(LDKTextureParamMipmap mipmapModeParam, LDKTextureParamWrap wrapParam, LDKTextureParamFilter filterMinParam, LDKTextureParamFilter filterMaxParam);
LDK_API bool ldkTextureData(LDKHTexture handle, uint32 width, uint32 height, void* data, uint32 bitsPerPixel);
LDK_API bool ldkTextureDestroy(LDKHTexture handle);


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
LDK_API void ldkRenderMesh(LDKVertexBuffer buffer, uint32 count, size_t satrt);

#endif //LDK_RENDER_H

