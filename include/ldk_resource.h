/**
 * @file ldk_resource.h
 * @brief Generic renderer-facing resource handle.
 *
 * Renderer resources use the same opaque storage type as RHI resources,
 * but they are not necessarily direct RHI objects. A mesh, for example,
 * may be a renderer-owned object that contains multiple RHI buffers.
 */

#ifndef LDK_RESOURCE_H
#define LDK_RESOURCE_H

#include <module/ldk_rhi.h>

typedef struct LDKResourceTexture
{
  LDKRHITexture id;
} LDKResourceTexture;

typedef struct LDKResourceMesh
{
  LDKRHIResource id;
} LDKResourceMesh;

typedef struct LDKResourceShader
{
  LDKRHIShaderModule id;
} LDKResourceShader;


typedef struct LDKResourceMaterial
{
  LDKRHIResource id;
} LDKResourceMaterial;


#define LDK_RESOURCE_TEXTURE_INVALID ((LDKResourceTexture){LDK_RHI_INVALID_TEXTURE})
#define LDK_RESOURCE_MESH_INVALID    ((LDKResourceMesh){LDK_RHI_INVALID_RESOURCE})
#define LDK_RESOURCE_SHADER_INVALID  ((LDKResourceShader){LDK_RHI_INVALID_SHADER_MODULE})
#define LDK_RESOURCE_MATERIAL_INVALID  ((LDKResourceMaterial){LDK_RHI_INVALID_MATERIAL)

#endif //LDK_RESOURCE_H
