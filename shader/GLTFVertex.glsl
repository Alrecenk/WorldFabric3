#version 430
in vec3 aVertexPosition;
in vec4 aVertexColor;
in vec3 aNormal;
in vec2 aTexcoord;
in vec4 aJoints;
in vec4 aWeights;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform sampler2D bones_texture;
uniform int boneless ;
uniform mat4 u_game_matrix; // matrix moving model point ijto game coordinates (not including camera view)

out vec4 v_color;
out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord;
out vec3 v_game_position ;
out vec3 v_game_normal ;

vec4 getBoneElem(int index){
    int y = index/32;
	int x = index - (y*32) ;
	return texelFetch(bones_texture, ivec2(x, y), 0);
}

mat4 getBone(float index){
    int i = int(round(index));
    mat4 bone ;
    bone[0] = getBoneElem(4*i);
    bone[1] = getBoneElem(4*i+1);
    bone[2] = getBoneElem(4*i+2);
    bone[3] = getBoneElem(4*i+3);
    return bone ;
}

vec3 getPositionByID(int id){
    int tri = id/3;
		int axis = id - tri *3;
		int row = tri/30;
		int col = tri - row*30 ;
    
    vec3 pos = vec3(float(row) * 0.05f - 1.0f, 0.5f, float(col) * 0.05f - 1.0f) ;
    if(axis == 0){
         pos.y = pos.y + 0.04f;
    }
    if(axis== 1){
         pos.z= pos.z +0.04f;
    }
    if(axis == 2){
         pos.y = pos.y -0.04f;
    }
    return pos ;
}

void main(void) {
    v_color = aVertexColor;

    if(boneless == 1){
        v_game_position = aVertexPosition ;
        v_game_normal = aNormal ;
    }else{
        vec4 x = vec4(aVertexPosition, 1) ;

        v_game_position = vec3(getBone(aJoints[0]) * x * aWeights[0] + 
                        getBone(aJoints[1]) * x * aWeights[1] + 
                        getBone(aJoints[2]) * x * aWeights[2] + 
                        getBone(aJoints[3]) * x * aWeights[3] );

        vec4 n = vec4(aNormal,0);

        v_game_normal = vec3(getBone(aJoints[0]) * n * aWeights[0] + 
                        getBone(aJoints[1]) * n * aWeights[1] + 
                        getBone(aJoints[2]) * n * aWeights[2] + 
                        getBone(aJoints[3]) * n * aWeights[3] );

        if(length(v_game_position) < 0.01f){
            v_game_position = aVertexPosition;
            v_color = vec4(1.0,0.0,0.0, 1.0);            
        }

    }    
    
    v_texcoord = aTexcoord;

    gl_Position = uPMatrix * uMVMatrix * vec4(v_game_position, 1.0);

    
    v_position = vec3(uMVMatrix * vec4(v_game_position, 1.0));

    
    v_normal = vec3(uMVMatrix * vec4(v_game_normal, 0.0));
    v_normal = normalize(v_normal); 


    v_game_position = vec3(u_game_matrix * vec4(v_game_position, 1.0));
    v_game_normal = vec3(u_game_matrix * vec4(v_game_normal, 0.0));
    v_game_normal = normalize(v_game_normal); 

}