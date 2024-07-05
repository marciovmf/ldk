
//
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

#define LDK_VERTEX_ATTRIBUTE_POSITION layout (location = 0) in

LDK_VERTEX_ATTRIBUTE_POSITION   vec3 vPosition;
LDK_VERTEX_ATTRIBUTE_MATRIX0    mat4 mModelInstanced;

uniform mat4 mModel;
uniform mat4 mView;
uniform mat4 mProj;
uniform bool instanced;

flat out int instanceId;

void main()
{
  mat4 model = instanced ? mModelInstanced : mModel;
  gl_Position = mProj * mView * model * vec4(vPosition, 1.0);
  instanceId = gl_InstanceID;
}
#endif


//
// Fragment Shader
//

#ifdef LDK_COMPILE_FRAGMENT_SHADER

uniform uint surfaceIndex;
uniform uint objectIndex;

out vec3 fragColor;
flat in int instanceId;

void main()
{
  // If I decide later to use a vec4 for color I can also pass the (gl_PrimititveID +1)
  fragColor = vec3(float(objectIndex), float(surfaceIndex), float(instanceId));
}

#endif

