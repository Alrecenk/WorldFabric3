#version 450

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_tex_coord;

//output write
layout (location = 0) out vec4 out_frag_color;
layout (location = 1) out vec4 out_frag_normal;
layout (location = 2) out vec4 out_frag_point;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

void main() 
{
	vec4 tex_color = texture(color_texture, in_tex_coord) * in_color;
	if(tex_color.a < 0.5){
		discard ;
	}
	out_frag_color = tex_color ;
	out_frag_normal = vec4(in_normal,1.0) ;
	out_frag_point = vec4(in_position,1.0) ; 

}