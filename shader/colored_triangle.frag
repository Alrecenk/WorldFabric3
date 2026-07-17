#version 450

//shader input
layout (location = 0) in vec3 inPosition;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inTexCoord;

//output write
layout (location = 0) out vec4 outFragColor;
layout (location = 1) out vec4 outFragNormal;
layout (location = 2) out vec4 outFragPoint;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

void main() 
{
	vec4 tex_color = texture(color_texture, inTexCoord);
	if(tex_color.a < 0.5){
		discard ;
	}
	outFragColor = tex_color ;
	outFragNormal = vec4(inNormal,1.0) ;
	outFragPoint = vec4(inPosition,1.0) ; 

}