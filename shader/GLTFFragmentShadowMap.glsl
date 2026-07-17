#version 430
// A simple fragment shader with a single point light
precision mediump float;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
	
uniform sampler2D u_texture;
uniform int u_has_texture ;
uniform float u_alpha_cutoff ;
uniform mat4 u_light_map_camera_matrix;

out vec4 frag_color;


	
void main(void) {
	frag_color = v_color ;
    
    if(u_has_texture > 0){
        vec4 t_color = texture(u_texture, v_texcoord);
        frag_color.r *= t_color.r ;
        frag_color.g *= t_color.g ;
        frag_color.b *= t_color.b ;
        frag_color.a *= t_color.a ;
    }

    if(frag_color.a < u_alpha_cutoff ){
        discard ;
    }

    vec4 light_h = u_light_map_camera_matrix*vec4(v_position - v_normal*0.008f,1.0f) ;
    float z_value = light_h.z/light_h.w ;
    frag_color = vec4(z_value,0,0,1.0f) ;
  
}