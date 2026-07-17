#version 410 core
uniform sampler2D mytexture;
in vec2 v2UVcoords;
out vec4 outputColor;
void main()
{
   outputColor = texture(mytexture, v2UVcoords);
}