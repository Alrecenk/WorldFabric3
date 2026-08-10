#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec4 out_color;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out vec2 out_tex_coord;

struct Vertex {
	vec3 position;
	vec3 normal;
	vec4 color;
	vec2 tex_coord;
	ivec4 joints;
	vec4 weights;
}; 

struct Instance {
	mat4 root;
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

void main() 
{	
	Vertex v = PushConstants.vertex_buffer.vertices[gl_VertexIndex];
	
	InstanceBuffer ib = PushConstants.instance_buffer;
	mat4 root = ib.instances[gl_InstanceIndex].root ;


	out_position = (root * vec4(v.position, 1.0)).xyz;
	gl_Position = PushConstants.camera_matrix * vec4(out_position, 1.0);
	out_normal = normalize((root * vec4(v.normal, 0.0)).xyz);
	out_tex_coord = v.tex_coord;
	out_color = v.color ;

}