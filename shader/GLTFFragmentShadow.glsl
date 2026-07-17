#version 430
// A fragment shader witha single light, casting shadows with a light_map
precision mediump float;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_game_position;
in vec3 v_game_normal ;

uniform sampler2D u_main_texture;
uniform int u_has_texture ; // use texture loaded with model

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

uniform int u_generate_texture = 0 ; // whether to generate a procedural texture
uniform vec3 u_texture_scale = vec3(3.0f, 6.0f, 3.0f); 
uniform vec2 u_center_variance = vec2(0.06f,0.06f) ;
uniform vec2 u_row_offset = vec2(0.5f, 0) ;
uniform float u_cell_edge_border = 0.9f;
uniform float u_block_edge_border = 0.12f ;

	
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



// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

// Compound versions to hash vectors
uint hash( uvec2 v ) { 
    return hash( v.x ^ hash(v.y));
}
uint hash( uvec3 v ) { 
    return hash( v.x ^ hash(v.y) ^ hash(v.z));
}
uint hash( uvec4 v ) { 
    return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w));
}

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

// Pseudo-random float in [0,1) from hashes of various inputs
float random(float x){ 
    return floatConstruct(hash(floatBitsToUint(x))); 
   }
float random(vec2  v){
    return floatConstruct(hash(floatBitsToUint(v))); 
}
float random(vec3  v){
    return floatConstruct(hash(floatBitsToUint(v))); 
}
float random(vec4  v){
    return floatConstruct(hash(floatBitsToUint(v))); 
}

vec3 randomColor(vec2 pos){
	float r = random(pos);
	float g = random(r);
	float b = random(g);
	return vec3(r,g,b);
}

vec3 randomVec3(vec2 seed, vec3 mid, vec3 variance){
	float x = random(seed);
	float y = random(x);
	float z = random(y);
	return vec3(mid.x + (2.0f*x-1.0f) * variance.x, mid.y + (2.0f*y-1.0f) * variance.y,mid.z + (2.0f*z-1.0f) * variance.z) ;
}

vec2 randomVec2(vec2 seed, vec2 mid, vec2 variance){
	float x = random(seed);
	float y = random(x);
	return vec2(mid.x + (2.0f*x-1.0f) * variance.x, mid.y + (2.0f*y-1.0f) * variance.y) ;
}

float rbf(vec2 a, vec2 b){
	vec2 c = a-b ;
	float d = c.x*c.x+c.y*c.y ;
	//return 1.0f/ (1.0f + 1000.0f*d);
	return exp(-30.0f*d) ;
}


float segmentDistance(vec2 x, vec2 A, vec2 B){
	vec2 AB = B-A ;
	float t = dot(x-A, AB) / sqrt(dot(AB,AB)) ;
	t = max(t, 0.0f);
	t = min(t, 1.0f);
	vec2 C = A + (AB * t) ;
	vec2 dx = C-x;
	return dot(dx,dx) ;
}

float rbf(vec2 x, vec2 A, vec2 B){
	return exp(-50.0f*segmentDistance(x,A,B)) ;
}


vec4 generateTexture(vec3 world_coord, vec3 world_normal){

    

	vec2 brick = vec2(world_coord.x*u_texture_scale.x+1.5f, world_coord.y*u_texture_scale.y+1.5f);
    

    float block_edge_distance = min(
        min(world_coord.x-floor(world_coord.x), ceil(world_coord.x)-world_coord.x),
        min(world_coord.y-floor(world_coord.y), ceil(world_coord.y)-world_coord.y));

    if(abs(world_normal.x) > 0.75f){
         brick = vec2(world_coord.z*u_texture_scale.z+1.5f, world_coord.y*u_texture_scale.y+1.5f);

         block_edge_distance = min(
        min(world_coord.z-floor(world_coord.z), ceil(world_coord.z)-world_coord.z),
        min(world_coord.y-floor(world_coord.y), ceil(world_coord.y)-world_coord.y));

    }
    if(abs(world_normal.y) > 0.75f){
         brick = vec2(world_coord.x*u_texture_scale.x+1.5f, world_coord.z*u_texture_scale.z+1.5f);
         block_edge_distance = min(
        min(world_coord.x-floor(world_coord.x), ceil(world_coord.x)-world_coord.x),
        min(world_coord.z-floor(world_coord.z), ceil(world_coord.z)-world_coord.z));
    }
    

	// get the bounding corners of the block to generate bricks from
	vec2 up_left = vec2(floor(brick.x), floor(brick.y)) ;
	vec2 down_left = vec2(floor(brick.x), ceil(brick.y)) ;
	vec2 up_right = vec2(ceil(brick.x), floor(brick.y)) ;
	vec2 down_right = vec2(ceil(brick.x), ceil(brick.y)) ;


	// randomly vary the brick centers a bit to add character
	
	vec2 p1 = randomVec2(up_left,up_left, u_center_variance);
	vec2 p2 = randomVec2(up_right,up_right, u_center_variance);
	vec2 p3 = randomVec2(down_left,down_left, u_center_variance);
	vec2 p4 = randomVec2(down_right, down_right, u_center_variance);

	// use a horizontal offset based on rows to misalign the brick on subsequent rows
	//TODO generalize to nonbrick things
	vec2 q1 = int(up_left.y)%2 == 0 ? p1 + u_row_offset: p1 - u_row_offset ;
	vec2 q2 = int(up_right.y)%2 == 0 ? p2 + u_row_offset : p2 - u_row_offset ;
	vec2 q3 = int(down_left.y)%2 == 0 ? p3 + u_row_offset : p3 - u_row_offset ;
	vec2 q4 = int(down_right.y)%2 == 0 ? p4 + u_row_offset : p4 - u_row_offset ;

    //generate a random color scaling from red to green for use with color remap
    float r = random(up_left);
    vec3 c1 = vec3(r, 1.0f-r,0.0f);
    r = random(up_right);
    vec3 c2 = vec3(r, 1.0f-r,0.0f);
    r = random(down_left);
    vec3 c3 = vec3(r, 1.0f-r,0.0f);
    r = random(down_right);
    vec3 c4 = vec3(r, 1.0f-r,0.0f);

	//RBF on line segment to determine nearest brick
	float r1 = rbf(brick, p1, q1);
	float r2 = rbf(brick, p2, q2);
	float r3 = rbf(brick, p3, q3);
	float r4 = rbf(brick, p4, q4);
	float total = r1+r2+r3+r4 ;
	r1/= total;
	r2/= total;
	r3/= total;
	r4/= total;


	// use maximum rbf result's dominance to determine if on an edge for mortar
	
    float edge_border = u_cell_edge_border ;
    if(block_edge_distance < u_block_edge_border){
        edge_border = edge_border + u_block_edge_border - block_edge_distance ;
    }

	float r_max = max(max(r1,r2),max(r3,r4));

	
	if(r_max > edge_border){
        vec3 blend_color = r1*c1+r2*c2+r3*c3+r4*c4 ;
		return vec4(blend_color, 1.0f) ;
	}else{
		return vec4(0.0f,0.0f,1.0f, 1.0f); // return pure blue for the edges, to be overridden with color_remap
	}

}

void main(void) {
    // First compute the color before lighting
    frag_color = v_color ;
    
    if(u_generate_texture > 0){
        vec4 t_color = generateTexture(v_game_position, v_game_normal);
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
    if(relative_to_light.x > 0 && relative_to_light.y > 0 && relative_to_light.x < 1.0f && relative_to_light.y < 1.0f){

        float frag_depth = relative_to_light.z;
        float light_amount = 0 ;
        if(frag_depth <= texture(u_light_map_depth, relative_to_light.xy).r){ 
            light_amount = 0.2f ;
        }
        // give it some light based on any neighbors that are lit
        float uv_offset = 0.0005f ;
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
        
        
        
        light =  computeLight( light_amount );
        

    }else{
        light =  computeLight(0.3f);
    }
    //light.r  = min(light.r, 1.0f) ;
    //light.g  = min(light.g, 1.0f) ;
    //light.b  = min(light.b, 1.0f) ;
    frag_color.r *= light.r ;
    frag_color.g *= light.g ;
    frag_color.b *= light.b ;
    frag_color.a = 1.0f ;
}
