#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec2 out_tex_coord;

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
	Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];
	mat4 pose = PushConstants.instance_buffer.instances[gl_InstanceIndex].pose;
	out_position = vec3(pose * vec4(v.position,1.0));
	gl_Position = PushConstants.camera_matrix * vec4(out_position, 1.0);
	out_tex_coord = v.tex_coord;

}