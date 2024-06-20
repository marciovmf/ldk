
#ifdef LDK_COMPILE_VERTEX_SHADER

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

struct PointLight {
  vec3 position;
  vec3 colorDiffuse;
  vec3 colorSpecular;
  float linear;
  float quadratic;
  float range;
};

struct DirectionalLight {
  vec3 direction;
  vec3 colorDiffuse;
  vec3 colorSpecular;
  float linear;
  float quadratic;
};

struct SpotLight {
  vec3 position;
  vec3 direction;
  vec3 colorDiffuse;
  vec3 colorSpecular;
  float linear;
  float quadratic;
  float cutOffInner;
  float cutOffOuter;
};

const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 32;
const int MAX_DIRECTIONAL_LIGHTS = 32;

uniform int numPointLights;
uniform int numSpotLights;
uniform int numDirectionalLights;

uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform vec3 viewPos;
uniform vec3 clearColor;
uniform vec3 ambientColor;
uniform float ambientIntensity;

float calcAttenuation(float linear, float quadratic, float distance)
{
  float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    
  return attenuation;
}


vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texAlbedo, float texSpecular)
{
  const float shininess = 1.5;
  // diffuse 
  vec3 lightDir = normalize(light.position - fragPos);
  float diff = max(dot(normal, lightDir), 0.0);
  vec3 diffuse  = light.colorDiffuse * diff * texAlbedo;
  // specular
  vec3 reflectDir = reflect(-lightDir, normal);  
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
  vec3 specular = light.colorSpecular * spec * texSpecular;
  // spotlight (soft edges)
  float theta = dot(lightDir, normalize(-light.direction)); 
  float epsilon = (light.cutOffInner - light.cutOffOuter);
  float intensity = clamp((theta - light.cutOffOuter) / epsilon, 0.0, 1.0);
  diffuse  *= intensity;
  specular *= intensity;
  // attenuation
  float attenuation = calcAttenuation(light.linear, light.quadratic, length(light.position - fragPos));
  diffuse  *= attenuation;
  specular *= attenuation;   
  return diffuse + specular;
}


vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texAlbedo, float texSpecular)
{
  const float shininess = 1.5;
  vec3 lightDir = normalize(light.position - fragPos);
  // diffuse
  float diff = max(dot(normal, lightDir), 0.0);
  // specular
  vec3 reflectDir = reflect(-lightDir, normal);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
  // attenuation
  float attenuation = calcAttenuation(light.linear, light.quadratic, length(light.position - fragPos));
  // combine results
  vec3 diffuse  = light.colorDiffuse * diff * texAlbedo;
  vec3 specular = light.colorSpecular * spec * texSpecular;
  diffuse  *= attenuation;
  specular *= attenuation;
  return diffuse + specular;
}


vec3 calcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 texAlbedo, float texSpecular)
{
  const float shininess = 2.0;
  // shouldnt it be normalize(-light.direction) ?
  vec3 lightDir = normalize(light.direction);
  // diffuse 
  float diff = max(dot(normal, lightDir), 0.0);
  // specular
  vec3 reflectDir = reflect(-lightDir, normal);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
  // combine results
  vec3 diffuse  = light.colorDiffuse * diff * texAlbedo;
  vec3 specular = light.colorSpecular * spec * texSpecular;
  return diffuse + specular;
} 


void main()
{
  // Set background color when depth is zero
  vec3 Normal = texture(gNormal, TexCoords).rgb;
  vec4 gColor = texture(gAlbedoSpec, TexCoords);
  vec3 Albedo = gColor.rgb;
  float Specular = gColor.a;

  if (Normal == vec3(0))
  {
    FragColor = vec4(clearColor,1);
    return;
  }

  vec3 FragPos = texture(gPosition, TexCoords).rgb;
  vec3 viewDir  = normalize(viewPos - FragPos);

  // How much can we see without lights ?
  vec3 ambientLight = (texture(gAlbedoSpec, TexCoords).rgb + ambientColor * ambientIntensity) * ambientIntensity;
  vec3 lighting = ambientLight;

  // Directional Lights
  for(int i = 0; i < numDirectionalLights; ++i)
  {
    lighting += calcDirectionalLight(directionalLights[i], Normal, viewDir, Albedo, Specular);
  }

  // Point lights
  for(int i = 0; i < numPointLights; ++i)
  {
    float distance = length(pointLights[i].position - FragPos);
    if(distance < pointLights[i].range)
      lighting += calcPointLight(pointLights[i], Normal, FragPos, viewDir, Albedo, Specular);
  }

  // Spot lights
  for(int i = 0; i < numSpotLights; ++i)
  {
    lighting += calcSpotLight(spotLights[i], Normal, FragPos, viewDir, Albedo, Specular);
  }

  FragColor = vec4(lighting, 1.0);
}

#endif

