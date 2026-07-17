#version 450
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_tex_coord;

//output write
layout (location = 4) out vec4 panel_color; // location is from ui image index on render targets

layout(set = 0, binding = 0) uniform sampler2D color_texture;


struct Vertex {
	vec3 position;
	vec2 tex_coord;
}; 

struct Instance {
	mat4 pose ;
	vec4 bg_color_1;
	vec4 bg_color_2;
	vec4 alpha_border_color;
	vec4 box_border_color;
	float alpha_border_width;
	float box_border_width;
}; 


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer{ 
	Instance instances[];
};

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 camera_matrix;
	vec3 camera_position;
	VertexBuffer vertex_buffer;
	InstanceBuffer instance_buffer;
} PushConstants;

void main() {
	vec4 tex_color = texture(color_texture, in_tex_coord);
	panel_color = tex_color ;
}