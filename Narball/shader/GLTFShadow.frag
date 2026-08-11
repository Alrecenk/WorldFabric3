#version 450
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coord;

//output write
layout (location = 0) out float out_frag_depth;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

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
	mat4[256] bone_pose;
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
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
} PushConstants;

void main() {
	
	vec4 tex_color = texture(color_texture, in_tex_coord);
	if(tex_color.a < 0.5){ // discard transparent fragments
		discard ;
	}
	
	vec4 clip= PushConstants.camera_matrix*vec4(in_position - in_normal*0.02f,1.0f) ;
	out_frag_depth = clip.z/clip.w + 0.0002 ;
}