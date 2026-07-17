#version 450
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;
//layout (location = 2) in vec3 in_camera_position ;
layout (location = 3) in mat4 in_pose;
//layout (location = 7) in mat4 in_world_matrix;


//output write
layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_normal; // must have the same outputs as post processor to run on the same render targets
layout (location = 2) out vec4 out_point;
layout (location = 3) out vec4 out_final_image;


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

	 vec3 X = vec3(in_pose[0][0],in_pose[0][1],in_pose[0][2]);
    vec3 Y = vec3(in_pose[1][0],in_pose[1][1],in_pose[1][2]);
    vec3 Z = vec3(in_pose[2][0],in_pose[2][1],in_pose[2][2]);
    vec3 C = vec3(in_pose[3][0],in_pose[3][1],in_pose[3][2]);


    vec3 P = PushConstants.camera_position - C ; // origin of ray relative to center of ellipsoid
    vec3 V = in_position - PushConstants.camera_position ; // direction of ray

    float PX = dot(P,X) ;
    float PY = dot(P,Y) ;
    float PZ = dot(P,Z) ;

    float VX = dot(V,X) ;
    float VY = dot(V,Y) ;
    float VZ = dot(V,Z) ;

    float XX = dot(X,X);
    float YY = dot(Y,Y);
    float ZZ = dot(Z,Z);

    float X4 = XX*XX ;
    float Y4 = YY*YY ;
    float Z4 = ZZ*ZZ ;

    // build quadratic formula components
 
    float a = VX*VX / X4 + VY*VY / Y4 + VZ*VZ / Z4 ;
    float b = 2.0f * (PX*VX/X4 + PY*VY/Y4 + PZ*VZ/Z4);
    float c = PX*PX/X4 + PY*PY/Y4 + PZ*PZ/Z4 - 1.0f ;

    if(b*b < 4*a*c){
        discard ;
    }else{
        out_color = in_color ;
		out_normal = vec4(0,0,0,0);
		out_point = vec4(0,0,0,0); // 0 alpha means these will be ignored if in blend mode
		out_final_image = in_color ;
		
        // compute position of intersection
        float s = (-b - sqrt(b*b - 4*a*c))/(2*a);
        vec3 p = PushConstants.camera_position + V*s ;
        // Compute clips space location
        vec4 clip_p = PushConstants.camera_matrix * vec4(p, 1.0);
        float depth = clip_p.z / clip_p.w;
        if(isnan(depth)){
            discard ;
        }
        // apply standard openGL depth calculation
        //gl_FragDepth = ((gl_DepthRange.diff * depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
		gl_FragDepth = depth ;
		
    }

}