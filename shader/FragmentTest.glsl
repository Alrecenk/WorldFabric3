#version 330
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
uniform float u_nearness_cutoff ;

uniform float direct_mult ;
uniform float indirect_mult ;

uniform int color_remap_enabled;//TODO
uniform vec3 red_remap ;
uniform vec3 green_remap ;
uniform vec3 blue_remap ;//TODO

	
out vec4 frag_color;

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }

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

// Pseudo-random value in half-open range [0:1].
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
float random( vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec3  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec4  v ) { return floatConstruct(hash(floatBitsToUint(v))); }

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

vec2 scaleAxes(vec2 a, vec2 scale){
	return vec2(a.x*scale.x, a.y*scale.y) ;
}
	
void main(void) {
	vec2 scale = vec2(4.0f, 8.0f); 

	vec2 brick = vec2(v_texcoord.x*scale.x + 0.5f, v_texcoord.y*scale.y+0.5f);

	// get the bounding corners of the block to generate bricks from
	vec2 up_left = vec2(floor(brick.x), floor(brick.y)) ;
	vec2 down_left = vec2(floor(brick.x), ceil(brick.y)) ;
	vec2 up_right = vec2(ceil(brick.x), floor(brick.y)) ;
	vec2 down_right = vec2(ceil(brick.x), ceil(brick.y)) ;


	// randomly vary the brick centers a bit to add character
	vec2 center_variance = vec2(0.12f,0.08f) ;
	vec2 p1 = randomVec2(up_left,up_left, center_variance);
	vec2 p2 = randomVec2(up_right,up_right, center_variance);
	vec2 p3 = randomVec2(down_left,down_left, center_variance);
	vec2 p4 = randomVec2(down_right, down_right, center_variance);

	// use a horizontal offset based on rows to misalign the brick on subsequent rows
	vec2 offset = vec2(0.5f, 0) ;
	vec2 q1 = int(up_left.y)%2 == 0 ? p1 + offset : p1 - offset ;
	vec2 q2 = int(up_right.y)%2 == 0 ? p2 + offset : p2 - offset ;
	vec2 q3 = int(down_left.y)%2 == 0 ? p3 + offset : p3 - offset ;
	vec2 q4 = int(down_right.y)%2 == 0 ? p4 + offset : p4 - offset ;

	//generate random color offsets for each brick
	vec3 average_color = vec3(0.3f,0.25f,0.3f) ;
	vec3 color_variance = vec3(0.03f,0.03f,0.03f) ;
	vec3 c1 = randomVec3(up_left, average_color, color_variance);
	vec3 c2 = randomVec3(up_right, average_color, color_variance);
	vec3 c3 = randomVec3(down_left, average_color, color_variance);
	vec3 c4 = randomVec3(down_right, average_color, color_variance);

	/*
	// scale back to original coodinates so edge boundaries are equally sized in both axes
	scale.x = 1.0f ;
	scale.y = 1.0f ;
	brick = scaleAxes(brick,scale) ;
	p1 = scaleAxes(p1,scale);
	p2 = scaleAxes(p2,scale);
	p3 = scaleAxes(p3,scale);
	p4 = scaleAxes(p4,scale);
	q1 = scaleAxes(q1,scale);
	q2 = scaleAxes(q2,scale);
	q3 = scaleAxes(q3,scale);
	q4 = scaleAxes(q4,scale);
	*/

	//RBG on line segment to determine nearest brick
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
	float edge_border = 0.9f;
	vec3 edge_color = vec3(0.5,0.5f,0.5f) ;
	float r_max = max(max(r1,r2),max(r3,r4));
	vec3 blend_color = r1*c1+r2*c2+r3*c3+r4*c4 ;
	if(r_max > edge_border){
		frag_color = vec4(blend_color, 1.0f) ;
	}else{
		frag_color = vec4(edge_color , 1.0f);
	}



    
}