#ifdef LDK_COMPILE_VERTEX_SHADER
// ================================
// Vertex Shader
// ================================


LDK_VERTEX_ATTRIBUTE_POSITION   vec3 aPos;
LDK_VERTEX_ATTRIBUTE_NORMAL     vec3 aNormal;
LDK_VERTEX_ATTRIBUTE_TEXCOORD0  vec2 aTexCoords;
LDK_VERTEX_ATTRIBUTE_MATRIX0    mat4 mModelInstanced;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 mModel;
uniform mat4 mView;
uniform mat4 mProj;
uniform bool instanced;

void main()
{
  mat4 model = instanced ? mModelInstanced : mModel;
  vec4 worldPos = model * vec4(aPos, 1.0);
  FragPos = worldPos.xyz; 
  TexCoords = aTexCoords;

  mat3 normalMatrix = transpose(inverse(mat3(mModel)));
  Normal = normalMatrix * aNormal;
  gl_Position = mProj * mView * worldPos;
}

#endif

#ifdef LDK_COMPILE_FRAGMENT_SHADER
// ================================
// Fragment Shader
// ================================

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec3 FragPos;
in vec2 TexCoords;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform float shininess;

void main()
{    
  gPosition = FragPos;
  gNormal = normalize(Normal);
  gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
  gAlbedoSpec.a = texture(texture_specular1, TexCoords).r * shininess;
}

#endif

