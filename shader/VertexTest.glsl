#version 330
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

out vec4 v_color;
out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord;


void main(void) {
    v_color = aVertexColor;

    v_position = aVertexPosition ;
    v_normal = aNormal ;
    
    v_texcoord = aTexcoord;
        
    gl_Position = uPMatrix * uMVMatrix * vec4(v_position, 1.0);

    v_position = vec3(uMVMatrix * vec4(v_position, 1.0));

    v_normal = vec3(uMVMatrix * vec4(v_normal, 0.0));
    v_normal = normalize(v_normal); 
}