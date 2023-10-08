#version 330 core

#define LDK_VERTEX_ATTRIBUTE_POSITION layout (location = 0) in
#define LDK_VERTEX_ATTRIBUTE_NORMAL   layout (location = 1) in
#define LDK_VERTEX_ATTRIBUTE_TANGENT  layout (location = 2) in
#define LDK_VERTEX_ATTRIBUTE_BINORMAL layout (location = 3) in
#define LDK_VERTEX_ATTRIBUTE_TEXCOORD layout (location = 4) in

LDK_VERTEX_ATTRIBUTE_POSITION  vec3 vPosition;
LDK_VERTEX_ATTRIBUTE_NORMAL    vec3 vNormal;
LDK_VERTEX_ATTRIBUTE_TEXCOORD  vec2 vTexcoord;

out vec2 fragTexCoord;

void main()
{
  gl_Position = vec4(vPosition, 1.0);
  fragTexCoord = vTexcoord;
}
