#version 330 core
layout (location = 0) in vec3 vertPos;
layout (location = 1) in vec2 vertUVIn;
layout (location = 4) in vec4 colorIn;
layout (location = 3) in vec3 normalIn;

uniform float intensity;
out vec4 vertColor; 

void main()
{
  vertColor = colorIn * intensity;
}
