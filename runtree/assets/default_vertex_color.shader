//
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

LDK_VERTEX_ATTRIBUTE0 vec3 vPosition;
LDK_VERTEX_ATTRIBUTE1 vec3 vColor;

uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mModel;

out vec3 color;

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

out vec4 fragColor;
in vec3 color;

void main()
{
  fragColor = vec4(color, 1.0);
}
#endif

