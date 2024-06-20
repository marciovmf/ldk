
//
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

#define LDK_VERTEX_ATTRIBUTE_POSITION layout (location = 0) in

LDK_VERTEX_ATTRIBUTE_POSITION  vec3 vPosition;

uniform mat4 mModel;
uniform mat4 mView;
uniform mat4 mProj;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(vPosition, 1.0);
}
#endif


//
// Fragment Shader
//

#ifdef LDK_COMPILE_FRAGMENT_SHADER

uniform uint surfaceIndex;
uniform uint objectIndex;

out vec3 fragColor;

void main()
{
  fragColor = vec3(float(objectIndex), float(surfaceIndex), float(gl_PrimitiveID + 1));
}

#endif

