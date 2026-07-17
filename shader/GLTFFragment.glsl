#version 430
// A simple fragment shader with a single point light
precision mediump float;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
	
uniform vec3 u_light_point;
uniform sampler2D u_texture;
uniform int u_has_texture ;
uniform float u_alpha_cutoff ;

uniform float direct_mult ;
uniform float indirect_mult ;

uniform int color_remap_enabled;
uniform vec3 red_remap ;
uniform vec3 green_remap ;
uniform vec3 blue_remap ;

uniform mat4 uPMatrix;	
uniform int always_in_front = 0;

out vec4 frag_color;

float lightFrom(vec3 light_point){
	vec3 light_ray = v_position - light_point;
	float direct = max(0., -dot(light_ray, v_normal)/length(light_ray));
	return direct ;
}
	
void main(void) {
	float direct = lightFrom(u_light_point) ;
	float indirect = 1.;
	float l = indirect_mult * indirect + direct_mult * direct ; // Add some ambient light
    frag_color = vec4(l *v_color.r ,l * v_color.g,l*v_color.b , v_color.a) ;
    
    if(u_has_texture > 0){
        vec4 t_color = texture(u_texture, v_texcoord);
        frag_color.r *= t_color.r ;
        frag_color.g *= t_color.g ;
        frag_color.b *= t_color.b ;
        frag_color.a *= t_color.a ;
    }

    if(color_remap_enabled > 0){
        vec3 remapped = red_remap * frag_color.r + green_remap * frag_color.g + blue_remap * frag_color.b ;
        frag_color.r = remapped.r ;
        frag_color.g = remapped.g ;
        frag_color.b = remapped.b ;
    }
    
    if(frag_color.a < u_alpha_cutoff ){
        discard ;
    }

   
    // Compute clips space location and depth
    vec4 clip_p = uPMatrix * vec4(v_position, 1.0);
    float depth = clip_p.z / clip_p.w;
    if(always_in_front > 0){
        depth = depth*0.1f ; // reduce depth so panel appears over game world
    }
    // apply standard openGL depth calculation
    gl_FragDepth = ((gl_DepthRange.diff * depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
    
}