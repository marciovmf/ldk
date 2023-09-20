#ifndef RENDER_H
#define RENDER_H

#include "../common.h"
#include "../maths.h"

typedef LDKHandle LDKHShader;

enum
{
  LDK_SHADER_MAX_PROPERTIES = 16,
  LDK_SHADER_PROPERTY_NAME_LENGTH = 64
};

LDK_API bool ldkRenderInitialize();
LDK_API void ldkRenderTerminate();
LDK_API LDKHShader ldkShaderProgramCreate();
LDK_API bool ldkShaderProgramDestroy(LDKHShader handle);
LDK_API bool ldkShaderSourceFragmentShader(LDKHShader handle, const char* source);
LDK_API bool ldkShaderSourceVertexShader(LDKHShader handle, const char* source);
LDK_API bool ldkShaderSourceGeometryShader(LDKHShader handle, const char* source);
LDK_API bool ldShaderProgramLink(LDKHShader handle);

LDK_API bool ldkShaderPropertySetUInt(LDKHShader handle, const char* name, uint32 value);
LDK_API bool ldkShaderPropertySetInt(LDKHShader handle, const char* name, int32 value);
LDK_API bool ldkShaderPropertySetFloat(LDKHShader handle, const char* name, float value);
LDK_API bool ldkShaderPropertySetVec2(LDKHShader handle, const char* name, Vec2 value);
LDK_API bool ldkShaderPropertySetVec3(LDKHShader handle, const char* name, Vec3 value);
LDK_API bool ldkShaderPropertySetVec4(LDKHShader handle, const char* name, Vec4 value);
LDK_API bool ldkShaderPropertySetMat4(LDKHShader handle, const char* name, Mat4 value);
//LDK_API bool ldkShaderPropertySampler2D(LDKHShader handle, const char* name, LDKHTexture value);

LDK_API bool ldkShaderBind(LDKHShader handle);
LDK_API void ldkShadeUnbind(LDKHShader handle);

#endif //RENDER_H

