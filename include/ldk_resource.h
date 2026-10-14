/**
 * # Renderer resource handles
 *
 * Generic renderer-facing resource handle.
 *
 * Renderer resources use the same opaque storage type as RHI resources,
 * but they are not necessarily direct RHI objects. A mesh, for example,
 * may be a renderer-owned object that contains multiple RHI buffers.
 */

#ifndef LDK_RESOURCE_H
#define LDK_RESOURCE_H

#include <module/ldk_rhi.h>

/** @group ldk.resource */
typedef struct LDKResourceTexture
{
  LDKRHITexture id;
} LDKResourceTexture;

/** @group ldk.resource */
typedef struct LDKResourceMesh
{
  LDKRHIResource id;
} LDKResourceMesh;

/** @group ldk.resource */
typedef struct LDKResourceShader
{
  LDKRHIShaderModule id;
} LDKResourceShader;


/** @group ldk.resource */
typedef struct LDKResourceMaterial
{
  LDKRHIResource id;
} LDKResourceMaterial;

/** @group ldk.resource */
typedef struct LDKResourceInstanceSet
{
  LDKRHIResource id;
} LDKResourceInstanceSet;

/** @group ldk.resource */
typedef struct LDKResourceSkybox
{
  LDKRHIResource id;
} LDKResourceSkybox;

/** @group ldk.resource */
#define LDK_RESOURCE_TEXTURE_INVALID  ((LDKResourceTexture){LDK_RHI_INVALID_RESOURCE})
/** @group ldk.resource */
#define LDK_RESOURCE_MESH_INVALID     ((LDKResourceMesh){LDK_RHI_INVALID_RESOURCE})
/** @group ldk.resource */
#define LDK_RESOURCE_SHADER_INVALID   ((LDKResourceShader){LDK_RHI_INVALID_RESOURCE})
/** @group ldk.resource */
#define LDK_RESOURCE_MATERIAL_INVALID ((LDKResourceMaterial){LDK_RHI_INVALID_RESOURCE})
/** @group ldk.resource */
#define LDK_RESOURCE_INSTANCE_SET_INVALID ((LDKResourceInstanceSet){LDK_RHI_INVALID_RESOURCE})
/** @group ldk.resource */
#define LDK_RESOURCE_SKYBOX_INVALID \
  ((LDKResourceSkybox){LDK_RHI_INVALID_RESOURCE})

#endif //LDK_RESOURCE_H
