#include "gl/glcorearb.h"
#include "array.h"
#include "entity/directionallight.h"
#include "entity/instancedobject.h"
#include "entity/spotlight.h"
#include "ldk/module/renderer.h"
#include "ldk/module/graphics.h"
#include "ldk/module/asset.h"
#include "ldk/engine.h"
#include "ldk/entity/camera.h"
#include "ldk/entity/staticobject.h"
#include "ldk/entity/pointlight.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/material.h"
#include "ldk/asset/config.h"
#include "ldk/asset/shader.h"
#include "ldk/common.h"
#include "ldk/os.h"
#include "ldk/gl.h"
#include "maths.h"
#include "module/entity.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

void internalRenderMeshInstanced(LDKInstancedObject* entity);

#ifndef LDK_RENDERER_OPENGL
#define LDK_RENDERER_OPENGL
#endif // LDK_RENDERER_OPENGL

static struct 
{
  bool initialized;
  LDKConfig* config;
  Vec3 clearColor;
  Vec3 editorClearColor;
  LDKArray* renderObjectArrayDeferred;
  LDKArray* renderObjectArray;
  LDKArray* pointLights;
  LDKArray* directionalLights;
  LDKArray* spotLights;

  LDKCamera* camera;
  float elapsedTime;

  float ambientLightIntensity;
  Vec3 ambientLightColor;


  // Deferred rendering
  GLuint gBuffer;
  LDKShader* shaderGeometryPass;
  LDKShader* shaderLightPass;
  LDKShader* shaderLightBox;
  unsigned int gPosition, gNormal, gAlbedoSpec; //gbuffer-textures
  unsigned int rboDepth; // Depth render buffer


#ifdef LDK_EDITOR
  // Picking
  GLuint fboPicking;
  GLuint texturePicking;
  GLuint textureDepthPicking;
  LDKHAsset hShaderPicking;
  LDKHAsset hShaderHighlight;
  LDKShader* shaderPicking;
  LDKShader* shaderHighlight;
  LDKEntitySelectionInfo selectedEntity;
#endif

  Vec3 higlightColor1;
  Vec3 higlightColor2;

} s_renderer = { 0 };

typedef enum
{
  LDK_RENDER_OBJECT_STATIC_OBJECT,
  LDK_RENDER_OBJECT_INSTANCED_OBJECT
}  LDKRenderObjectType;

typedef struct
{
  LDKRenderObjectType type; 
  uint32 surfaceIndex;
  union
  {
    LDKStaticObject* staticMesh;
    LDKInstancedObject* instancedMesh;
  };
} LDKRenderObject;

//
// Renderer Internal
//
static void InternalRenderXYQuadNDC()
{
  static bool once = true;
  static unsigned int quadVAO = 0;
  static unsigned int quadVBO;

  if (once)
  {
    once = false;
    float quadVertices[] =
    {
      // positions        // texture Coords
      -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
      -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
      1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  }

  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}

static void InternalRenderCubeNDC()
{
  unsigned int cubeVAO = 0;
  unsigned int cubeVBO = 0;

  // initialize (if necessary)
  if (cubeVAO == 0)
  {
    float vertices[] = {
      // back face
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
      1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
      1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
      1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
      -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                                                            // front face
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
      1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
      1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
      1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
      -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                                                            // left face
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
      -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                                                            // right face
      1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
      1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
      1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
      1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
      1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
      1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
                                                           // bottom face
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
      1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
      -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                                                            // top face
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
      1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
      1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
      1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
      -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    // fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // link vertex attributes
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }
  // render Cube
  glBindVertexArray(cubeVAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
}

static Vec3 internalGetClearColor()
{
#ifdef LDK_EDITOR
  if (ldkEngineIsEditorRunning())
    return s_renderer.editorClearColor;
#endif
  return s_renderer.clearColor;
}
#ifdef LDK_EDITOR
static void internalRenderStaticObjectForPicking(LDKStaticObject* entity, uint32 objectIndex)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if(!mesh)
    return;

  Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);
  ldkShaderParamSetMat4(s_renderer.shaderPicking, "mModel", world);
  ldkShaderParamSetUint(s_renderer.shaderPicking, "objectIndex", objectIndex);
  ldkRenderBufferBind(mesh->vBuffer);
  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    ldkShaderParamSetUint(s_renderer.shaderPicking, "surfaceIndex", i);
    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  ldkRenderBufferBind(NULL);
}

static void internalRenderInstancedObjectForPicking(LDKInstancedObject* entity, uint32 objectIndex)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);

  if(!mesh)
    return;

  ldkRenderBufferBind(mesh->vBuffer);
  ldkInstanceBufferBind(entity->instanceBuffer);
  ldkShaderParamSetBool(s_renderer.shaderPicking, "instanced", true);
  ldkShaderParamSetUint(s_renderer.shaderPicking, "objectIndex", objectIndex);

  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    // Test pasing the instance index instead of the surface index
    ldkShaderParamSetUint(s_renderer.shaderPicking, "surfaceIndex", i);
    uint32 numInstances = ldkInstancedObjectCount(entity);
    glDrawElementsInstanced(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)), numInstances);
  }

  ldkInstanceBufferUnbind(entity->instanceBuffer);
  ldkRenderBufferBind(NULL);
  ldkMaterialBind(NULL);
}

static void internalRenderHighlight(LDKStaticObject* entity)
{
#ifdef LDK_EDITOR
  if (entity->entity.isEditorGizmo)
    return;
#endif
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if(!mesh)
    return;


  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glLineWidth(3.0);
  glDisable(GL_DEPTH_TEST);

  Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);

  ldkRenderBufferBind(mesh->vBuffer);
  ldkShaderProgramBind(s_renderer.shaderHighlight);
  ldkShaderParamSetMat4(s_renderer.shaderHighlight, "mModel", world);
  ldkShaderParamSetVec3(s_renderer.shaderHighlight, "color1", s_renderer.higlightColor1);
  ldkShaderParamSetVec3(s_renderer.shaderHighlight, "color2", s_renderer.higlightColor2);
  ldkShaderParamSetMat4(s_renderer.shaderHighlight, "mView", ldkCameraViewMatrix(s_renderer.camera));
  ldkShaderParamSetMat4(s_renderer.shaderHighlight, "mProj", ldkCameraProjectMatrix(s_renderer.camera));
  ldkShaderParamSetFloat(s_renderer.shaderHighlight, "deltaTime", s_renderer.elapsedTime);

  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  glEnable(GL_DEPTH_TEST);
  glLineWidth(1.0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  ldkRenderBufferBind(NULL);
}

//
// Render to the picking bufer and read the entity associated with x,y coords.
// If any entity info is found, returns true and the selection pointer, is
// updated with the selection information.
// If nothing was selected, returns fals and the selection pointer remains
// unchanged.
//
static bool internalRenderPickingBuffer(LDKArray* renderObjects, LDKEntitySelectionInfo* selection, int32 x, int32 y)
{
  // Render Objects in the picking framebuffer
  uint32 count = ldkArrayCount(renderObjects);
  if (count == 0)
  {
    return false;
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_renderer.fboPicking);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  LDKRenderObject* ro = (LDKRenderObject*) ldkArrayGetData(renderObjects);
  ldkShaderProgramBind(s_renderer.shaderPicking);
  ldkShaderParamSetMat4(s_renderer.shaderPicking, "mView", ldkCameraViewMatrix(s_renderer.camera));
  ldkShaderParamSetMat4(s_renderer.shaderPicking, "mProj", ldkCameraProjectMatrix(s_renderer.camera));

  for(uint32 i = 0; i < count; i++)
  {
    if (ro->type == LDK_RENDER_OBJECT_STATIC_OBJECT)
    {
      ldkShaderParamSetBool(s_renderer.shaderPicking, "instanced", false);
      internalRenderStaticObjectForPicking(ro->staticMesh, i + 1);
    }
    else if (ro->type == LDK_RENDER_OBJECT_INSTANCED_OBJECT)
    {
      ldkShaderParamSetBool(s_renderer.shaderPicking, "instanced", true);
      internalRenderInstancedObjectForPicking(ro->instancedMesh, i + 1);
    }
    ro++;
  }

  // Read from the pick framebuffer
  bool changed = false;
  Vec3 pixelColor;

  // Decode entity information from pixel data
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_renderer.fboPicking);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, &pixelColor);
  uint32 objectIndex = (uint32) pixelColor.x;
  uint32 surfaceIndex = (uint32) pixelColor.y;
  uint32 instanceIndex = (uint32) pixelColor.z;
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  if (objectIndex > 0 && (objectIndex - 1) < ldkArrayCount(renderObjects))
  {
    ro = (LDKRenderObject*) ldkArrayGet(renderObjects, objectIndex-1);
    if (ro->type == LDK_RENDER_OBJECT_STATIC_OBJECT)
    {
      selection->handle = ro->staticMesh->entity.handle;
      selection->surfaceIndex = surfaceIndex;
      selection->instanceIndex = instanceIndex;
      changed = true;
    }
    else if (ro->type == LDK_RENDER_OBJECT_INSTANCED_OBJECT)
    {
      selection->handle = ro->instancedMesh->entity.handle;
      selection->surfaceIndex = surfaceIndex;
      selection->instanceIndex = instanceIndex;
      changed = true;
    }
  }
  else
  {
    changed = false;
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  return changed;
}

LDKEntitySelectionInfo ldkRendererSelectedEntity(void)
{
  return s_renderer.selectedEntity;
}

#endif // LDK_EDITOR

static void internalRenderMesh(LDKStaticObject* entity)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);

  if(!mesh)
    return;

  ldkRenderBufferBind(mesh->vBuffer);
  Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);

  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    LDKHAsset* materials = entity->materials ? entity->materials : mesh->materials;
    LDKHAsset hMaterial = materials[surface->materialIndex];
    LDKMaterial* material = ldkAssetLookup(LDKMaterial, hMaterial);
    ldkMaterialParamSetBool(material, "instanced", false);
    ldkMaterialParamSetMat4(material, "mModel", world);
    ldkMaterialBind(material);

    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  ldkRenderBufferBind(NULL);
  ldkMaterialBind(NULL);
}

static void internalRenderMeshInstanced(LDKInstancedObject* entity)
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

    LDKHAsset hMaterial = mesh->materials[surface->materialIndex];
    material = ldkAssetLookup(LDKMaterial, hMaterial);
    ldkMaterialParamSetBool(material, "instanced", true);
    ldkMaterialBind(material);

    uint32 numInstances = ldkInstancedObjectCount(entity);
    glDrawElementsInstanced(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)), numInstances);
  }

  ldkInstanceBufferUnbind(entity->instanceBuffer);
  ldkRenderBufferBind(NULL);
  ldkMaterialBind(NULL);
}

#ifdef LDK_EDITOR
static bool internalPickingFBOCreate(uint32 width, uint32 height)
{
  if (s_renderer.fboPicking != 0)
    return false;

  bool success = false;
  // Create the FBO
  glGenFramebuffers(1, &s_renderer.fboPicking);
  glBindFramebuffer(GL_FRAMEBUFFER, s_renderer.fboPicking);

  // Create the texture object for the primitive information buffer
  glGenTextures(1, &s_renderer.texturePicking);
  glBindTexture(GL_TEXTURE_2D, s_renderer.texturePicking);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height,
      0, GL_RGB, GL_FLOAT, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      s_renderer.texturePicking, 0);

  // Create the texture object for the depth buffer
  glGenTextures(1, &s_renderer.textureDepthPicking);
  glBindTexture(GL_TEXTURE_2D, s_renderer.textureDepthPicking);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height,
      0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
      s_renderer.textureDepthPicking, 0);

  // Disable reading to avoid problems with older GPUs
  glReadBuffer(GL_NONE);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);

  // Verify that the FBO is correct
  GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (Status != GL_FRAMEBUFFER_COMPLETE) {
    printf("FB error, status: 0x%x\n", Status);
    success = false;
  }

#ifdef LDK_EDITOR
  // Loads the picking shader
  s_renderer.hShaderPicking = ldkAssetGet(LDKShader, "assets/editor/picking.shader")->asset.handle;
  s_renderer.hShaderHighlight = ldkAssetGet(LDKShader, "assets/editor/highlight.shader")->asset.handle;

  s_renderer.higlightColor1 = ldkConfigGetVec3(s_renderer.config, "editor.highlight-color1");
  s_renderer.higlightColor2 = ldkConfigGetVec3(s_renderer.config, "editor.highlight-color2");
#endif
  // Restore the default framebuffer
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return success;
}

static void internalPickingFBODEstroy()
{
  // the display can be resized befor we get initialized...
  if (!s_renderer.initialized)
    return;

  glDeleteTextures(1, &s_renderer.texturePicking);
  glDeleteTextures(1, &s_renderer.textureDepthPicking);
  glDeleteFramebuffers(1, &s_renderer.fboPicking);
  s_renderer.fboPicking = 0;
  s_renderer.texturePicking = 0;
  s_renderer.textureDepthPicking = 0;
}
#endif

static void internalGBufferFBODestroy()
{
  glDeleteFramebuffers(1, &s_renderer.gBuffer);
  glDeleteTextures(1, &s_renderer.gPosition);
  glDeleteTextures(1, &s_renderer.gNormal);
  glDeleteTextures(1, &s_renderer.gAlbedoSpec);
  glDeleteRenderbuffers(1, &s_renderer.rboDepth);
}

static void internalGBufferFBOCreate(uint32 width, uint32 height)
{
  if (s_renderer.shaderGeometryPass == NULL)
    s_renderer.shaderGeometryPass = ldkAssetGet(LDKShader, "assets/geometry-pass.shader");

  if (s_renderer.shaderLightPass == NULL)
    s_renderer.shaderLightPass = ldkAssetGet(LDKShader, "assets/light-pass.shader");

  if (s_renderer.shaderLightBox == NULL)
    s_renderer.shaderLightBox = ldkAssetGet(LDKShader, "assets/editor/lightbox.shader");


  glGenFramebuffers(1, &s_renderer.gBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, s_renderer.gBuffer);
  // position color buffer
  glGenTextures(1, &s_renderer.gPosition);
  glBindTexture(GL_TEXTURE_2D, s_renderer.gPosition);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_renderer.gPosition, 0);
  // normal color buffer
  glGenTextures(1, &s_renderer.gNormal);
  glBindTexture(GL_TEXTURE_2D, s_renderer.gNormal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, s_renderer.gNormal, 0);
  // color + specular color buffer
  glGenTextures(1, &s_renderer.gAlbedoSpec);
  glBindTexture(GL_TEXTURE_2D, s_renderer.gAlbedoSpec);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, s_renderer.gAlbedoSpec, 0);

  // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
  unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, attachments);

  // create and attach depth buffer (renderbuffer)
  glGenRenderbuffers(1, &s_renderer.rboDepth);
  glBindRenderbuffer(GL_RENDERBUFFER, s_renderer.rboDepth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_renderer.rboDepth);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // finally check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    ldkLogError("Error creating g-buffer framebuffer");
}


//
// Ambient light settings
//

void ldkAmbientLightSetIntensity(float intensity)
{
  s_renderer.ambientLightIntensity = intensity;
}

float ldkAmbientLightGetIntensity()
{
  return s_renderer.ambientLightIntensity;
}

void ldkAmbientLightSetColor(Vec3 color)
{
  s_renderer.ambientLightColor = color;
}

Vec3 ldkAmbientLightGetColor()
{
  return s_renderer.ambientLightColor;
}


// -----------------------------------------
// Geometry render pass
// -----------------------------------------
static void internalGeometryPass(LDKArray* array)
{
  uint32 count = ldkArrayCount(array);
  LDKRenderObject* ro = (LDKRenderObject*) ldkArrayGetData(array);

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

  glEnable(GL_DEPTH_TEST); // because it might be disabled by a material
}


// -----------------------------------------
// Light pass
// -----------------------------------------
static void internalLightPass()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  ldkShaderProgramBind(s_renderer.shaderLightPass); // Uses GBuffer

  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, s_renderer.gPosition);
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, s_renderer.gNormal);
  glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, s_renderer.gAlbedoSpec);

  Vec3 clearColor = internalGetClearColor();
  ldkShaderParamSetVec3(s_renderer.shaderLightPass, "viewPos", s_renderer.camera->position);
  ldkShaderParamSetVec3(s_renderer.shaderLightPass, "ambientColor", ldkAmbientLightGetColor());
  ldkShaderParamSetVec3(s_renderer.shaderLightPass, "clearColor", clearColor);
  ldkShaderParamSetFloat(s_renderer.shaderLightPass, "ambientIntensity", ldkAmbientLightGetIntensity());
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "gPosition", 0);
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "gNormal", 1);
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "gAlbedoSpec", 2); 

  // Point light uniforms
  LDKPointLight* point = (LDKPointLight*) ldkArrayGetData(s_renderer.pointLights);
  uint32 numPointLights = ldkArrayCount(s_renderer.pointLights);
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "numPointLights", numPointLights);
  for (uint32 i = 0; i < numPointLights; i++)
  {
    // update attenuation parameters and calculate radius
    const float CONSTANT = 1.0f;
    const char* arrayName = "pointLights";
    const float linear = point->attenuation.linear;
    const float quadratic = point->attenuation.quadratic;
    const float maxBrightness = (float) (fmax(fmax(point->colorDiffuse.x, point->colorDiffuse.y), point->colorDiffuse.z));
    float radius = (float) (-linear + sqrt(linear * linear- 4 * quadratic * (CONSTANT - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);

    LDKSmallStr strPosition, strColorDiffuse, strColorSpecular, strLinear, strQuadratic, strRadius;
    ldkSmallStringFormat(&strPosition,      "%s[%d].position", arrayName, i);  
    ldkSmallStringFormat(&strColorDiffuse,  "%s[%d].colorDiffuse", arrayName, i);
    ldkSmallStringFormat(&strColorSpecular, "%s[%d].colorSpecular", arrayName, i);
    ldkSmallStringFormat(&strLinear,        "%s[%d].linear", arrayName, i);  
    ldkSmallStringFormat(&strQuadratic,     "%s[%d].quadratic", arrayName, i);
    ldkSmallStringFormat(&strRadius,        "%s[%d].range", arrayName, i);

    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strPosition.str, point->position);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorDiffuse.str, point->colorDiffuse);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorSpecular.str, point->colorSpecular);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strLinear.str, linear);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strQuadratic.str, quadratic);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strRadius.str, radius);
    point++;
  }

  // Spot light uniforms
  LDKSpotLight* spot = (LDKSpotLight*) ldkArrayGetData(s_renderer.spotLights);
  uint32 numSpotLights = ldkArrayCount(s_renderer.spotLights);
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "numSpotLights", numSpotLights);

  for (uint32 i = 0; i < numSpotLights; i++)
  {
    // update attenuation parameters and calculate radius
    const float CONSTANT = 1.0f;
    const char* arrayName = "spotLights";
    const float linear = spot->attenuation.linear;
    const float quadratic = spot->attenuation.quadratic;
    const float maxBrightness = (float) (fmax(fmax(point->colorDiffuse.x, point->colorDiffuse.y), point->colorDiffuse.z));
    float radius = (float) (-linear + sqrt(linear * linear- 4 * quadratic * (CONSTANT - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);

    LDKSmallStr strPosition, strDirection, strColorDiffuse, strColorSpecular, strLinear, strQuadratic, strCutoffInner, strCutoffOuter, strRadius;
    ldkSmallStringFormat(&strPosition,      "%s[%d].position", arrayName, i);
    ldkSmallStringFormat(&strDirection,     "%s[%d].direction", arrayName, i);
    ldkSmallStringFormat(&strColorDiffuse,  "%s[%d].colorDiffuse", arrayName, i);
    ldkSmallStringFormat(&strColorSpecular, "%s[%d].colorSpecular", arrayName, i);
    ldkSmallStringFormat(&strCutoffInner,   "%s[%d].cutOffInner", arrayName, i);
    ldkSmallStringFormat(&strCutoffOuter,   "%s[%d].cutOffOuter", arrayName, i);
    ldkSmallStringFormat(&strLinear,        "%s[%d].linear", arrayName, i);
    ldkSmallStringFormat(&strQuadratic,     "%s[%d].quadratic", arrayName, i);
    ldkSmallStringFormat(&strRadius,        "%s[%d].range", arrayName, i);

    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strPosition.str, spot->position);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strDirection.str, spot->direction);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorDiffuse.str, spot->colorDiffuse);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorSpecular.str, spot->colorSpecular);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strCutoffInner.str, spot->cutOffInner);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strCutoffOuter.str, spot->cutOffOuter);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strLinear.str, linear);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strQuadratic.str, quadratic);
    ldkShaderParamSetFloat(s_renderer.shaderLightPass, strRadius.str, radius);
    spot++;
  }

  // directional light uniforms
  LDKDirectionalLight* directional = (LDKDirectionalLight*) ldkArrayGetData(s_renderer.directionalLights);
  uint32 numDirectionalLights = ldkArrayCount(s_renderer.directionalLights);
  ldkShaderParamSetInt(s_renderer.shaderLightPass, "numDirectionalLights", numDirectionalLights);
  for (uint32 i = 0; i < numDirectionalLights; i++)
  {
    const char* arrayName = "directionalLights";
    LDKSmallStr strPosition, strColorDiffuse, strColorSpecular;
    ldkSmallStringFormat(&strPosition,      "%s[%d].direction", arrayName, i);  
    ldkSmallStringFormat(&strColorDiffuse,  "%s[%d].colorDiffuse", arrayName, i);
    ldkSmallStringFormat(&strColorSpecular, "%s[%d].colorSpecular", arrayName, i);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strPosition.str, directional->position);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorDiffuse.str, directional->colorDiffuse);
    ldkShaderParamSetVec3(s_renderer.shaderLightPass, strColorSpecular.str, directional->colorSpecular);
    directional++;
  }

  InternalRenderXYQuadNDC();

  // copy geometry's depth buffer to default framebuffer's depth buffer
  LDKSize viewport = ldkGraphicsViewportSizeGet();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_renderer.gBuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
  glBlitFramebuffer(0, 0, viewport.width, viewport.height, 0, 0, viewport.width, viewport.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
}


//
// Renderer
//

bool ldkRendererInitialize(LDKConfig* config)
{
  LDK_ASSERT(s_renderer.initialized == false);
  s_renderer.initialized = true;
  bool success = true;
  s_renderer.clearColor =  vec3(0.0f, 0.0f, 0.0f);
  s_renderer.editorClearColor = ldkConfigGetVec3(config, "editor.clear-color");
  s_renderer.config = config;
  s_renderer.renderObjectArray = ldkArrayCreate(sizeof(LDKRenderObject), 64);
  s_renderer.renderObjectArrayDeferred = ldkArrayCreate(sizeof(LDKRenderObject), 64);
  s_renderer.ambientLightIntensity = 1.0f;
  s_renderer.pointLights = ldkArrayCreate(sizeof(LDKPointLight), 32);
  s_renderer.spotLights = ldkArrayCreate(sizeof(LDKSpotLight), 32);
  s_renderer.directionalLights = ldkArrayCreate(sizeof(LDKDirectionalLight), 8);
  success &= s_renderer.renderObjectArray != NULL;

#ifdef LDK_EDITOR
  //
  // Picking Framebuffer setup
  //
  LDKSize viewport = ldkGraphicsViewportSizeGet();
  internalPickingFBOCreate(viewport.width, viewport.height);
  internalGBufferFBOCreate(viewport.width, viewport.height);
  s_renderer.shaderPicking = ldkAssetLookup(LDKShader, s_renderer.hShaderPicking);
  s_renderer.shaderHighlight = ldkAssetLookup(LDKShader, s_renderer.hShaderHighlight);
#endif
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return success;
}

void ldkRendererResize(uint32 width, uint32 height)
{
  glViewport(0, 0, width, height);
#ifdef LDK_EDITOR
  internalPickingFBODEstroy();
  internalPickingFBOCreate(width, height);
#endif

  internalGBufferFBODestroy();
  internalGBufferFBOCreate(width, height);
}

void ldkRendererTerminate(void)
{
  if (!s_renderer.initialized)
    return;

  s_renderer.initialized = false;
  ldkArrayDestroy(s_renderer.renderObjectArray);
  ldkArrayDestroy(s_renderer.renderObjectArrayDeferred);
  ldkArrayDestroy(s_renderer.directionalLights);
  ldkArrayDestroy(s_renderer.spotLights);
  ldkArrayDestroy(s_renderer.pointLights);
#ifdef LDK_EDITOR
  internalPickingFBODEstroy();
  internalGBufferFBODestroy();
#endif
}

void ldkRendererSetClearColor(LDKRGB color)
{
  s_renderer.clearColor = vec3( color.r * 255.0f, color.g * 255.0f, color.b * 255.0f);
}

void ldkRendererSetClearColorVec3(Vec3 color)
{
  s_renderer.clearColor = color;
}

void ldkRendererSetCamera(LDKCamera* camera)
{
  if (camera->entity.active == FALSE)
  {
    ldkLogError("Using disabled camera for rendering!");
    s_renderer.camera = NULL;
    return;
  }

  s_renderer.camera = camera;
}

LDKCamera* ldkRendererGetCamera()
{
  return s_renderer.camera;
}

void ldkRendererAddPointLight(LDKPointLight* entity)
{
  if (entity->entity.active == false)
    return;
  ldkArrayAdd(s_renderer.pointLights, entity);
}

void ldkRendererAddDirectionalLight(LDKDirectionalLight* entity)
{
  if (entity->entity.active == false)
    return;
  ldkArrayAdd(s_renderer.directionalLights, entity);
}

void ldkRendererAddSpotLight(LDKSpotLight* entity)
{
  if (entity->entity.active == false)
    return;
  ldkArrayAdd(s_renderer.spotLights, entity);
}

void ldkRendererAddStaticObject(LDKStaticObject* entity)
{
  if (entity->entity.active == false)
    return;

  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if (mesh == NULL)
    return;

  LDKHAsset* materials = entity->materials ? entity->materials : mesh->materials;
  const uint32 numMaterials = mesh->numMaterials;

  LDKRenderObject ro;
  ro.type = LDK_RENDER_OBJECT_STATIC_OBJECT;
  ro.staticMesh = entity;

  for (uint32 i = 0; i < numMaterials; i++)
  {
    LDKMaterial* material = ldkAssetLookup(LDKMaterial, materials[i]);
    if (material == NULL)
      continue;

    if (material->deferred)
    {
      ldkArrayAdd(s_renderer.renderObjectArrayDeferred, &ro);
      break;
    }
    else
    {
      ldkArrayAdd(s_renderer.renderObjectArray, &ro);
      break;
    }
  }
}

void ldkRendererAddInstancedObject(LDKInstancedObject* entity)
{
  if (entity->entity.active == false)
    return;

  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if (mesh == NULL)
    return;

  LDKHAsset* materials =  mesh->materials;
  const uint32 numMaterials = mesh->numMaterials;

  LDKRenderObject ro;
  ro.type = LDK_RENDER_OBJECT_INSTANCED_OBJECT;
  ro.instancedMesh = entity;

  for (uint32 i = 0; i < numMaterials; i++)
  {
    LDKMaterial* material = ldkAssetLookup(LDKMaterial, materials[i]);
    if (material == NULL)
      continue;

    if (material->deferred)
    {
      ldkArrayAdd(s_renderer.renderObjectArrayDeferred, &ro);
      break;
    }
    else
    {
      ldkArrayAdd(s_renderer.renderObjectArray, &ro);
      break;
    }
  }
}

void drawLineThick(float x1, float y1, float z1, float x2, float y2, float z2, float thickness)
{
  GLfloat dx = x2 - x1;
  GLfloat dy = y2 - y1;

  // Calculate the normalized perpendicular vector components
  GLfloat nx = dy;
  GLfloat ny = -dx;
  GLfloat length = (float) sqrt(nx * nx + ny * ny);
  nx /= length;
  ny /= length;

  // Calculate half thickness offsets
  GLfloat halfThickness = thickness / 2.0f;
  GLfloat offset_x = halfThickness * nx;
  GLfloat offset_y = halfThickness * ny;

  // Define the vertices
  GLfloat vertices[] = {
    // Top-left corner
    x1 - offset_x, y1 - offset_y, z1,
    // Top-right corner
    x2 - offset_x, y2 - offset_y, z2,
    // Bottom-right corner
    x2 + offset_x, y2 + offset_y, z2,
    // Bottom-left corner
    x1 + offset_x, y1 + offset_y, z1
  };

  // Generate and bind VAO and VBO
  GLuint VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Set vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // Unbind VBO and VAO
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Use shader program and draw
  //glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);
}

void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color)
{
  GLfloat vertices[] = {
    x1, y1, z1,
    x2, y2, z2
  };

  GLfloat colors[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };

  // Generate and bind VAO and VBO
  GLuint VAO, VBO, colorVBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &colorVBO);

  glBindVertexArray(VAO);

  // Bind and set vertex data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // Bind and set color data
  glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(1);

  // Unbind VAO
  glBindVertexArray(0);

  // Use shader program and draw
  glBindVertexArray(VAO);
  glLineWidth(thickness);
  glDrawArrays(GL_LINES, 0, 2);
  glBindVertexArray(0);

  // Clean up
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &colorVBO);
  glDeleteVertexArrays(1, &VAO);
}

void drawCircle(float x, float y, float z, float radius, float thickness, float angle, LDKRGB innerColor, LDKRGB outerColor)
{
    int numSegments = 60; // Number of line segments to approximate the circle
    float theta = fabsf(angle) / numSegments; // Angle between each segment
    float start_angle = (angle >= 0) ? 0.0f : (float)(2 * M_PI - fabs(angle));
    float halfThickness = thickness / 2.0f;

    // Calculate the number of vertices needed
    uint32_t numVertices = (uint32_t)(ceil(fabs(angle) / theta) * 2);

    // If angle is less than 360 degrees, we need to add 2 extra vertices to close the circle
    if (fabs(angle) < 2 * M_PI)
        numVertices += 2;

    // Generate and bind VAO and VBO
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Each vertex needs position (3 components) and color (3 components)
    GLfloat vertices[255 * 6]; // 6 components (x, y, z, r, g, b) per vertex

    for (uint32_t i = 0; i < numVertices / 2; i++) {
        float currentAngle = start_angle + i * theta;

        // Inner vertex (position + color)
        vertices[i * 12] = x + (radius - halfThickness) * cosf(currentAngle);
        vertices[i * 12 + 1] = y + (radius - halfThickness) * sinf(currentAngle);
        vertices[i * 12 + 2] = z;

        // Inner vertex color (innerColor)
        vertices[i * 12 + 3] = innerColor.r;
        vertices[i * 12 + 4] = innerColor.g;
        vertices[i * 12 + 5] = innerColor.b;

        // Outer vertex (position + color)
        vertices[i * 12 + 6] = x + (radius + halfThickness) * cosf(currentAngle);
        vertices[i * 12 + 7] = y + (radius + halfThickness) * sinf(currentAngle);
        vertices[i * 12 + 8] = z;

        // Outer vertex color (outerColor)
        vertices[i * 12 + 9] = outerColor.r;
        vertices[i * 12 + 10] = outerColor.g;
        vertices[i * 12 + 11] = outerColor.b;
    }

    // If angle is less than 360 degrees, add vertices to close the circle
    if (fabs(angle) > 2 * M_PI) {
        float last_angle = start_angle + fabsf(angle);

        // Inner vertex of the last segment
        vertices[numVertices * 6 - 12] = x + (radius - halfThickness) * cosf(last_angle);
        vertices[numVertices * 6 - 11] = y + (radius - halfThickness) * sinf(last_angle);
        vertices[numVertices * 6 - 10] = z;
        vertices[numVertices * 6 - 9] = innerColor.r;
        vertices[numVertices * 6 - 8] = innerColor.g;
        vertices[numVertices * 6 - 7] = innerColor.b;

        // Outer vertex of the last segment
        vertices[numVertices * 6 - 6] = x + (radius + halfThickness) * cosf(last_angle);
        vertices[numVertices * 6 - 5] = y + (radius + halfThickness) * sinf(last_angle);
        vertices[numVertices * 6 - 4] = z;
        vertices[numVertices * 6 - 3] = outerColor.r;
        vertices[numVertices * 6 - 2] = outerColor.g;
        vertices[numVertices * 6 - 1] = outerColor.b;
    }

    // Load vertex data into buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute pointers for position (3 components)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Set vertex attribute pointers for color (3 components)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Unbind VBO and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Use shader program and draw
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices);
    glBindVertexArray(0);

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void drawFrustum(float nearWidth, float nearHeight, float farWidth, float farHeight, float nearDist, float farDist, LDKRGB color)
{
  // Define vertices of the frustum edges
  GLfloat vertices[] = {
    // Near plane
    -nearWidth / 2, -nearHeight / 2, -nearDist,
    nearWidth / 2, -nearHeight / 2, -nearDist,
    nearWidth / 2, -nearHeight / 2, -nearDist,
    nearWidth / 2, nearHeight / 2, -nearDist,
    nearWidth / 2, nearHeight / 2, -nearDist,
    -nearWidth / 2, nearHeight / 2, -nearDist,
    -nearWidth / 2, nearHeight / 2, -nearDist,
    -nearWidth / 2, -nearHeight / 2, -nearDist,

    // Far plane
    -farWidth / 2, -farHeight / 2, -farDist,
    farWidth / 2, -farHeight / 2, -farDist,
    farWidth / 2, -farHeight / 2, -farDist,
    farWidth / 2, farHeight / 2, -farDist,
    farWidth / 2, farHeight / 2, -farDist,
    -farWidth / 2, farHeight / 2, -farDist,
    -farWidth / 2, farHeight / 2, -farDist,
    -farWidth / 2, -farHeight / 2, -farDist,

    // Connecting lines between near and far planes
    -nearWidth / 2, -nearHeight / 2, -nearDist,
    -farWidth / 2, -farHeight / 2, -farDist,
    nearWidth / 2, -nearHeight / 2, -nearDist,
    farWidth / 2, -farHeight / 2, -farDist,
    nearWidth / 2, nearHeight / 2, -nearDist,
    farWidth / 2, farHeight / 2, -farDist,
    -nearWidth / 2, nearHeight / 2, -nearDist,
    -farWidth / 2, farHeight / 2, -farDist,

    // Triangle to indicate up direction (front face)
    -farWidth / 4, farHeight / 2, -farDist,
    farWidth / 4, farHeight / 2, -farDist,
    0.0f, 3 * farHeight / 4, -farDist,

    // Triangle to indicate up direction (back face)
    farWidth / 4, farHeight / 2, -farDist,
    -farWidth / 4, farHeight / 2, -farDist,
    0.0f, 3 * farHeight / 4, -farDist
  };

  // Define colors (for each vertex)
  GLfloat colors[sizeof(vertices) / sizeof(GLfloat)]; // Same size as vertices array

  for (int i = 0; i < sizeof(colors) / sizeof(GLfloat); i += 3) {
    colors[i] = color.r;
    colors[i + 1] = color.g;
    colors[i + 2] = color.b;
  }

  // Generate and bind VAO and VBOs
  GLuint VAO, VBO, colorVBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &colorVBO);

  glBindVertexArray(VAO);

  // Bind and set vertex data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE0);

  // Bind and set color data
  glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
  glVertexAttribPointer(LDK_VERTEX_ATTRIBUTE1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(LDK_VERTEX_ATTRIBUTE1);

  // Unbind VAO
  glBindVertexArray(0);

  // Use shader program and draw
  glBindVertexArray(VAO);
  glDrawArrays(GL_LINES, 0, sizeof(vertices) / (3 * sizeof(GLfloat)));
  glDrawArrays(GL_TRIANGLES, sizeof(vertices) / (3 * sizeof(GLfloat)) - 6, 3); // Draw the front face of the triangle
  glDrawArrays(GL_TRIANGLES, sizeof(vertices) / (3 * sizeof(GLfloat)) - 3, 3); // Draw the back face of the triangle
  glBindVertexArray(0);

  // Clean up
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &colorVBO);
  glDeleteVertexArrays(1, &VAO);
}

void ldkRendererRender(float deltaTime)
{
  glEnable(GL_DEPTH_TEST); 
  glEnable(GL_CULL_FACE);

  s_renderer.elapsedTime += deltaTime;

  if (s_renderer.camera == NULL)
    return;

  glBindFramebuffer(GL_FRAMEBUFFER, s_renderer.gBuffer);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Deferred path
  internalGeometryPass(s_renderer.renderObjectArrayDeferred);
  internalLightPass();

  // Forward path
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  internalGeometryPass(s_renderer.renderObjectArray);

#ifdef LDK_EDITOR
  if (ldkEngineIsEditorRunning())
  {
    LDKMouseState mouseState;
    ldkOsMouseStateGet(&mouseState);
    if (ldkOsMouseButtonDown(&mouseState, LDK_MOUSE_BUTTON_LEFT))
    {
      // Picking and selection
      int32 x = mouseState.cursor.x;
      int32 y = ldkGraphicsViewportSizeGet().height - mouseState.cursor.y;

      LDKEntitySelectionInfo selection = {0};
      internalRenderPickingBuffer(s_renderer.renderObjectArrayDeferred, &selection, x, y);
      internalRenderPickingBuffer(s_renderer.renderObjectArray, &selection, x, y);
      s_renderer.selectedEntity = selection;
    }

    if (ldkHandleIsValid(s_renderer.selectedEntity.handle))
    {
      LDKEntity* entity = ldkEntityManagerFind(s_renderer.selectedEntity.handle);
      LDKHandleType t = ldkHandleType(s_renderer.selectedEntity.handle.value);
      if (entity)
      {
        if (t == typeid(LDKStaticObject) && entity)
        {
          internalRenderHighlight((LDKStaticObject*) entity);
        }
        else if (t == typeid(LDKInstancedObject))
        {
          LDKInstancedObject* io = (LDKInstancedObject*) entity;
          LDKObjectInstance* instance = ldkArrayGet(io->instanceArray, s_renderer.selectedEntity.instanceIndex);

          //TODO: This is ugly. Get rid of this static entity
          static LDKStaticObject o;
          o.mesh = io->mesh;
          o.scale = instance->scale;
          o.position = instance->position;
          o.rotation = instance->rotation;
          internalRenderHighlight(&o);
        }
        else
        {
          LDK_NOT_IMPLEMENTED();
        }
      }
    }
  }
#endif // LDK_EDITOR

  ldkArrayClear(s_renderer.renderObjectArray);
  ldkArrayClear(s_renderer.renderObjectArrayDeferred);
  ldkArrayClear(s_renderer.spotLights);
  ldkArrayClear(s_renderer.directionalLights);
  ldkArrayClear(s_renderer.pointLights);
  ldkMaterialBind(0);
  ldkRenderBufferBind(NULL);
}
