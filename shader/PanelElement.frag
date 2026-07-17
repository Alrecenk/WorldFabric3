#version 450
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_tex_coord;


layout (location = 2) in mat4 pose;
layout (location = 6) in vec4 bg_color_1;
layout (location = 7) in vec4 bg_color_2;
layout (location = 8) in vec4 alpha_border_color;
layout (location = 9) in vec4 box_border_color;
layout (location = 10) in float alpha_border_width ;
layout (location = 11) in float box_border_width ;

//output write
layout (location = 0) out vec4 panel_color;

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

float getAlpha(vec2 n, float w, float h){
	n.x/=w;
	n.y/=h;
	return texture(color_texture, n).a;
}

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
	vec3 X = vec3(pose * vec4(1,0,0,0)) ;
	vec3 Y = vec3(pose * vec4(0,1,0,0)) ;
	float w = length(X);
	float h = length(Y);
	float x = w * in_tex_coord.x;
	float y = h * in_tex_coord.y;
	float t = 0.25*(in_tex_coord.x *3.0+  in_tex_coord.y);
	vec4 bg_color = t * bg_color_1 + (1-t) * bg_color_2 ;
	float a = tex_color.a ;
	if(x < box_border_width || y < box_border_width || x > w-box_border_width || y > h-box_border_width){
		panel_color = box_border_color ;
	}else{
		
		if(a > 0.1){
			panel_color = bg_color * (1-a) + tex_color * a ;

		}else{
			float n1a = getAlpha(vec2(x+alpha_border_width, y), w, h) ;
			float n2a = getAlpha(vec2(x-alpha_border_width, y), w, h) ;
			float n3a = getAlpha(vec2(x, y+alpha_border_width), w, h) ;
			float n4a = getAlpha(vec2(x, y-alpha_border_width), w, h) ;
			if(n1a > 0.5 || n2a > 0.5 || n3a > 0.5 || n4a > 0.5){
				panel_color = alpha_border_color * (1-a) + tex_color * a ;
			}else{
				panel_color = bg_color * (1-a) + tex_color * a ;
			}
		}
	}
}