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

#define LDK_VERTEX_ATTRIBUTE_POSITION layout (location = 0) in

LDK_VERTEX_ATTRIBUTE_POSITION  vec3 vPosition;


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

uniform vec3 color;
uniform float deltaTime;
out vec4 fragColor;

void main()
{
  float a = abs(sin(deltaTime));
  fragColor = mix(
      vec4(1.0, 1.0, 1.0, 1.0),
      vec4(color, 1.0), 
       a);
}
#endif

