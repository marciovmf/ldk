#version 330 core

uniform sampler2D colorMap;
in vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
  fragColor = texture(colorMap, fragTexCoord / 5);
}

