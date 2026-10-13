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

/** @group core.resources */
typedef struct LDKResourceTexture
{
  LDKRHITexture id;
} LDKResourceTexture;

/** @group core.resources */
typedef struct LDKResourceMesh
{
  LDKRHIResource id;
} LDKResourceMesh;

/** @group core.resources */
typedef struct LDKResourceShader
{
  LDKRHIShaderModule id;
} LDKResourceShader;


/** @group core.resources */
typedef struct LDKResourceMaterial
{
  LDKRHIResource id;
} LDKResourceMaterial;

/** @group core.resources */
typedef struct LDKResourceInstanceSet
{
  LDKRHIResource id;
} LDKResourceInstanceSet;

/** @group core.resources */
typedef struct LDKResourceSkybox
{
  LDKRHIResource id;
} LDKResourceSkybox;

/** @group core.resources */
#define LDK_RESOURCE_TEXTURE_INVALID  ((LDKResourceTexture){LDK_RHI_INVALID_RESOURCE})
/** @group core.resources */
#define LDK_RESOURCE_MESH_INVALID     ((LDKResourceMesh){LDK_RHI_INVALID_RESOURCE})
/** @group core.resources */
#define LDK_RESOURCE_SHADER_INVALID   ((LDKResourceShader){LDK_RHI_INVALID_RESOURCE})
/** @group core.resources */
#define LDK_RESOURCE_MATERIAL_INVALID ((LDKResourceMaterial){LDK_RHI_INVALID_RESOURCE})
/** @group core.resources */
#define LDK_RESOURCE_INSTANCE_SET_INVALID ((LDKResourceInstanceSet){LDK_RHI_INVALID_RESOURCE})
/** @group core.resources */
#define LDK_RESOURCE_SKYBOX_INVALID \
  ((LDKResourceSkybox){LDK_RHI_INVALID_RESOURCE})

#endif //LDK_RESOURCE_H
