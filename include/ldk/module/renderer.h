/**
 *
 * render.h
 * 
 * High-level renderer interface
 */

#ifndef LDK_RENDER_H
#define LDK_RENDER_H

#include "../entity/camera.h"
#include "../entity/pointlight.h"
#include "../entity/directionallight.h"
#include "../entity/staticobject.h"
#include "../entity/instancedobject.h"
#include "../asset/config.h"

//
// Rendering
//

LDK_API bool ldkRendererInitialize(LDKConfig* config);
LDK_API void ldkRendererResize(uint32 width, uint32 height);
LDK_API void ldkRendererTerminate(void);

LDK_API void ldkRendererSetCamera(LDKCamera* camera);
LDK_API LDKCamera* ldkRendererGetCamera();
LDK_API void ldkRendererSetClearColor(LDKRGB color);
LDK_API void ldkRendererSetClearColorVec3(Vec3 color);

LDK_API void ldkRendererAddStaticObject(LDKStaticObject* entity);
LDK_API void ldkRendererAddInstancedObject(LDKInstancedObject* entity);
LDK_API void ldkRendererAddPointLight(LDKPointLight* entity);
LDK_API void ldkRendererAddDirectionalLight(LDKDirectionalLight* entity);
LDK_API void ldkRendererRender(float deltaTime);

//
// Ambient light settings
//

LDK_API void ldkAmbientLightSetIntensity(float intensity);
LDK_API float ldkAmbientLightGetIntensity();
LDK_API void ldkAmbientLightSetColor(Vec3 color);
LDK_API Vec3 ldkAmbientLightGetColor();

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

