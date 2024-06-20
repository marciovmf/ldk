/*
 *  A shader file contains source code for VERTEX, FRAGMENT and GEOMETRY stages.
 *  each stage source must be enclosed in #ifdef /#endif directives as follows:
 *
 *  #ifdef LDK_COMPILE_VERTEX_SHADER
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
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

LDK_VERTEX_ATTRIBUTE0 vec3 vPosition;

uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mModel;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(vPosition, 1.0);
}
#endif


//
// Fragment Shader
//
#ifdef LDK_COMPILE_FRAGMENT_SHADER

out vec4 fragColor;
uniform vec3 color;

void main()
{
  fragColor = vec4(color, 1.0);
}
#endif

