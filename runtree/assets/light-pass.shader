
#ifdef LDK_COMPILE_VETEX_SHADER

// ================================
// Vertex Shader
// ================================

LDK_VERTEX_ATTRIBUTE0 vec3 aPos;
LDK_VERTEX_ATTRIBUTE1 vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
  TexCoords = aTexCoords;
  gl_Position = vec4(aPos, 1.0);
}

#endif

#ifdef LDK_COMPILE_FRAGMENT_SHADER

// ================================
// Fragment Shader
// ================================

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct Light {
  vec4 position;
  vec3 colorDiffuse;
  vec3 colorSpecular;
  float linear;
  float quadratic;
  float range;
};

const int MAX_LIGHTS = 64;
uniform int numLights;
uniform Light lights[MAX_LIGHTS];
uniform vec3 viewPos;
uniform vec3 clearColor;
uniform vec3 ambientColor;
uniform float ambientIntensity;

vec3 calcDirLight(Light light, vec3 normal, vec3 viewDir)
{
  vec3 lightDir = normalize(-light.position.xyz);
  // diffuse 
  float diff = max(dot(normal, lightDir), 0.0);
  // specular
  vec3 reflectDir = reflect(-lightDir, normal);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 2.0);
  // combine results
  //vec3 ambient  = texture(gAlbedoSpec, TexCoords).rgb;
  vec3 diffuse  = light.colorDiffuse * diff * texture(gAlbedoSpec, TexCoords).rgb;
  vec3 specular = light.colorSpecular * spec * texture(gAlbedoSpec, TexCoords).a;
  return (diffuse + specular);
} 

vec3 calcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
  vec3 lightDir = normalize(light.position.xyz - fragPos);
  // diffuse
  float diff = max(dot(normal, lightDir), 0.0);
  // specular
  vec3 reflectDir = reflect(-lightDir, normal);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 1.7);
  // attenuation
  float distance    = length(light.position.xyz - fragPos);
  float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * (distance * distance));    

  // combine results
  //vec3 ambient  = light.color * texture(gAlbedoSpec, TexCoords).rgb * ambientIntensity;
  vec3 diffuse  = light.colorDiffuse * diff * texture(gAlbedoSpec, TexCoords).rgb;
  vec3 specular = light.colorSpecular * spec * texture(gAlbedoSpec, TexCoords).a;
  //ambient  *= attenuation;
  diffuse  *= attenuation;
  specular *= attenuation;
  return (diffuse + specular);
} 

void main()
{
  // Set background color if depth is zero
  vec3 Normal = texture(gNormal, TexCoords).rgb;
  if (Normal == vec3(0))
  {
    FragColor = vec4(clearColor,1);
    return;
  }

  vec3 FragPos = texture(gPosition, TexCoords).rgb;
  vec3 viewDir  = normalize(viewPos - FragPos);

  // How much can we see without lights ?
  vec3 ambientLight = (texture(gAlbedoSpec, TexCoords).rgb + ambientColor * ambientIntensity)  * ambientIntensity;
  vec3 lighting = ambientLight;

  for(int i = 0; i < numLights; ++i)
  {
    if (lights[i].position.w == 0.00)
    {
      // Directional Light
      lighting += calcDirLight(lights[i], Normal, viewDir);
    }
    else
    {
      // Point light
      float distance = length(lights[i].position.xyz - FragPos);
      if(distance >= lights[i].range)
        continue;
      lighting += calcPointLight(lights[i], Normal, FragPos, viewDir);
    }
  }
  FragColor = vec4(lighting, 1.0);
}

#endif

