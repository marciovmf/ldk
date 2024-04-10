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
LDK_VERTEX_ATTRIBUTE_NORMAL     vec3 vColor; // NORMAL is defined at position 1

out vec3 color;

uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mModel;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(vPosition, 1.0);
  color = vColor;
}
#endif


//
// Fragment Shader
//
#ifdef LDK_COMPILE_FRAGMENT_SHADER

in vec3 color;
out vec4 fragColor;

void main()
{
  fragColor = vec4(color, 1.0);
}
#endif

