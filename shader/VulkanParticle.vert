#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec4 out_color;
//layout (location = 2) out vec3 out_camera_position; // TODO kind of innefficient to interpolate things that don't change
layout (location = 3) out mat4 out_pose;
//layout (location = 7) out mat4 out_world_matrix;

struct Vertex {
	vec3 position;
}; 

struct Instance {
	mat4 pose; // pose of the particle ellipse
	vec4 color;
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
	vec3 camera_position ;
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
	
} PushConstants;

void main() {	

	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	Instance i = PushConstants.instanceBuffer.instances[gl_InstanceIndex];
	//vec4 cam = inverse(PushConstants.camera_matrix) * vec4(0,0,0,1);
	//vec3 viewer = cam.xyz / cam.w;

	vec3 X = vec3(i.pose[0][0], i.pose[0][1], i.pose[0][2]);
	vec3 Y = vec3(i.pose[1][0], i.pose[1][1], i.pose[1][2]);
	vec3 Z = vec3(i.pose[2][0], i.pose[2][1], i.pose[2][2]);
	vec3 C = vec3(i.pose[3][0], i.pose[3][1], i.pose[3][2]);

	vec3 to_particle = C - PushConstants.camera_position;
	float distance = length(to_particle) ;
	float max_axis = 1.2f*max(max(length(X), length(Y)), length(Z));
	vec3 quad_Z = to_particle / distance;
	vec3 quad_Y = vec3(0.05f, 1.0f, 0.1f);
	quad_Y = quad_Y - quad_Z * dot(quad_Z, quad_Y);
	vec3 quad_X = cross(quad_Y, quad_Z);
	quad_Y *= max_axis / length(quad_Y);
	quad_X *= max_axis / length(quad_X);

	mat4 quad_pose = {{ quad_X.x, quad_X.y, quad_X.z, 0},
						{quad_Y.x, quad_Y.y, quad_Y.z, 0},
						{quad_Z.x, quad_Z.y, quad_Z.z, 0},
						{C.x, C.y, C.z, 1} };


	
	//out_camera_position = PushConstants.camera_position ;

	out_position = vec3(quad_pose * vec4(v.position, 1));
	out_color = i.color ;
	out_pose = i.pose ;

	
	//out_world_matrix = PushConstants.camera_matrix ;
	gl_Position = PushConstants.camera_matrix * vec4(out_position, 1.0);

	
}