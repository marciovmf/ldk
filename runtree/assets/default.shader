
//
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

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

