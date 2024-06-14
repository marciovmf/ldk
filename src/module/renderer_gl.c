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
#include "ldk/hlist.h"
#include "ldk/gl.h"
#include "module/entity.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

#ifndef LDK_RENDERER_OPENGL
#define LDK_RENDERER_OPENGL
#endif // LDK_RENDERER_OPENGL


typedef struct
{
  Vec3 colorDiffuse;
  Vec3 colorSpecular;
  float linear;
  float quadratic;
  union{
    Vec4 position;
    Vec4 direction;
  };
} LDKLight;

typedef struct 
{
  float objectIndex;
  float surfaceIndex;
  float triangleIndex;
} LDKPickingPixelInfo;

static struct 
{
  bool initialized;
  LDKConfig* config;
  Vec3 clearColor;
  Vec3 editorClearColor;
  LDKArray* renderObjectArrayDeferred;
  LDKArray* renderObjectArray;
  LDKArray* lights;

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


  // Picking
  GLuint fboPicking;
  GLuint texturePicking;
  GLuint textureDepthPicking;
  LDKHandle hShaderPicking;
  LDKHandle hShaderHighlight;
  LDKShader* shaderPicking;
  LDKShader* shaderHighlight;

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
  static unsigned int quadVAO = 0;
  static unsigned int quadVBO;

  if (quadVAO == 0)
  {
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
  if (ldkEngineIsEditorRunning())
    return internal.editorClearColor;

  return internal.clearColor;
}

static void internalRenderForPicking(LDKStaticObject* entity, uint32 objectIndex)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if(!mesh)
    return;

  ldkShaderParamSetUint(internal.shaderPicking, "objectIndex", objectIndex);
  Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);
  ldkShaderParamSetMat4(internal.shaderPicking, "mModel", world);
  ldkRenderBufferBind(mesh->vBuffer);
  for(uint32 i = 0; i < mesh->numSurfaces; i++)
  {
    LDKSurface* surface = &mesh->surfaces[i];
    ldkShaderParamSetUint(internal.shaderPicking, "surfaceIndex", i);
    glDrawElements(GL_TRIANGLES, surface->count, GL_UNSIGNED_SHORT, (void*) (surface->first * sizeof(uint16)));
  }

  ldkRenderBufferBind(NULL);
}

static void internalRenderHighlight(LDKStaticObject* entity)
{
  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if(!mesh)
    return;

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glLineWidth(3.0);
  glDisable(GL_DEPTH_TEST);


  Mat4 world = mat4World(entity->position, entity->scale, entity->rotation);

  ldkRenderBufferBind(mesh->vBuffer);
  ldkShaderProgramBind(internal.shaderHighlight);
  ldkShaderParamSetMat4(internal.shaderHighlight, "mModel", world);
  ldkShaderParamSetFloat(internal.shaderHighlight, "deltaTime", internal.elapsedTime);
  ldkShaderParamSetVec3(internal.shaderHighlight, "color1", internal.higlightColor1);
  ldkShaderParamSetVec3(internal.shaderHighlight, "color2", internal.higlightColor2);
  ldkShaderParamSetMat4(internal.shaderHighlight, "mView", ldkCameraViewMatrix(internal.camera));
  ldkShaderParamSetMat4(internal.shaderHighlight, "mProj", ldkCameraProjectMatrix(internal.camera));

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

static void internalRenderPickingBuffer(LDKArray* renderObjects)
{
  // Render Objects in the picking framebuffer
  uint32 count = ldkArrayCount(renderObjects);
  if (count == 0)
    return;

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, internal.fboPicking);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ldkShaderProgramBind(internal.shaderPicking);
  ldkShaderParamSetMat4(internal.shaderPicking, "mView", ldkCameraViewMatrix(internal.camera));
  ldkShaderParamSetMat4(internal.shaderPicking, "mProj", ldkCameraProjectMatrix(internal.camera));

  LDKRenderObject* ro = (LDKRenderObject*) ldkArrayGetData(renderObjects);
  for(uint32 i = 0; i < count; i++)
  {
    if (ro->type == LDK_RENDER_OBJECT_STATIC_OBJECT)
      internalRenderForPicking(ro->staticMesh, i + 1);
    ro++;
  }

  // Read from the pick framebuffer
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
      ro = (LDKRenderObject*) ldkArrayGetData(renderObjects);
      internal.selectedEntity = ro[objectIndex - 1].staticMesh->entity.handle; 
      ldkLogInfo("Selected entity = %llx", internal.selectedEntity);
    }
    else
    {
      internal.selectedEntity = LDK_HANDLE_INVALID; 
    }
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);


  //
  // Highlight picked entity
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
      if (so)
        internalRenderHighlight(so);
      //else internalRenderMeshInstanced(io);
    }
    else
    {
      internal.selectedEntity = LDK_HANDLE_INVALID;
    }
  }
}

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
    LDKHandle* materials = entity->materials ? entity->materials : mesh->materials;
    LDKHandle hMaterial = materials[surface->materialIndex];
    LDKMaterial* material = ldkAssetLookup(LDKMaterial, hMaterial);
    ldkMaterialParamSetBool(material, "instanced", false);

    ldkMaterialBind(material);
    ldkMaterialParamSetMat4(material, "mModel", world);

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

    LDKHandle hMaterial = mesh->materials[surface->materialIndex];
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

static bool internalPickingFBOCreate(uint32 width, uint32 height)
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

  internal.higlightColor1 = ldkConfigGetVec3(internal.config, "editor.highlight-color1");
  internal.higlightColor2 = ldkConfigGetVec3(internal.config, "editor.highlight-color2");

  // Restore the default framebuffer
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return success;
}

static void internalPickingFBODEstroy()
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

static void internalGBufferDestroy()
{
  glDeleteFramebuffers(1, &internal.gBuffer);
  glDeleteTextures(1, &internal.gPosition);
  glDeleteTextures(1, &internal.gNormal);
  glDeleteTextures(1, &internal.gAlbedoSpec);
  glDeleteRenderbuffers(1, &internal.rboDepth);
}

static void internalGBufferCreate(uint32 width, uint32 height)
{
  if (internal.shaderGeometryPass == NULL)
    internal.shaderGeometryPass = ldkAssetGet(LDKShader, "assets/geometry-pass.shader");

  if (internal.shaderLightPass == NULL)
    internal.shaderLightPass = ldkAssetGet(LDKShader, "assets/light-pass.shader");

  if (internal.shaderLightBox == NULL)
    internal.shaderLightBox = ldkAssetGet(LDKShader, "assets/editor/lightbox.shader");


  glGenFramebuffers(1, &internal.gBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, internal.gBuffer);
  // position color buffer
  glGenTextures(1, &internal.gPosition);
  glBindTexture(GL_TEXTURE_2D, internal.gPosition);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal.gPosition, 0);
  // normal color buffer
  glGenTextures(1, &internal.gNormal);
  glBindTexture(GL_TEXTURE_2D, internal.gNormal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, internal.gNormal, 0);
  // color + specular color buffer
  glGenTextures(1, &internal.gAlbedoSpec);
  glBindTexture(GL_TEXTURE_2D, internal.gAlbedoSpec);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, internal.gAlbedoSpec, 0);

  // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
  unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, attachments);

  // create and attach depth buffer (renderbuffer)
  glGenRenderbuffers(1, &internal.rboDepth);
  glBindRenderbuffer(GL_RENDERBUFFER, internal.rboDepth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, internal.rboDepth);
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
  internal.ambientLightIntensity = intensity;
}

float ldkAmbientLightGetIntensity()
{
  return internal.ambientLightIntensity;
}

void ldkAmbientLightSetColor(Vec3 color)
{
  internal.ambientLightColor = color;
}

Vec3 ldkAmbientLightGetColor()
{
  return internal.ambientLightColor;
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
}


// -----------------------------------------
// Light pass
// -----------------------------------------
static void internalLightPass(LDKArray* lights)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // lighting pass: calculate lighting 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  ldkShaderProgramBind(internal.shaderLightPass);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, internal.gAlbedoSpec);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, internal.gNormal);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, internal.gPosition);

  uint32 numLights = ldkArrayCount(lights);

  Vec3 clearColor = internalGetClearColor();
  ldkShaderParamSetInt(internal.shaderLightPass, "numLights", numLights);
  ldkShaderParamSetFloat(internal.shaderLightPass, "ambientIntensity", ldkAmbientLightGetIntensity());
  ldkShaderParamSetVec3(internal.shaderLightPass, "ambientColor", ldkAmbientLightGetColor());
  ldkShaderParamSetVec3(internal.shaderLightPass, "clearColor", clearColor);
  ldkShaderParamSetInt(internal.shaderLightPass, "gPosition", 0);
  ldkShaderParamSetInt(internal.shaderLightPass, "gNormal", 1);
  ldkShaderParamSetInt(internal.shaderLightPass, "gAlbedoSpec", 2); 

  // send light uniforms
  LDKLight* light = (LDKLight*) ldkArrayGetData(lights);
  for (uint32 i = 0; i < numLights; i++)
  {
    LDKSmallStr strPosition, strColorDiffuse, strColorSpecular, strLinear, strQuadratic, strRadius;
    ldkSmallStringFormat(&strPosition,      "lights[%d].position", i);  
    ldkSmallStringFormat(&strColorDiffuse,  "lights[%d].colorDiffuse", i);
    ldkSmallStringFormat(&strColorSpecular, "lights[%d].colorSpecular", i);
    ldkShaderParamSetVec4(internal.shaderLightPass, strPosition.str, light[i].position);
    ldkShaderParamSetVec3(internal.shaderLightPass, strColorDiffuse.str, light[i].colorDiffuse);
    ldkShaderParamSetVec3(internal.shaderLightPass, strColorSpecular.str, light[i].colorSpecular);

    // update attenuation parameters and calculate radius
    const float CONSTANT = 1.0f; // We don't send it to the shader. We assume it is always 1.0
    const float linear = light->linear;
    const float quadratic = light->quadratic;

    ldkSmallStringFormat(&strLinear,    "lights[%d].linear", i);  
    ldkSmallStringFormat(&strQuadratic, "lights[%d].quadratic", i);
    ldkShaderParamSetFloat(internal.shaderLightPass, strLinear.str, light[i].linear);
    ldkShaderParamSetFloat(internal.shaderLightPass, strQuadratic.str, quadratic);

    const float maxBrightness = (float) (fmax(fmax(light[i].colorDiffuse.x, light[i].colorDiffuse.y), light[i].colorDiffuse.z));
    float radius = (float) (-linear + sqrt(linear * linear - 4 * quadratic * (CONSTANT - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
    //fabs(sin(internal.elapsedTime ) * 2.0f);

    ldkSmallStringFormat(&strRadius, "lights[%d].range", i);
    ldkShaderParamSetFloat(internal.shaderLightPass, strRadius.str, radius);

    light++;
  }

  ldkShaderParamSetVec3(internal.shaderLightPass, "viewPos", internal.camera->position);
  InternalRenderXYQuadNDC();

  // copy geometry's depth buffer to default framebuffer's depth buffer
  LDKSize viewport = ldkGraphicsViewportSizeGet();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, internal.gBuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
  glBlitFramebuffer(0, 0, viewport.width, viewport.height, 0, 0, viewport.width, viewport.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


//
// Renderer
//

bool ldkRendererInitialize(LDKConfig* config)
{
  LDK_ASSERT(internal.initialized == false);
  internal.initialized = true;
  bool success = true;
  internal.clearColor =  vec3(0.0f, 0.0f, 0.0f);
  internal.editorClearColor = ldkConfigGetVec3(config, "editor.clear-color");
  internal.config = config;
  internal.renderObjectArray = ldkArrayCreate(sizeof(LDKRenderObject), 64);
  internal.renderObjectArrayDeferred = ldkArrayCreate(sizeof(LDKRenderObject), 64);
  internal.ambientLightIntensity = 1.0f;
  internal.lights = ldkArrayCreate(sizeof(LDKLight), 32);
  success &= internal.renderObjectArray != NULL;

  //
  // Picking Framebuffer setup
  //
  LDKSize viewport = ldkGraphicsViewportSizeGet();
  internalPickingFBOCreate(viewport.width, viewport.height);
  internalGBufferCreate(viewport.width, viewport.height);
  internal.shaderPicking = ldkAssetLookup(LDKShader, internal.hShaderPicking);
  internal.shaderHighlight = ldkAssetLookup(LDKShader, internal.hShaderHighlight);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return success;
}

void ldkRendererResize(uint32 width, uint32 height)
{
  glViewport(0, 0, width, height);
  internalPickingFBODEstroy();
  internalPickingFBOCreate(width, height);

  internalGBufferDestroy();
  internalGBufferCreate(width, height);
}

void ldkRendererTerminate(void)
{
  if (!internal.initialized)
    return;

  internal.initialized = false;
  ldkArrayDestroy(internal.renderObjectArray);
  ldkArrayDestroy(internal.renderObjectArrayDeferred);
  internalPickingFBODEstroy();
  internalGBufferDestroy();
}

void ldkRendererSetClearColor(LDKRGB color)
{
  internal.clearColor = vec3( color.r * 255.0f, color.g * 255.0f, color.b * 255.0f);
}

void ldkRendererSetClearColorVec3(Vec3 color)
{
  internal.clearColor = color;
}

void ldkRendererSetCamera(LDKCamera* camera)
{
  if (camera->entity.active == FALSE)
  {
    ldkLogError("Using disabled camera for rendering!");
    internal.camera = NULL;
    return;
  }

  internal.camera = camera;
}

LDKCamera* ldkRendererGetCamera()
{
  return internal.camera;
}

void ldkRendererAddPointLight(LDKPointLight* entity)
{
  if (entity->entity.active == false)
    return;

  LDKLight light;
  light.position = vec4(
      entity->position.x,
      entity->position.y,
      entity->position.z,
      1.000f);
  light.colorDiffuse = entity->colorDiffuse;
  light.colorSpecular = entity->colorSpecular;
  light.linear = entity->linear;
  light.quadratic = entity->quadratic;
  ldkArrayAdd(internal.lights, &light);
}

void ldkRendererAddDirectionalLight(LDKDirectionalLight* entity)
{
  if (entity->entity.active == false)
    return;

  LDKLight light;
  light.direction = vec4(
      entity->position.x,
      entity->position.y,
      entity->position.z,
      0.00000f);
  light.colorDiffuse = entity->colorDiffuse;
  light.colorSpecular = entity->colorSpecular;
  ldkArrayAdd(internal.lights, &light);
}

void ldkRendererAddStaticObject(LDKStaticObject* entity)
{
  if (entity->entity.active == false)
    return;

  LDKMesh* mesh = ldkAssetLookup(LDKMesh, entity->mesh);
  if (mesh == NULL)
    return;

  LDKHandle* materials = entity->materials ? entity->materials : mesh->materials;
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
      ldkArrayAdd(internal.renderObjectArrayDeferred, &ro);
      break;
    }
    else
    {
      ldkArrayAdd(internal.renderObjectArray, &ro);
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

  LDKHandle* materials =  mesh->materials;
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
      ldkArrayAdd(internal.renderObjectArrayDeferred, &ro);
      break;
    }
    else
    {
      ldkArrayAdd(internal.renderObjectArray, &ro);
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

void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color1, LDKRGB color2)
{
  GLfloat vertices[] = {
    x1, y1, z1,
    x2, y2, z2
  };

  GLfloat colors[] = {
    color1.r, color1.g, color1.b,
    color2.r, color2.g, color2.b
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

void drawCircle(float x, float y, float z, float radius, float thickness, float angle)
{
  int numSegments = 60; // Number of line segments to approximate the circle
  float theta = (float) (fabs(angle) / numSegments); // Angle between each segment
  float start_angle = (angle >= 0) ? 0.0f : (float) (2 * M_PI - fabs(angle));
  float halfThickness = thickness / 2.0f;

  // Calculate the number of vertices needed
  uint32 numVertices = (uint32) (ceil(fabs(angle) / theta) * 2);

  // If angle is less than 360 degrees, we need to add 2 extra vertices to close the circle
  if (fabs(angle) < 2 * M_PI)
    numVertices += 2;

  // Generate and bind VAO and VBO
  GLuint VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  // Calculate vertices for the main segments
  GLfloat vertices[ 255 * 3]; // 3 components (x, y, z) per vertex
  for (uint32 i = 0; i < numVertices / 2; i++) {
    float currentAngle = start_angle + i * theta;
    // Inner vertex of the current segment
    vertices[i * 6] = (float) (x + (radius - halfThickness) * cos(currentAngle));
    vertices[i * 6 + 1] =  (float) ((y + (radius - halfThickness) * sin(currentAngle)));
    vertices[i * 6 + 2] = z;

    // Outer vertex of the current segment
    vertices[i * 6 + 3] = (float) (x + (radius + halfThickness) * cos(currentAngle));
    vertices[i * 6 + 4] = (float) (y + (radius + halfThickness) * sin(currentAngle));
    vertices[i * 6 + 5] = z;
  }

  // If angle is less than 360 degrees, add vertices to close the circle
  if (fabs(angle) > 2 * M_PI) {
    float last_angle = (float) (start_angle + fabs(angle));

    // Inner vertex of the last segment
    vertices[numVertices * 3 - 6] = (float) (x + (radius - halfThickness) * cos(last_angle));
    vertices[numVertices * 3 - 5] = (float) (y + (radius - halfThickness) * sin(last_angle));
    vertices[numVertices * 3 - 4] = z;

    // Outer vertex of the last segment
    vertices[numVertices * 3 - 3] = (float) (x + (radius + halfThickness) * cos(last_angle));
    vertices[numVertices * 3 - 2] = (float) (y + (radius + halfThickness) * sin(last_angle));
    vertices[numVertices * 3 - 1] = z;
  }

  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Set vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // Unbind VBO and VAO
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Use shader program and draw
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices);
  glBindVertexArray(0);
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

  internal.elapsedTime += deltaTime;

  if (internal.camera == NULL)
    return;

  Mat4 cameraViewMatrix = ldkCameraViewMatrix(internal.camera);
  Mat4 cameraProjMatrix = ldkCameraProjectMatrix(internal.camera);

  glBindFramebuffer(GL_FRAMEBUFFER, internal.gBuffer);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Deferred path
  internalGeometryPass(internal.renderObjectArrayDeferred);
  internalLightPass(internal.lights);

  // Forward path
  internalGeometryPass(internal.renderObjectArray);

  if (ldkEngineIsEditorRunning())
  {
    ldkShaderProgramBind(internal.shaderLightBox);
    ldkShaderParamSetMat4(internal.shaderLightBox, "projection", cameraProjMatrix);
    ldkShaderParamSetMat4(internal.shaderLightBox, "view", cameraViewMatrix);

    uint32 count = ldkArrayCount(internal.lights);
    LDKLight* light = (LDKLight*) ldkArrayGetData(internal.lights);
    for (unsigned int i = 0; i < count; i++)
    {
      Vec3 p = vec3(light->position.x, light->position.y, light->position.z);
      Mat4 m = mat4World(p, vec3(0.1f, 0.1f, 0.1f), quatId());
      ldkShaderParamSetMat4(internal.shaderLightBox, "model", m);
      ldkShaderParamSetVec3(internal.shaderLightBox, "lightColor", light->colorDiffuse);
      InternalRenderCubeNDC();
      light++;
    }

    // Picking and selection
    internalRenderPickingBuffer(internal.renderObjectArrayDeferred);
#if 1
    // TESTING STUFF!!!!

    Mat4 identity = mat4Id();
    LDKShader* shader = ldkAssetGet(LDKShader, "assets/default_vertex_color.shader");
    ldkShaderProgramBind(shader);
    ldkShaderParamSetMat4(shader, "mModel", identity);
    ldkShaderParamSetMat4(shader, "mView", cameraViewMatrix);
    ldkShaderParamSetMat4(shader, "mProj", cameraProjMatrix);
    ldkShaderParamSetVec3(shader, "color", vec3(1.0f, 1.0f, 1.0f));

    drawLine(0.0f, 0.0f, 2.0f, 10.0f, 10.0f, -2.0f, 0.2f, ldkRGB(255, 0, 0), ldkRGB(255, 255, 0));
    drawCircle(0.0f, 0.0f, 0.0f,
        2.0f, // radius
        0.2f, // thinkness
        (float) degToRadian(360.0));

    drawCircle(10.0f, 0.0f, -5.0f,
        2.0f, // radius
        2.8f, // thinkness
        (float) degToRadian(360.0));

    LDKHListIterator it = ldkEntityManagerGetIterator(LDKCamera);

    // Camera gizmos
    while (ldkHListIteratorNext(&it))
    {
      LDKCamera* e = (LDKCamera*) it.ptr;
      if ((e->entity.flags & LDK_ENTITY_FLAG_INTERNAL) == LDK_ENTITY_FLAG_INTERNAL)
        continue;

      Mat4 world = mat4World(e->position, vec3One(), quatInverse(mat4ToQuat(ldkCameraViewMatrix(e))));
      ldkShaderParamSetMat4(shader, "mModel", world);
      drawFrustum(0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1.5f, ldkRGB(255, 0, 0));
    }

#endif
  }

  ldkArrayClear(internal.renderObjectArray);
  ldkArrayClear(internal.renderObjectArrayDeferred);
  ldkArrayClear(internal.lights);
  ldkMaterialBind(0);
  ldkRenderBufferBind(NULL);
}

LDKHandle ldkRendererSelectedEntity(void)
{
  return internal.selectedEntity;
}