#version 430
// A fragment shader witha single light, casting shadows with a light_map
precision mediump float;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;

uniform sampler2D u_main_texture;
uniform int u_has_texture ;
uniform float u_alpha_cutoff ;

uniform int u_color_remap_enabled;
uniform vec3 u_red_remap ;
uniform vec3 u_green_remap ;
uniform vec3 u_blue_remap ;
	
uniform vec3 u_light_point;
uniform vec3 u_light_color;
uniform mat4 u_light_map_camera_matrix;
uniform sampler2D u_light_map_depth;
uniform vec3 u_ambient_light;
uniform vec3 u_camera_position ;
uniform float specular_mult;
uniform float diffuse_mult;
uniform float ambient_mult;
//uniform float shininess ;
uniform int u_generate_texture = 0 ;

	
out vec4 frag_color;



vec3 computeLight(float fraction_visible){
    vec3 to_light = normalize(u_light_point - v_position) ;
    vec3 to_camera = normalize(u_camera_position - v_position) ;

    float diffuse = max(dot(v_normal,to_light),0.0);
    //float specular = pow(max(0., -1.0 * dot(to_camera, reflect (to_light,v_normal))),shininess); // TODO pow is expensive if this is always 2
    float specular = max(0., -1.0 * dot(to_camera, reflect (to_light,v_normal)));
    specular = specular*specular ;
    return u_light_color * fraction_visible * ( specular * specular_mult + diffuse * diffuse_mult) + u_ambient_light * ambient_mult ;

}

void main(void) {
    // First compute the color before lighting
    frag_color = v_color ;
    
   
    if(u_generate_texture > 0){
        vec4 t_color = vec4(1.0f,0.0f,1.0f,1.0f) ; //magenta cause path isn't defined at the moment
        frag_color.r *= t_color.r ;
        frag_color.g *= t_color.g ;
        frag_color.b *= t_color.b ;
        frag_color.a *= t_color.a ;
    }else if(u_has_texture > 0){
        vec4 t_color = texture(u_main_texture, v_texcoord);
        frag_color.r *= t_color.r ;
        frag_color.g *= t_color.g ;
        frag_color.b *= t_color.b ;
        frag_color.a *= t_color.a ;
    }

    if(frag_color.a < u_alpha_cutoff ){
        discard ;
    }

    if(u_color_remap_enabled > 0){
        vec3 remapped = u_red_remap * frag_color.r + u_green_remap * frag_color.g + u_blue_remap * frag_color.b ;
        frag_color.r = remapped.r ;
        frag_color.g = remapped.g ;
        frag_color.b = remapped.b ;
    }
    

    vec4 light_h = u_light_map_camera_matrix*vec4(v_position,1.0f) ;
    vec3 relative_to_light = vec3(light_h.x/light_h.w, light_h.y/light_h.w, light_h.z/light_h.w ) ;
    relative_to_light.x = (relative_to_light.x+1.0f)*0.5f ;
    relative_to_light.y = (relative_to_light.y+1.0f)*0.5f ;

    vec3 light = u_ambient_light * ambient_mult ;
    // If within the spotlight area
    if(relative_to_light.x >= 0 && relative_to_light.y >= 0 && relative_to_light.x <= 1.0f && relative_to_light.y <= 1.0f){

        float frag_depth = relative_to_light.z;
        float light_amount = 0 ;
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy).r){ 
            light_amount = 0.2f ;
        }
        // give it some light based on any neighbors that are lit
        float uv_offset = 0.00025f ;
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy + vec2(0,uv_offset)).r){
            light_amount += 0.2f;
        }
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy + vec2(0,-uv_offset)).r){
            light_amount += 0.2f;
        }
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy + vec2(uv_offset,0)).r){
            light_amount += 0.2f;
        }
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy + vec2(-uv_offset,0)).r){
            light_amount += 0.2f;
        }
        
        
        light =  computeLight(light_amount);
        

    }
    //light.r  = min(light.r, 1.0f) ;
    //light.g  = min(light.g, 1.0f) ;
    //light.b  = min(light.b, 1.0f) ;
    frag_color.r *= light.r ;
    frag_color.g *= light.g ;
    frag_color.b *= light.b ;



    frag_color.a = min(1.0f,max(0.4f,1.4f + dot(v_normal,normalize(v_position-u_camera_position))));
    
}