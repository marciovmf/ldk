#ifndef RENDER_H
#define RENDER_H

#include "../common.h"
#include "../maths.h"

typedef LDKHandle LDKHMaterial;
typedef LDKHandle LDKHShader;
typedef LDKHandle LDKHShaderProgram;

enum
{
  LDK_SHADER_MAX_PARAM = 16,
};

LDK_API bool ldkRenderInitialize();
LDK_API void ldkRenderTerminate();
LDK_API LDKHShaderProgram ldkShaderProgramCreate(LDKHShader hVertex, LDKHShader hFragment, LDKHShader hGeometry);
LDK_API bool ldkShaderDestroy(LDKHShader hShader);
LDK_API bool ldkShaderProgramDestroy(LDKHShaderProgram hShader);
LDK_API LDKHShader ldkFragmentShaderCreate(const char* source);
LDK_API LDKHShader ldkVertexShaderCreate(const char* source);
LDK_API LDKHShader ldkGeometryShaderCreate(const char* source);


//
// Shader
//
LDK_API bool ldkShaderParamSetUInt(LDKHShader handle, const char* name, uint32 value);
LDK_API bool ldkShaderParamSetInt(LDKHShader handle, const char* name, int32 value);
LDK_API bool ldkShaderParamSetFloat(LDKHShader handle, const char* name, float value);
LDK_API bool ldkShaderParamSetVec2(LDKHShader handle, const char* name, Vec2 value);
LDK_API bool ldkShaderParamSetVec3(LDKHShader handle, const char* name, Vec3 value);
LDK_API bool ldkShaderParamSetVec4(LDKHShader handle, const char* name, Vec4 value);
LDK_API bool ldkShaderParamSetMat4(LDKHShader handle, const char* name, Mat4 value);
//LDK_API bool ldkShaderParam(LDKHShader handle, const char* name, LDKHTexture value);
LDK_API bool ldkShaderBind(LDKHShader handle);
LDK_API void ldkShadeUnbind(LDKHShader handle);


//
// Material
//
LDK_API LDKHMaterial ldkMaterialCreate(LDKHShaderProgram hProgram);
LDK_API bool ldkMaterialDestroy(LDKHMaterial hMaterial);
LDK_API bool ldkMaterialParamSetInt(LDKHMaterial hMaterial, const char* name, int value);
LDK_API bool ldkMaterialParamSetFloat(LDKHMaterial hMaterial, const char* name, float value);
LDK_API bool ldkMaterialParamSetVec2(LDKHMaterial hMaterial, const char* name, Vec2 value );
LDK_API bool ldkMaterialParamSetVec3(LDKHMaterial hMaterial, const char* name, Vec3 value );
LDK_API bool ldkMaterialParamSetVec4(LDKHMaterial hMaterial, const char* name, Vec4 value );

#endif //RENDER_H

