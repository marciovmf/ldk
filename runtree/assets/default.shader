/*
 *  A shader file contains source code for VERTEX, FRAGMENT and GEOMETRY stages.
 *  each stage source must be enclosed in #ifdef /#endif directives as follows:
 *
 *  #ifdef LDK_COMPILE_VETEX_SHADER
 *  // your vertex shader source goes here
 *  #endif
 *
 *  #ifdef LDK_COMPILE_FRAGMENT_SHADER
 *  // your vertex shader source goes here
 *  #endif
 *
 *  #ifdef LDK_COMPILE_FRAGMENT_SHADER
 *  // your geometry shader source goes here
 *  #endif
 *
 * IMPORTANT: as the geometry stage is optional, if your shader includes a
 * GEOMETRY source, the very first line of the .shader file must be exaclty '//@use-geomety-shader'
 * if this line is missing, no geometry shader will be compiled at all.
 *
 */


//
// Vertes Shader
//

#ifdef LDK_COMPILE_VETEX_SHADER

LDK_VERTEX_ATTRIBUTE_POSITION   vec3 vPosition;
LDK_VERTEX_ATTRIBUTE_NORMAL     vec3 vNormal;
LDK_VERTEX_ATTRIBUTE_TEXCOORD0  vec2 vTexcoord;

out vec2 fragTexCoord;

uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mModel;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(vPosition, 1.0);
  fragTexCoord = vTexcoord;
}
#endif


//
// Fragment Shader
//
#ifdef LDK_COMPILE_FRAGMENT_SHADER

uniform sampler2D colorMap;
uniform vec3 color;
uniform float colorIntensity;

in vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
  fragColor = mix(texture(colorMap, fragTexCoord), vec4(color, 1.0), colorIntensity);
}
#endif

