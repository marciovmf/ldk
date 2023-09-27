/**
 *
 * render.h
 * 
 * High level rendering API for the engine
 * 
 */

#ifndef LDK_RENDER_H
#define LDK_RENDER_H

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

//
// Shader
//
LDK_API LDKHShaderProgram ldkShaderProgramCreate(LDKHShader handleVs, LDKHShader handleFs, LDKHShader handleGs);
LDK_API LDKHShader ldkFragmentShaderCreate(const char* source);
LDK_API LDKHShader ldkVertexShaderCreate(const char* source);
LDK_API LDKHShader ldkGeometryShaderCreate(const char* source);
LDK_API bool ldkShaderDestroy(LDKHShader handle);
LDK_API bool ldkShaderProgramDestroy(LDKHShaderProgram handle);
LDK_API bool ldkShaderBind(LDKHShader handle);
LDK_API void ldkShaderUnbind(LDKHShader handle);

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
LDK_API bool ldkMaterialBind(LDKHMaterial handle);
LDK_API void ldkMaterialUnbind(LDKHMaterial handle);

#endif //LDK_RENDER_H

