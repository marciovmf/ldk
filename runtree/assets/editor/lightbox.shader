//
// Vertex Shader
//

#ifdef LDK_COMPILE_VERTEX_SHADER

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 mProj;
uniform mat4 mView;
uniform mat4 mModel;

void main()
{
  gl_Position = mProj * mView * mModel * vec4(aPos, 1.0);
}
#endif


//
// Fragment Shader
//

#ifdef LDK_COMPILE_FRAGMENT_SHADER

layout (location = 0) out vec4 FragColor;

uniform vec3 lightColor;

void main()
{           
  //FragColor = vec4(lightColor, 1.0);
  FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}
#endif

