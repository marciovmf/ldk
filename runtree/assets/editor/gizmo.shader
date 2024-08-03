//
// Vertex Shader
//
#ifdef LDK_COMPILE_VERTEX_SHADER

LDK_VERTEX_ATTRIBUTE_POSITION   vec3 aPos;
LDK_VERTEX_ATTRIBUTE_NORMAL     vec3 aNormal;
LDK_VERTEX_ATTRIBUTE_TEXCOORD0  vec2 aTexCoords;

uniform mat4 mProj;
uniform mat4 mView;
uniform mat4 mModel;

out vec2 uv;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(aPos, 1.0);
  uv = aTexCoords;
}
#endif


//
// Fragment Shader
//

#ifdef LDK_COMPILE_FRAGMENT_SHADER

uniform sampler2D diffuse;
in vec2 uv;
out vec4 fragColor;

void main()
{           
  fragColor = texture(diffuse, uv);
  fragColor.a = 0.3;
}
#endif

