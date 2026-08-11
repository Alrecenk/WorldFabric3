#version 450
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_tex_coord;

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
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer{ 
	Instance instances[];
};

struct Fragment {
    uint  color;
	float depth;
};

layout(buffer_reference, std430) buffer FragmentBuffer {
    Fragment frame[];
};

layout(buffer_reference, std430) buffer CountBuffer {
    uint counts[];
};

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 camera_matrix;
	vec3 camera_position;
	VertexBuffer vertex_buffer;
	InstanceBuffer instance_buffer;
	FragmentBuffer fragment_buffer;
	CountBuffer count_buffer ;
	int frame_width;
	int frame_height;
	int fragments;
} PushConstants;


void main() 
{
	vec4 tex_color = texture(color_texture, in_tex_coord) * in_color;

	vec3 to_viewer  = normalize(PushConstants.camera_position - in_position) ;
	float brightness = dot(to_viewer, normalize(in_normal)) ;
	tex_color = vec4(tex_color.r*brightness, tex_color.g*brightness,tex_color.b*brightness,tex_color.a) ;

	ivec2 pixel = ivec2(gl_FragCoord.xy);
    uint pixel_index = pixel.y * PushConstants.frame_width + pixel.x ;

	FragmentBuffer fb = PushConstants.fragment_buffer;
	CountBuffer cb = PushConstants.count_buffer;

	uint frag_index= atomicAdd(cb.counts[pixel_index], 1);
    if (frag_index < PushConstants.fragments) {
        uint index = pixel_index * PushConstants.fragments + frag_index;
        fb.frame[index].depth = gl_FragCoord.z;
        fb.frame[index].color = packUnorm4x8(tex_color);
    }

}