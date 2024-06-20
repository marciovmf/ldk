//
// Vertex Shader
//


#ifdef LDK_COMPILE_VERTEX_SHADER

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

uniform vec3 color1;
uniform vec3 color2;
uniform float deltaTime;
out vec4 fragColor;

void main()
{
  float a = abs(sin(deltaTime));
  fragColor = mix(
      vec4(color2, 1.0),
      vec4(color1, 1.0), 
      a);
}
#endif

