#include "GLTF.h"
#include "Variant.h"
#include "OptimizationProblem.h"

#include <math.h>
#include <map>
#include <vector>
#include <string>
#include <limits>
#include <stdlib.h>
#include <queue>
#include <sstream>
#include <unordered_set>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::string;
using std::vector;
using std::map;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::dvec3;
using glm::dvec4;
using glm::ivec4;
using glm::mat4;
typedef GLTF::Triangle Triangle;
using std::string;
using std::queue;


// Constructor
GLTF::GLTF(){
    this->position_changed = false;
    this->model_changed = false;
}

//Destructor
GLTF::~GLTF(){
}

// Returns a Variant of 3 vec3's'for each each triangle dereferenced
// Used to get arrays for displaying with a simple shader
Variant GLTF::getFloatBuffer(std::vector<glm::vec3>& point_list, int material){

    int num_triangles = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            num_triangles++;
        }
    }

    //TODO this access pattern prevents these Variant members from being private 
    // but using the constructor forces a copy that isn't necesarry 
    // Maybe here should be an array constructor that just takes type and size
    // and then the get array provides a shallow pointer that can't be freed?
    Variant buffer;
    buffer.type_ = Variant::FLOAT_ARRAY;
    buffer.ptr = (byte*)malloc(4 + num_triangles * 9 * sizeof(float));
    
    *((int*)buffer.ptr) = num_triangles * 9 ;// number of floats in array
    float* buffer_array =  (float*)(buffer.ptr+4) ; // pointer to start of float array
    int j9 = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            Triangle& t = this->triangles[k];
            // A
            vec3& A = point_list[t.A];
            buffer_array[j9] = A.x;
            buffer_array[j9+1] = A.y;
            buffer_array[j9+2] = A.z;
            // B
            vec3& B = point_list[t.B];
            buffer_array[j9+3] = B.x;
            buffer_array[j9+4] = B.y;
            buffer_array[j9+5] = B.z;
            // C
            vec3& C = point_list[t.C];
            buffer_array[j9+6] = C.x;
            buffer_array[j9+7] = C.y;
            buffer_array[j9+8] = C.z;
            j9+=9;
        }
    }
    return buffer ;
}

// Returns a Variant of 3 vec2's'for each each triangle dereferenced
// Used to get arrays for displaying with a simple shader
Variant GLTF::getFloatBuffer(std::vector<glm::vec2>& point_list, int material){
    int num_triangles = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            num_triangles++;
        }
    }

    //TODO this access pattern prevents these Variant members from being private 
    // but using the constructor forces a copy that isn't necesarry 
    // Maybe here should be an array constructor that just takes type and size
    // and then the get array provides a shallow pointer that can't be freed?
    Variant buffer;
    buffer.type_ = Variant::FLOAT_ARRAY;
    buffer.ptr = (byte*)malloc(4 + num_triangles * 6 * sizeof(float));
    
    *((int*)buffer.ptr) = num_triangles * 6 ;// number of floats in array
    float* buffer_array =  (float*)(buffer.ptr+4) ; // pointer to start of float array
    int j6 = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            Triangle& t = this->triangles[k];
            // A
            vec2& A = point_list[t.A];
            buffer_array[j6] = A.x;
            buffer_array[j6+1] = A.y;
            // B
            vec2& B = point_list[t.B];
            buffer_array[j6+2] = B.x;
            buffer_array[j6+3] = B.y;
            // C
            vec2& C = point_list[t.C];
            buffer_array[j6+4] = C.x;
            buffer_array[j6+5] = C.y;
            j6+=6;
        }
    }
    return buffer ;
}


// Returns a Variant of 3 vec4's'for each each triangle dereferenced
// Used to get arrays for displaying with a simple shader
Variant GLTF::getFloatBuffer(std::vector<glm::vec4>& point_list, int material){

    int num_triangles = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            num_triangles++;
        }
    }

    //TODO this access pattern prevents these Variant members from being private 
    // but using the constructor forces a copy that isn't necesarry 
    // Maybe here should be an array constructor that just takes type and size
    // and then the get array provides a shallow pointer that can't be freed?
    Variant buffer;
    buffer.type_ = Variant::FLOAT_ARRAY;
    buffer.ptr = (byte*)malloc(4 + num_triangles * 12 * sizeof(float));
    
    *((int*)buffer.ptr) = num_triangles * 12 ;// number of floats in array
    float* buffer_array =  (float*)(buffer.ptr+4) ; // pointer to start of float array
    int j = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            Triangle& t = this->triangles[k];
            // A
            vec4& A = point_list[t.A];
            buffer_array[j] = A.w;
            buffer_array[j+1] = A.x;
            buffer_array[j+2] = A.y;
            buffer_array[j+3] = A.z;
            // B
            vec4& B = point_list[t.B];
            buffer_array[j+4] = B.w;
            buffer_array[j+5] = B.x;
            buffer_array[j+6] = B.y;
            buffer_array[j+7] = B.z;
            // C
            vec4& C = point_list[t.C];
            buffer_array[j+8] = C.w;
            buffer_array[j+9] = C.x;
            buffer_array[j+10] = C.y;
            buffer_array[j+11] = C.z;
            j+=12;
        }
    }
    return buffer ;
}

// Returns a Variant of 3 vec4's'for each each triangle dereferenced
// Used to get arrays for displaying with a simple shader
Variant GLTF::getFloatBuffer(std::vector<glm::ivec4>& point_list, int material){

    int num_triangles = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            num_triangles++;
        }
    }

    //TODO this access pattern prevents these Variant members from being private 
    // but using the constructor forces a copy that isn't necesarry 
    // Maybe here should be an array constructor that just takes type and size
    // and then the get array provides a shallow pointer that can't be freed?
    Variant buffer;
    buffer.type_ = Variant::FLOAT_ARRAY;
    buffer.ptr = (byte*)malloc(4 + num_triangles * 12 * sizeof(float));
    
    *((int*)buffer.ptr) = num_triangles * 12 ;// number of floats in array
    float* buffer_array =  (float*)(buffer.ptr+4) ; // pointer to start of float array
    int j = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == material){
            Triangle& t = this->triangles[k];
            // A
            ivec4& A = point_list[t.A];
            buffer_array[j] = (float)A.w;
            buffer_array[j+1] = (float)A.x;
            buffer_array[j+2] = (float)A.y;
            buffer_array[j+3] = (float)A.z;
            // B
            ivec4& B = point_list[t.B];
            buffer_array[j+4] = (float)B.w;
            buffer_array[j+5] = (float)B.x;
            buffer_array[j+6] = (float)B.y;
            buffer_array[j+7] = (float)B.z;
            // C
            ivec4& C = point_list[t.C];
            buffer_array[j+8] = (float)C.w;
            buffer_array[j+9] = (float)C.x;
            buffer_array[j+10] = (float)C.y;
            buffer_array[j+11] = (float)C.z;
            j+=12;
        }
    }
    return buffer ;
}


// Returns a Variant of openGL triangle buffers for displaying this mesh in world space
Variant GLTF::getChangedBuffer(int selected_material) {
    return Variant(getChangedBufferObject(selected_material));
}

std::map<string, Variant> GLTF::getChangedBufferObject(int selected_material){
    std::map<string, Variant> buffers;
    
    int num_triangles = 0 ;
    for(int k=0;k<this->triangles.size();k++){
        if(this->triangles[k].material == selected_material){
            num_triangles++;
        }
    }

    buffers["vertices"] = Variant(num_triangles*3);

    if(this->position_changed){
        Variant& pos_buffer = buffers["position"];
        pos_buffer.type_ = Variant::FLOAT_ARRAY;
        pos_buffer.ptr = (byte*)malloc(4 + num_triangles * 9 * sizeof(float));
        
        *((int*)pos_buffer.ptr) = num_triangles * 9 ;// number of floats in array
        float* pos_buffer_array =  (float*)(pos_buffer.ptr+4) ; // pointer to start of float array

        Variant& norm_buffer = buffers["normal"];
        norm_buffer.type_ = Variant::FLOAT_ARRAY;
        norm_buffer.ptr = (byte*)malloc(4 + num_triangles * 9 * sizeof(float));
        
        *((int*)norm_buffer.ptr) = num_triangles * 9 ;// number of floats in array
        float* norm_buffer_array =  (float*)(norm_buffer.ptr+4) ; // pointer to start of float array
        
        int j9 = 0 ;
        for(int k=0;k<this->triangles.size();k++){
            if(this->triangles[k].material == selected_material){
                Triangle& t = this->triangles[k];
                // A
                vec3& A = vertices[t.A].position;
                if (boneless) { // if no bones model's built in transform is ignored, so copy it at buffer build time
                    A = vertices[t.A].transformed_position;
                }
                pos_buffer_array[j9] = A.x;
                pos_buffer_array[j9+1] = A.y;
                pos_buffer_array[j9+2] = A.z;
                // B
                vec3& B = vertices[t.B].position;
                if (boneless) {
                    B = vertices[t.B].transformed_position;
                }
                pos_buffer_array[j9+3] = B.x;
                pos_buffer_array[j9+4] = B.y;
                pos_buffer_array[j9+5] = B.z;
                // C
                vec3& C = vertices[t.C].position;
                if (boneless) {
                    C = vertices[t.C].transformed_position;
                }
                pos_buffer_array[j9+6] = C.x;
                pos_buffer_array[j9+7] = C.y;
                pos_buffer_array[j9+8] = C.z;

                
                // A
                vec3& A2 = vertices[t.A].normal;
                norm_buffer_array[j9] = A2.x;
                norm_buffer_array[j9+1] = A2.y;
                norm_buffer_array[j9+2] = A2.z;
                // B
                vec3& B2 = vertices[t.B].normal;
                norm_buffer_array[j9+3] = B2.x;
                norm_buffer_array[j9+4] = B2.y;
                norm_buffer_array[j9+5] = B2.z;
                // C
                vec3& C2 = vertices[t.C].normal;
                norm_buffer_array[j9+6] = C2.x;
                norm_buffer_array[j9+7] = C2.y;
                norm_buffer_array[j9+8] = C2.z;
                
                j9+=9;
            }
        }
    }    

    if(this->model_changed){
        vector<vec4> color ;
        vector<vec2> tex_coord;
        vector<vec4> weights;
        vector<ivec4> joints;
        for(const auto& v : this->vertices){
            color.push_back(vec4(v.color_mult.g, v.color_mult.b, v.color_mult.a, v.color_mult.r));// Not sure why, but GLSL doesn't map these the same
            tex_coord.push_back(v.tex_coord);
            weights.push_back(v.weights);
            joints.push_back(v.joints);
        }

        buffers["color"] = this->getFloatBuffer(color, selected_material);
        buffers["tex_coord"] = this->getFloatBuffer(tex_coord, selected_material);
        if(!boneless){
            buffers["weights"] = this->getFloatBuffer(weights, selected_material);
            buffers["joints"] = this->getFloatBuffer(joints, selected_material);  
        }

        const auto& mat = this->materials[selected_material] ;
        map<string,Variant> mat_map;
        mat_map["color"] = Variant(mat.color);
        mat_map["metallic"] = Variant(mat.roughness);
        mat_map["roughness"] = Variant(mat.roughness);
        mat_map["name"] = Variant(mat.name) ;
        mat_map["double_sided"] = Variant(mat.double_sided ? 1 : 0);
        mat_map["has_texture"] = Variant(mat.texture ? 1: 0);
        if(mat.texture){
            const Image& img = this->images[mat.image];
            //printf("Mat image: %d for %d tris  hash: %d\n", mat.image, num_triangles, img.data.hash());
            mat_map["image_name"] = Variant(img.name);
            mat_map["image_width"] = Variant(img.width) ;
            mat_map["image_height"] = Variant(img.height) ;
            mat_map["image_channels"] = Variant(img.channels);
            mat_map["image_data"] = img.data.clone();
        }
        
        buffers["material"] = Variant(mat_map);
        
    }

    if(model_changed || position_changed){
        buffers["bones"] = getBoneData(); 
        buffers["boneless"] = Variant(boneless ? 1 : 0) ;  
    }
    

    return buffers;
}

// returns the data necesarry to create an indexed Vulkan render call
		// one model for each material
std::vector<std::shared_ptr<GLTF::RenderModel>> GLTF::getRenderBuffers(bool include_morphable){
	std::vector<std::shared_ptr<RenderModel>> models ;
	for (auto const& [material_id, mat] : materials) {
		std::shared_ptr<RenderModel> render_model = std::shared_ptr<RenderModel>(new RenderModel()) ;
		std::unordered_map<int,int> old_to_new_index ;
		int num_vertices = 0 ;
		int num_triangles = 0;
		for (int k = 0; k < triangles.size(); k++) {
			if (triangles[k].material == material_id && ( include_morphable || 
				(vertices[triangles[k].A].morph_position.size() == 0 && 
				vertices[triangles[k].B].morph_position.size() == 0 && // No morphable vertices
				vertices[triangles[k].C].morph_position.size() == 0)
			)) {
				num_triangles++;
				if(old_to_new_index.find(triangles[k].A) == old_to_new_index.end()){
					old_to_new_index[triangles[k].A] = num_vertices ;
					Vertex& v = vertices[triangles[k].A];
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord,v.joints,v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].A]) ;
				if (old_to_new_index.find(triangles[k].B) == old_to_new_index.end()) {
					old_to_new_index[triangles[k].B] = num_vertices;
					Vertex& v = vertices[triangles[k].B];
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord, v.joints, v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].B]);
				if (old_to_new_index.find(triangles[k].C) == old_to_new_index.end()) {
					old_to_new_index[triangles[k].C] = num_vertices;
					Vertex& v = vertices[triangles[k].C];
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord, v.joints, v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].C]);
				num_triangles++;
			}
		}
		if (mat.texture) {
			const Image& img = this->images[mat.image];
			render_model->texture_width = img.width;
			render_model->texture_height = img.height;

			if(img.channels == 4){
				render_model->color_texture_data = img.data.clone() ;
			}else if(img.channels == 3){
				render_model->color_texture_data.makeFillableByteArray(img.width * img.height * 4); ; // always output a 4 channel ARGB though images may or may not have alpha
				byte* out_array = render_model->color_texture_data.getByteArray();
				byte* in_array = img.data.getByteArray();
				for(int x = 0 ; x< img.width; x++){
					for(int y = 0; y < img.height ;y++){
						out_array[4 * (x + y * img.width)] = in_array[3 * (x + y * img.width)] ;
						out_array[4 * (x + y * img.width)+1] = in_array[3 * (x + y * img.width)+1];
						out_array[4 * (x + y * img.width)+2] = in_array[3 * (x + y * img.width)+2];
						out_array[4 * (x + y * img.width)+3] = 0xff;
					}
				}
			}
			//printf(" width: %d, height %d, channels: %d, texture data length: %d\n",img.width, img.height, img.channels,render_model->color_texture_data.getArrayLength()) ;
			if (num_triangles > 0) {
				models.push_back(render_model);
			}

		}else{
			//printf("can't load texture in Vulkan GLB buffer!\n");
			render_model->texture_width = 1;
			render_model->texture_height = 1;

			render_model->color_texture_data.makeFillableByteArray(4); ; // always output a 4 channel ARGB though images may or may not have alpha
			byte* out_array = render_model->color_texture_data.getByteArray();
			out_array[0] = 255 ;
			out_array[1] = 255;
			out_array[2] = 255;
			out_array[3] = 255;
			if (num_triangles > 0) {
				models.push_back(render_model);
			}
		}

		

	}
	return models; 
}


//Like getRenderBuffers but returns only the morphable triangles of the model morphed with the given morph weights
std::vector< std::shared_ptr<GLTF::RenderModel>> GLTF::getMorphedRenderBuffers(std::vector<float> weights, bool include_texture){
	if(morph_names.size() == 0){
		return std::vector< std::shared_ptr<GLTF::RenderModel>>();
	}
	//cache the triangles that have morph so we don't walk entire model to find them each time we morph
	if(morph_triangles.size() == 0){
		for (int k = 0; k < triangles.size(); k++) {
			if (vertices[triangles[k].A].morph_position.size() > 0 ||
				vertices[triangles[k].B].morph_position.size() > 0 ||
				vertices[triangles[k].C].morph_position.size() > 0
			) {
				morph_triangles.push_back(k) ;
			}
		}
	}

	std::vector<std::shared_ptr<RenderModel>> models;
	for (auto const& [material_id, mat] : materials) {
		std::shared_ptr<RenderModel> render_model = std::shared_ptr<RenderModel>(new RenderModel());
		static std::unordered_map<int, int> old_to_new_index;
		old_to_new_index.clear();
		int num_vertices = 0;
		int num_triangles = 0;
		for(int k : morph_triangles){
			if (triangles[k].material == material_id ) {
				num_triangles++;
				if (old_to_new_index.find(triangles[k].A) == old_to_new_index.end()) {
					old_to_new_index[triangles[k].A] = num_vertices;
					Vertex& v = vertices[triangles[k].A];
					glm::vec3 position = v.position ;
					glm::vec3 normal = v.normal ;
					for(int i = 0; i < weights.size() && i < v.morph_position.size();i++){
						position += v.morph_position[i] * weights[i] ;
						normal += v.morph_normal[i] * weights[i] ;
					}
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord, v.joints, v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].A]);
				if (old_to_new_index.find(triangles[k].B) == old_to_new_index.end()) {
					old_to_new_index[triangles[k].B] = num_vertices;
					Vertex& v = vertices[triangles[k].B];
					glm::vec3 position = v.position;
					glm:: vec3 normal = v.normal;
					for (int i = 0; i < weights.size() && i < v.morph_position.size(); i++) {
						position += v.morph_position[i] * weights[i];
						normal += v.morph_normal[i] * weights[i];
					}
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord, v.joints, v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].B]);
				if (old_to_new_index.find(triangles[k].C) == old_to_new_index.end()) {
					old_to_new_index[triangles[k].C] = num_vertices;
					Vertex& v = vertices[triangles[k].C];
					glm::vec3 position = v.position;
					glm:: vec3 normal = v.normal;
					for (int i = 0; i < weights.size() && i < v.morph_position.size(); i++) {
						position += v.morph_position[i] * weights[i];
						normal += v.morph_normal[i] * weights[i];
					}
					render_model->vertices.emplace_back(v.position, v.normal, v.color_mult, v.tex_coord, v.joints, v.weights);
					num_vertices++;
				}
				render_model->indices.push_back(old_to_new_index[triangles[k].C]);
				num_triangles++;
			}
		}
		if(include_texture){
			if (mat.texture) {
				const Image& img = this->images[mat.image];
				render_model->texture_width = img.width;
				render_model->texture_height = img.height;

				if (img.channels == 4) {
					render_model->color_texture_data = img.data.clone();
				}
				else if (img.channels == 3) {
					render_model->color_texture_data.makeFillableByteArray(img.width * img.height * 4); ; // always output a 4 channel ARGB though images may or may not have alpha
					byte* out_array = render_model->color_texture_data.getByteArray();
					byte* in_array = img.data.getByteArray();
					for (int x = 0; x < img.width; x++) {
						for (int y = 0; y < img.height; y++) {
							out_array[4 * (x + y * img.width)] = in_array[3 * (x + y * img.width)];
							out_array[4 * (x + y * img.width) + 1] = in_array[3 * (x + y * img.width) + 1];
							out_array[4 * (x + y * img.width) + 2] = in_array[3 * (x + y * img.width) + 2];
							out_array[4 * (x + y * img.width) + 3] = 0xff;
						}
					}


				}
				//printf(" width: %d, height %d, channels: %d, texture data length: %d\n",img.width, img.height, img.channels,render_model->color_texture_data.getArrayLength()) ;
				

			}
			else {
				//printf("can't load texture in Vulkan GLB buffer!\n");
				render_model->texture_width = 1;
				render_model->texture_height = 1;

				render_model->color_texture_data.makeFillableByteArray(4); ; // always output a 4 channel ARGB though images may or may not have alpha
				byte* out_array = render_model->color_texture_data.getByteArray();
				out_array[0] = 255;
				out_array[1] = 255;
				out_array[2] = 255;
				out_array[3] = 255;
			}
		}
		if (num_triangles > 0) {
			models.push_back(render_model);
		}
		



	}
	return models;
}

// Returns instance data containing a base pose andall bone poses in mat4 format
std::shared_ptr<GLTF::Instance256> GLTF::getPoseBuffer(){
	std::shared_ptr<GLTF::Instance256> instance  = std::shared_ptr<GLTF::Instance256>(new GLTF::Instance256());
	instance->root = transform ;
	for (int node_id = 0; node_id < nodes.size(); node_id++) {
		instance->bone_pose[node_id] = nodes[node_id].transform ;
	}

	return instance ;
}

//returns a transform that would center this model at 0,0,0 with roughly a size of 1
glm::mat4 GLTF::getNormalizationTransform() const{
	// Move to origin and make about 1 meter in size
	float size = 0;
	glm::vec3 center(0, 0, 0);
	for (int k = 0; k < 3; k++) {
		center[k] = (max[k] + min[k]) * 0.5f;
		size = fmax(size, fabs(max[k] - center[k]));
		size = fmax(size, fabs(min[k] - center[k]));

	}
	glm::mat4 result = glm::scale(glm::mat4(1), { (1.0f / size),(1.0f / size),(1.0f / size) });
	result = glm::translate(result, center * -1.0f);
	return result ;
}

Variant GLTF::getBoneData(){
    int num_bones = (int)nodes.size() ;
    int shader_num_bones = 40; // TODO avoid duplicate constant definition with bones texture
    Variant bone_buffer;
    bone_buffer.type_ = Variant::FLOAT_ARRAY;
    bone_buffer.ptr = (byte*)malloc(4 + shader_num_bones * 16 * sizeof(float));
    *((int*)bone_buffer.ptr) = shader_num_bones * 16 ;// number of floats in array
    float* bone_buffer_array =  (float*)(bone_buffer.ptr+4) ; // pointer to start of float array
    for(int node_id=0; node_id<nodes.size(); node_id++){    
        Node& node = nodes[node_id];  
        memcpy(bone_buffer_array + (node_id*16), &(node.transform), 64);
        /*
        Variant to_print = Variant(node.transform);
        printf("Node: %d\n", node_id);
            to_print.printFormatted();
            */

    }
    return bone_buffer ;
}



Variant GLTF::getCompressedBoneData(){
    int num_bones = (int)nodes.size() ;
    /*
    // lossless compression
    Variant bone_buffer;
    bone_buffer.makeFillableFloatArray(num_bones*4);
    float* bone_buffer_array =  bone_buffer.getFloatArray() ;
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        bone_buffer_array[node_id*4] = node.rotation.x ;
        bone_buffer_array[node_id*4 + 1] = node.rotation.y ;
        bone_buffer_array[node_id*4 + 2] = node.rotation.z ;
        bone_buffer_array[node_id*4 + 3] = node.rotation.w ;
    }
    //printf("num compressed bones: %d size: %d bytes\n", num_bones, num_bones*16);
    return bone_buffer ;
    */
    
    // super compressed version
    // Find IDs of all bones with nondefault values
    vector<byte> compressed_ids ;
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        glm::quat diff = node.base_rotation - node.rotation ;
        float d2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z + diff.w*diff.w ;
        if(d2 > 0.001f){
            compressed_ids.push_back(node_id);
        }
    }
    // pack changed bones as shorts into single byte array with indices
    Variant bone_buffer;
    bone_buffer.makeFillableByteArray((int)compressed_ids.size()*9);
    byte* data = bone_buffer.getByteArray();
    int j = 0 ;
    for(byte node_id : compressed_ids){
        data[j] = node_id;
        j++;
        Node& node = nodes[node_id];
        glm::quat r = node.rotation ;
        ((short *) (data+j))[0] = (short)(r.x*32767) ;
        j+=2;
        ((short *) (data+j))[0] = (short)(r.y*32767) ;
        j+=2;
        ((short *) (data+j))[0] = (short)(r.z*32767) ;
        j+=2;
        ((short *) (data+j))[0] = (short)(r.w*32767) ;
        j+=2;
    }

    return bone_buffer ;
    
}

Variant GLTF::getBoneData(const Variant& compressed){
    vector<float> x0 = getX() ;
    /*
    // light compression
    float* x =  compressed.getFloatArray() ;
    int j = 0 ;
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        node.rotation.x = x[j];
        j++;
        node.rotation.y = x[j];
        j++;
        node.rotation.z = x[j];
        j++;
        node.rotation.w = x[j];
        j++;
    }
    */
    
    // super compressed version
    byte* data = compressed.getByteArray();
    int num_set = compressed.getArrayLength()/9;
    for(int k=0;k<num_set;k++){
        int node_id = data[k*9];
        glm::quat& r = nodes[node_id].rotation;
        r.x = ((short *)(data+k*9+1))[0]/32767.0f ;
        r.y = ((short *)(data+k*9+3))[0]/32767.0f ;
        r.z = ((short *)(data+k*9+5))[0]/32767.0f ;
        r.w = ((short *)(data+k*9+7))[0]/32767.0f ;
        r = glm::normalize(r) ;
    }

    computeNodeMatrices();
    Variant bd = getBoneData() ;
    setX(x0) ;
    computeNodeMatrices();//TODO don't rely on holding state in the node matrices
    return bd;

}

std::vector<glm::mat4> GLTF::getBoneVector() {
	int num_bones = (int)nodes.size();
	std::vector<glm::mat4> bones ;
	bones.reserve(num_bones);
	for (int node_id = 0; node_id < nodes.size(); node_id++) {
		bones.push_back(nodes[node_id].transform) ;
	}
	return bones;
}

void GLTF::setModel(const byte* data, int data_length){
    vector<Vertex> new_vertices;
    vector<Triangle> new_triangles;
    this->materials.clear();
    this->images.clear();
    this->animations.clear();
	this->morph_names.clear();
	this->morph_triangles.clear();
    joint_to_node = map<int,map<int,int>>();
    nodes = vector<Node>();
    root_nodes = vector<int>();

    //printf ("num bytes: %d \n", data_length);
    //printf("GLB Magic: %u  JSON_CHUNK: %u\n", 0x46546C67, 0x4E4F534A);
    uint magic_num = *(uint *)data;

    if(magic_num == 0x46546C67){ // check if GLTF
        uint version = *(int *)(data + 4);
        uint total_length = *(int *)(data + 8);
        //printf("GLB file detected. Version : %u  Data Length: %u\n", version, total_length);
        //printf("File Length %d\n", data_length);

        uint JSON_length = *((uint *) (data + 12));// in bytes, not necessarily characters
        uint first_chunk_type = *((uint *) (data + 16));
        
        if(first_chunk_type == 0x4E4F534A){

            string header = string((char *) (data + 20), JSON_length);
            //printf("JSON: \n %s\n", header.c_str());
            
            json = Variant::parseJSON(header);
			
			//CSVLog json_output("model_header.json", "") ;
            //json_output.logLine(json.toString()) ;
            int bin_chunk_start = 20 + JSON_length ;
            
            if(bin_chunk_start %4 != 0){
                bin_chunk_start += 4 - (bin_chunk_start %4);
            }
            uint bin_length = *((uint*)(data+bin_chunk_start)) ;
            uint second_chunk_type = *((uint*)(data+bin_chunk_start+4)) ;
            if(second_chunk_type == 0x004E4942){
                bin = Variant(data+bin_chunk_start+8, bin_length);
                if(json["materials"].defined()){
                    int num_materials = json["materials"].getArrayLength();
                    for(int k=0;k<num_materials;k++){
                        addMaterial(k, json, bin);
                    }
                }else{ // literally no materials
                    printf("No materials present? Defaulting to white.\n");
                    Material m ; // make a default white
                    materials[0] = m;
                }

                boneless = true ; // will be set to false if loaded points find rigging data
                if(json["skins"].defined()){
                    vector<Variant> skins = json["skins"].getVariantArray();;
                    for(int s = 0; s < skins.size(); s++){
                        vector<Variant> skin_nodes = skins[s]["joints"].getVariantArray();
                        joint_to_node[s] = map<int,int>();
                        for(int k=0;k<skin_nodes.size();k++){
                            joint_to_node[s][k] = (int)(skin_nodes[k].getNumberAsFloat());
                        }

                        if(skins[s]["inverseBindMatrices"].defined()){
                            GLTF::Accessor ma = GLTF::access(skins[s]["inverseBindMatrices"].getInt(), json, bin);
                            if(ma.type != "MAT4" || ma.component_type != 5126){
                                printf("expected mat4 floats for inverse bind matrices, skipping.\n");
                                continue ;
                            }
                            int num_matrices= ma.data.getArrayLength()/16;
                            float* data = ma.data.getFloatArray(); // still held by Variant, not a memory leak

                            for(int k=0;k<num_matrices;k++){
                                glm::mat4 &m = joint_inverse_bind_matrix[joint_to_node[s][k]];
                                memcpy(&m, data + 16*k, 64);
                            }
                        }

                    }

                }

                nodes.resize(json["nodes"].getArrayLength());

                int default_scene = 0 ;
                if(json["scene"].defined()){
                    default_scene = json["scene"].getInt() ;
                }
                addScene(new_vertices, new_triangles, default_scene, json, bin) ;

                if(json["animations"].defined()){
                    vector<Variant> animations = json["animations"].getVariantArray();
                    for(Variant& animation : animations){
                        addAnimation(animation, json, bin);
                    }
                }

				loadVRMExtensions(json);

            }else{
                printf("Bin chunk not found after json !(got %d)\n", second_chunk_type);
            }
        }else{
            printf("first chunk not json: %u\n", first_chunk_type);
        }
    }else{
        printf("Not a GLB file! %d != %d\n", magic_num, 0x46546C67);
    }
    original_pose = getCurrentPose();
    printf("Loaded GLB. %d triangles\n", (int)new_triangles.size());
    setModel(new_vertices, new_triangles);

}

void GLTF::receiveTableData(std::string key, const Variant& data){
    if(data.defined()){
        //printf("Got key: %s size: %d\n", key.c_str(), data.getArrayLength());
        
        setModel(data.getByteArray(), data.getArrayLength());

        float size = 0 ;
        vec3 center(0,0,0);
        for(int k=0;k<3;k++){
            center[k] = (max[k]+ min[k])*0.5f ;
            size = fmax(size , fabs(max[k]-center[k]));
            size = fmax(size , fabs(min[k]-center[k]));
            
        }
        transform = mat4(1);
        //printf("size: %f, center %f,%f,%f\n", size, center[0], center[1], center[2]);
        
        computeNodeMatrices();
        applyTransforms();
        
    }else{
        printf("Got server response, but could not retrieve requested model(%s)!\n", key.c_str());
    }
}

GLTF::Accessor GLTF::access(int accessor_id, Variant& json, const Variant& bin){
    vector<Variant> accessors = json["accessors"].getVariantArray();
    vector<Variant> views = json["bufferViews"].getVariantArray() ;
    return access(accessor_id, accessors, views , bin);
   
}


GLTF::Accessor GLTF::access(int accessor_id, vector<Variant>& accessors, vector<Variant>& views, const Variant& bin){
     map<string,int> TYPE_LENGTH = {{"SCALAR",1},{"VEC2",2},{"VEC3",3},{"VEC4",4},{"MAT2",4},{"MAT3",9},{"MAT4",16}} ; // TODO static const
    map<int,int> COMPONENT_LENGTH = {{5120, 1},{5121, 1},{5122, 2},{5123, 2},{5125, 4},{5126, 4}}; // TODO static const

    
    auto& accessor = accessors[accessor_id];
    //printf("accessor:\n");
    //accessor.printFormatted();
    string type = accessor["type"].getString();
    uint c_type = (uint)(accessor["componentType"].getInt());
    int count = accessor["count"].getInt();
    int element_length = TYPE_LENGTH[type] * COMPONENT_LENGTH[c_type];

    auto& view = views[accessor["bufferView"].getInt()];
    int offset = 0 ;
    Variant v_offset = view["byteOffset"];
    if(v_offset.defined()){
        offset = v_offset.getInt();
    }
    Variant a_offset = accessor["byteOffset"] ;
    if(a_offset.defined()){
        offset += a_offset.getInt();
    }
    int byteLength = view["byteLength"].getInt();
    //printf("view:\n");
    //view.printFormatted();
    int stride = element_length;
    Variant v_stride = view["byteStride"] ;
    if(v_stride.defined()){
        stride = v_stride.getInt();
    }

    GLTF::Accessor result = {type, c_type, Variant()};
    result.data.ptr = (byte*)malloc(4 + element_length*count);
    ((int*)result.data.ptr)[0] = count * TYPE_LENGTH[type]; 
    for(int k=0;k<count;k++){
        memcpy(result.data.ptr + 4 +k*element_length, bin.ptr + 4 + offset + k*stride, element_length);
    }

    if(c_type == 5126){ // 32 bit float
        result.data.type_ = Variant::FLOAT_ARRAY;
    }else if(c_type == 5120 || 5121){ // signed and unsigned byte
        result.data.type_ = Variant::BYTE_ARRAY ;
    }else if(c_type == 5122 || c_type == 5123){ // shorts
        result.data.type_ = Variant::SHORT_ARRAY ;
    }else if(c_type == 5125){ // unsigned int
        result.data.type_ = Variant::INT_ARRAY ;
    }else{
        printf("unrecognized accessor component type, behavior undefined!\n");
        result.data.type_ = Variant::BYTE_ARRAY ;
    }

    return result ;

}



//TODO consider using quaternion down the hierarchy recursion instead of mat4 for better precision/speed.
void GLTF::addPrimitive(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
                        Variant& primitive, int node_id, const glm::mat4& transform, Variant& json, const Variant& bin){
    //printf("Adding primitive:\n");
    //primitive.printFormatted();
    auto node = json["nodes"][node_id];
    int skin_id = node["skin"].defined() ? node["skin"].getInt() : 0;

    if(primitive["mode"].defined() && primitive["mode"].getInt() != 4){
        printf("Primitive mode %d not implemented yet. Skipping.\n", primitive["mode"].getInt()); // TODO
        return ;
    }

    
    GLTF::Accessor pa = GLTF::access(primitive["attributes"]["POSITION"].getInt(), json, bin);
    if(pa.type != "VEC3" || pa.component_type != 5126){
        printf("expected vec3 floats for position, aborting.\n");
        return ;
    }

    //pa.data.printFormatted();

    int num_vertices = pa.data.getArrayLength()/3;
    //printf("num_vertices: %d \n", num_vertices);
    float* point_data = pa.data.getFloatArray(); // still held by Variant, not a memory leak

    bool has_normals = false;
    GLTF::Accessor na ;
    float* normal_data = nullptr; // still held by Variant, not a memory leak
    if(primitive["attributes"]["NORMAL"].defined()){
        na = GLTF::access(primitive["attributes"]["NORMAL"].getInt(), json, bin);
        if(na.type == "VEC3" && na.component_type == 5126){
            has_normals = true;
            normal_data = na.data.getFloatArray();
            //printf("Got normals!\n");
        }else{
            printf("Normals are a weird type, skipping %s : %d \n" , na.type.c_str(), na.component_type);
        }
    }
    
    bool has_texcoords = false;
    GLTF::Accessor ta ;
    float* texcoords_data = nullptr; // still held by Variant, not a memory leak
    if(primitive["attributes"]["TEXCOORD_0"].defined()){
        ta = GLTF::access(primitive["attributes"]["TEXCOORD_0"].getInt(), json, bin);

        if(ta.type == "VEC2" && ta.component_type == 5126){
            has_texcoords = true;
            texcoords_data = ta.data.getFloatArray();
            //printf("Got Texture coordinates!\n");
        }else{
            printf("Texture coordinates are a weird type, skipping %s : %d \n" , ta.type.c_str(), ta.component_type);
        }
    }

    bool has_weights = false;
    GLTF::Accessor wa ;
    GLTF::Accessor ja;
    float* weight_data = nullptr; // still held by Variant, not a memory leak
    uint* joint_data = nullptr; // careful, could leak memory
    if(primitive["attributes"]["WEIGHTS_0"].defined()){
        wa = GLTF::access(primitive["attributes"]["WEIGHTS_0"].getInt(), json, bin);
        ja = GLTF::access(primitive["attributes"]["JOINTS_0"].getInt(), json, bin);
        if(wa.type == "VEC4" && wa.component_type == 5126){
            has_weights = true;
            weight_data = wa.data.getFloatArray();
            if(ja.component_type == 5123){
                //printf("unsigned short joints\n");
                int num_joints = ja.data.getArrayLength() ;
                short* shorts = ja.data.getShortArray();
                joint_data = (uint*)malloc(4*num_joints);
                for(int k=0;k<num_joints;k++){
                    joint_data[k] = (unsigned short)shorts[k];
                }
            }else if(ja.component_type == 5125){
                //printf("unsigned int joints\n");
                int num_joints = ja.data.getArrayLength() ;
                int* ints = ja.data.getIntArray();
                joint_data = (uint*)malloc(4*num_joints);
                for(int k=0;k<num_joints;k++){
                    joint_data[k] = (unsigned int)ints[k];
                }
            }else if(ja.component_type == 5121){
                //printf("unsigned byte joints\n");
                int num_joints = ja.data.getArrayLength() ;
                byte* bytes = ja.data.getByteArray();
                joint_data = (uint*)malloc(4*num_joints);
                for(int k=0;k<num_joints;k++){
                    joint_data[k] = bytes[k];
                }
            }else{
                printf("joint component type unsafe: %d \n " , ja.component_type);
            }
        }else{
            printf("Weights are a weird type, skipping %s : %d \n" , ta.type.c_str(), ta.component_type);
        }
    }


    uint* index_data = nullptr; // this one you need to be careful
    int num_indices = 0 ;

    int start_vertices = (int)vertices.size();
    //printf("Start vertices: %d \n", start_vertices); 

    if(primitive["indices"].defined()){
        //printf("Found indices!\n");
        GLTF::Accessor ia = GLTF::access(primitive["indices"].getInt(), json, bin);
        if(ia.type == "SCALAR"){
            if(ia.component_type == 5121){
                //printf("unsigned byte indices, technically valid, but not yet implemented, aborting\n"); // TODO
				//printf("unsigned short indices\n");
				num_indices = ia.data.getArrayLength();
				byte* bytes = ia.data.getByteArray();
				index_data = (uint*)malloc(4 * num_indices);
				for (int k = 0; k < num_indices; k++) {
					index_data[k] = (unsigned char)bytes[k];
				}
            }else if(ia.component_type == 5123){
                //printf("unsigned short indices\n");
                num_indices = ia.data.getArrayLength();
                short* shorts = ia.data.getShortArray();
                index_data = (uint*)malloc(4*num_indices);
                for(int k=0;k<num_indices;k++){
                    index_data[k] = (unsigned short)shorts[k];
                }
            }else if(ia.component_type == 5125){
                //printf("unsigned int indices\n");
                num_indices = ia.data.getArrayLength();
                int* ints = ia.data.getIntArray();
                index_data = (uint*)malloc(4*num_indices);
                for(int k=0;k<num_indices;k++){
                    index_data[k] = (unsigned int)ints[k];
                }
            }else{
                printf("Indices are not a valid type ( %d)  aborting\n", ia.component_type);
                return ;
            }
        }else{
            printf("Indices are not scalar, aborting\n");
            return ;
        }
    }else{ // If no indices defined
        // indices are sequential
        num_indices = num_vertices;
        index_data = (uint*)malloc(4*num_indices);
        for(int k=0;k<num_indices;k++){
            index_data[k] = k ;
        }
    }

    int material = 0 ;
    if(primitive["material"].defined()){
        material = primitive["material"].getInt();
    }else{
        printf("Failed to get material property!\n");
    }
    if(!json["materials"][material].defined()){
        printf("Failed to find material %d for primitive \n",  material);
    }

	std::vector<GLTF::Accessor> morph_point ;
	std::vector<GLTF::Accessor> morph_normal ;
	std::vector<float*> morph_point_data;
	std::vector<float*> morph_normal_data; // points to data held by accessors so don't need to clean
	if (primitive["targets"].defined()) {
		int num_morphs = primitive["targets"].getArrayLength() ;
		if(num_morphs != 0){
			if(num_morphs != morph_names.size()){
				printf("Morph targets in primitive doesn't match names in mesh, can't identify where this data goes!\n");
			}else{
				for (int i = 0; i < num_morphs ; i++) {
					morph_point.push_back(GLTF::access(primitive["targets"][i]["POSITION"].getInt(), json, bin));
					morph_normal.push_back(GLTF::access(primitive["targets"][i]["NORMAL"].getInt(), json, bin));
					morph_point_data.push_back(morph_point[i].data.getFloatArray()) ;
					morph_normal_data.push_back(morph_normal[i].data.getFloatArray());
				}

			}
		}
		

	}

    for(int k=0;k<num_vertices;k++){
        Vertex v ;
        v.position = vec3(point_data[3*k], point_data[3*k+1],point_data[3*k+2]) ;
        v.color_mult = vec4(1,1,1,1);
        if(has_normals){
            v.normal = vec3(normal_data[3*k], normal_data[3*k+1], normal_data[3*k+2]) ;
        }
        if(has_texcoords){
            v.tex_coord = vec2(texcoords_data[2*k], texcoords_data[2*k+1]) ;
        }
        if(has_weights){
            v.weights = vec4(weight_data[4*k], weight_data[4*k+1], weight_data[4*k+2], weight_data[4*k+3]) ;
            v.joints = ivec4(joint_to_node[skin_id][joint_data[4*k]],
                            joint_to_node[skin_id][joint_data[4*k+1]],
                            joint_to_node[skin_id][joint_data[4*k+2]],
                            joint_to_node[skin_id][joint_data[4*k+3]]) ;
            float scale = 1.0f/(v.weights[0] + v.weights[1]+v.weights[2] + v.weights[3]);
            v.weights *= scale; ;
            boneless = false ; 
        }else{
            v.weights = vec4(1,0,0,0);
            v.joints = ivec4(node_id,0,0,0);
        }

		for(int i=0;i<morph_point.size();i++){
			v.morph_position.push_back( vec3(morph_point_data[i][3*k], morph_point_data[i][3 * k+1], morph_point_data[i][3 * k+2])) ;
			v.morph_normal.push_back(vec3(morph_normal_data[i][3 * k], morph_normal_data[i][3 * k + 1], morph_normal_data[i][3 * k + 2]));
		}
		
        vertices.push_back(v);
    }

    for(int k=0;k<num_indices;k+=3){
        //printf("Triangle: %d , %d , %d\n", (int)index_data[k]+start_vertices, (int)index_data[k+1]+start_vertices,(int)index_data[k+2]+start_vertices);
        triangles.push_back({(int)index_data[k]+start_vertices,
                        (int)index_data[k+1]+start_vertices, 
                        (int)index_data[k+2]+start_vertices,
                        material});
    }

	

    free(index_data);
    if(joint_data != nullptr){
        free(joint_data);
    }
}

void GLTF::addMaterial(int material_id, Variant& json, const Variant& bin){
    if(this->materials.find(material_id) == this->materials.end()){
        //printf("Adding material %d!\n", material_id);
        Variant m_json = json["materials"][material_id];
        //m_json.printFormatted();
        Material mat ;
        if(m_json["name"].defined()){
            //printf("Got name!\n");
            mat.name = m_json["name"].getString();
        }
        // Doublesided can appear in two different places, prefer deeper one
        if(m_json["pbrMetallicRoughness"]["doubleSided"].defined()){
            //printf("Got double sided!\n");
            mat.double_sided = m_json["pbrMetallicRoughness"]["doubleSided"].getInt() > 0;
        }else if(m_json["doubleSided"].defined()){
            //printf("Got double sided!\n");
            mat.double_sided = m_json["doubleSided"].getInt() > 0;
        }
        if(m_json["pbrMetallicRoughness"]["metallicFactor"].defined()){
            //printf("Got metallic!\n");
            mat.metallic = m_json["pbrMetallicRoughness"]["metallicFactor"].getNumberAsFloat();
        }

        if(m_json["pbrMetallicRoughness"]["roughnessFactor"].defined()){
            //printf("Got roughness!\n");
            mat.roughness = m_json["pbrMetallicRoughness"]["roughnessFactor"].getNumberAsFloat();
        }

        if(m_json["pbrMetallicRoughness"]["baseColorFactor"].defined()){
            //printf("Got base color!\n");
            Variant c = m_json["pbrMetallicRoughness"]["baseColorFactor"] ;
            mat.color = vec3(c[0].getNumberAsFloat(), c[1].getNumberAsFloat(), c[2].getNumberAsFloat());
        }

        if(m_json["pbrMetallicRoughness"]["baseColorTexture"]["index"].defined()){
            //printf("Got base color texture index for %d!\n", material_id);
            int texture_id = m_json["pbrMetallicRoughness"]["baseColorTexture"]["index"].getInt();
            mat.image = json["textures"][texture_id]["source"].getInt();
            mat.texture = addImage(mat.image, json, bin);
        }else if(m_json["pbrMetallicRoughness"]["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseTexture"]["index"].defined()){
            //printf("Got diffuse color texture index for %d!\n", material_id);
            int texture_id = m_json["pbrMetallicRoughness"]["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseTexture"]["index"].getInt();
            mat.image = json["textures"][texture_id]["source"].getInt();
            mat.texture = addImage(mat.image, json, bin);
        }else{
            //printf("No texture index for %d!\n", material_id);
            mat.texture = false;
        }

        this->materials[material_id] = mat ;
        this->model_changed = true;
    }
}

bool GLTF::addImage(int image_id, Variant& json, const Variant& bin){
    //printf("Adding image %d!\n", image_id);
    if(this->images.find(image_id) == this->images.end()){
        Image& img = this->images[image_id] ;
        Variant i_json = json["images"][image_id];
        if(!i_json.defined()){
            printf("Material referencing image not found ( %d)!\n", image_id);
            return false;
        }
        //i_json.printFormatted();
        if(i_json["name"].defined()){
            img.name = i_json["name"].getString();
            //printf("Got image name! %s\n", img.name.c_str());
        }
        if(!json["bufferViews"][i_json["bufferView"]].defined()){
            printf("No buffer view on image, external resources not supported, aborting texture load.\n");
            return false;
        }

        auto view = json["bufferViews"][i_json["bufferView"]];
        int offset = 0 ;
        if(view["byteOffset"].defined()){
            offset = view["byteOffset"].getInt();
        }
        int byteLength = view["byteLength"].getInt();
        byte* pixels = stbi_load_from_memory(bin.ptr + 4 + offset, byteLength, &img.width, &img.height, &img.channels, 0) ;
        
        img.data = Variant(pixels,img.width*img.height*img.channels);
        free(pixels);
        //printf("Loaded texture: %ix%ix%i = %d \n", img.width, img.height, img.channels, byteLength);
        return true ;
    }
    return true;
}

void GLTF::addMesh(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
                   int mesh_id, int node_id, const glm::mat4& transform, Variant& json, const Variant& bin){
    //printf("Adding mesh %d!\n", mesh_id);

	if(json["meshes"][mesh_id]["extras"]["targetNames"].defined()){
		int num_morphs = json["meshes"][mesh_id]["extras"]["targetNames"].getArrayLength() ;
		if(num_morphs != 0 && morph_names.size() == 0){
			printf("Found GLTF morph targets:\n");
			for (int i = 0; i < num_morphs; i++) {
				morph_names.push_back(json["meshes"][mesh_id]["extras"]["targetNames"][i].getString());
				printf("%s,", morph_names[i].c_str()) ;
			}
			printf("\n");
		}

	}

    auto primitives = json["meshes"][mesh_id]["primitives"].getVariantArray();
    for(int k=0;k<primitives.size();k++){
        addPrimitive(vertices, triangles, primitives[k], node_id, transform, json, bin);
    }
}

void GLTF::addNode(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
                   int node_id, const glm::mat4& transform, Variant& json, const Variant& bin){
    auto node = json["nodes"][node_id];
    Node& node_struct = nodes[node_id];
    if(node["name"].defined()){
        node_struct.name = node["name"].getString();
    }

    mat4 new_transform = transform ;
    if(node["matrix"].defined()){
        printf("Node has matrix not yet implemented!\n");
        //new_transform *= M;
    }else{
        Variant tv = node["translation"];
        if(tv.defined()){
            vec3 t = vec3(tv[0].getNumberAsFloat(), tv[1].getNumberAsFloat(), tv[2].getNumberAsFloat());
            node_struct.translation = t;
            node_struct.base_translation = t;
            new_transform = glm::translate(new_transform, t);
        }
        Variant rv = node["rotation"] ;
        if(rv.defined()){
            // GLB is XYZW but GLM:quat is WXYZ
            glm::quat qrot(rv[3].getNumberAsFloat(), rv[0].getNumberAsFloat(), rv[1].getNumberAsFloat(), rv[2].getNumberAsFloat() );
            node_struct.rotation = qrot;
            node_struct.base_rotation = qrot;
            new_transform *= glm::mat4_cast(qrot);
        }
        Variant sv = node["scale"];
        
        if(sv.defined()){
            vec3 s = vec3(sv[0].getNumberAsFloat(), sv[1].getNumberAsFloat(), sv[2].getNumberAsFloat());
            node_struct.scale = s;
            node_struct.base_scale = s;
            new_transform = glm::scale(new_transform, s);
        }
    }
    
    if(node["mesh"].defined()){
        int mesh_id = node["mesh"].getInt();
        
        addMesh(vertices, triangles, mesh_id, node_id, new_transform, json, bin);
    }
    if(node["children"].defined()){
        auto children_ids = node["children"];
        for(int k=0;k<children_ids.getArrayLength();k++){
            int child_id = children_ids[k].getInt();
            node_struct.children.push_back(child_id);
            addNode(vertices, triangles, child_id, new_transform, json, bin);
            nodes[child_id].parent = node_id ;
        }
    }
}

void GLTF::addScene(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
                    int scene_id, Variant& json, const Variant& bin){
    //printf("Adding scene %d!\n", scene_id);
    auto nodes = json["scenes"][scene_id]["nodes"];
    glm::mat4 ident(1);
    for(int k=0;k<nodes.getArrayLength();k++){
        int node_id = nodes[k].getInt();
        root_nodes.push_back(node_id);
        addNode(vertices, triangles, node_id, ident, json, bin);
    }
}


void GLTF::addAnimation(Variant& animation_json, Variant& json, const Variant& bin){
    //animation_json.printFormatted();

    Animation animation ;
    animation.name = animation_json["name"].getString();
    //printf("Adding animation %d: %s!\n", (int)this->animations.size(), animation.name.c_str());

    vector<Variant> accessors = json["accessors"].getVariantArray();
    vector<Variant> views = json["bufferViews"].getVariantArray() ;

    vector<Variant> samplers_json = animation_json["samplers"].getVariantArray() ;
    vector<Variant> channels_json = animation_json["channels"].getVariantArray() ;
    for(Variant& channel_json : channels_json){
        AnimationChannel channel ;
        channel.node = (int)channel_json["target"]["node"].getNumberAsFloat();
        string type = channel_json["target"]["path"].getString();
        int vec_size = 0 ;
        if(type == "rotation"){
            channel.path = ROTATION;
            vec_size = 4;
        }else if(type == "scale"){
            channel.path = SCALE;
            vec_size = 3 ;
        }else if(type == "translation"){
            channel.path = TRANSLATION;
            vec_size = 3 ;
        }else{
            printf("Unrecognized animation channel path: %s . Aborting.\n", type.c_str());
            return;
        }

        Variant& sampler = samplers_json[channel_json["sampler"].getInt()];
        Accessor input = access(sampler["input"].getInt(), accessors, views, bin);
        Accessor output = access(sampler["output"].getInt(), accessors, views, bin);
        int num_samples = input.data.getArrayLength();
        float* times = input.data.getFloatArray();
        float* values = output.data.getFloatArray();
        bool all_same = true;
        for(int k=0;k<num_samples;k++){
            std::pair<float, vec4> sample;
            sample.first = times[k];
            if(vec_size == 4){
                sample.second = {values[k*4],values[k*4+1],values[k*4+2],values[k*4+3]};  
            }else{
                sample.second = {values[k*3],values[k*3+1],values[k*3+2],0};
            }
            if (k != 0 && all_same) {
                vec4& q = sample.second;
                vec4& r = channel.samples[0].second;
                all_same &= r.x == q.x && r.y == q.y && r.z == q.z && r.w == q.w;
            }
            animation.duration = fmax(animation.duration,times[k]);
            channel.samples.push_back(sample);
        }
        if (all_same || num_samples <= 1) {
            //printf("identical animation channel discarded!\n");
        }else {
            animation.channels.push_back(channel);
        }
    }


    this->animations.push_back(animation); // TODO id is ignored here, could be a bug source if we ever load them out of order
}


void GLTF::loadVRMExtensions(Variant& json){


	//Extension for VRM 0.0 
	if (json["extensions"]["VRM"]["humanoid"]["humanBones"].defined()) {
		VRM_version = 0;
		vector<Variant> human_bones = json["extensions"]["VRM"]["humanoid"]["humanBones"].getVariantArray();
		for (Variant& bone : human_bones) {
			human_bone[bone["bone"].getString()] = bone["node"].getInt();
			//printf("%s : %d \n", bone["bone"].getString().c_str(), bone["node"].getInt() );
		}
	}
	if (json["extensions"]["VRM"]["firstPerson"].defined()) {
		first_person_bone = json["extensions"]["VRM"]["firstPerson"]["firstPersonBone"].getInt();
		//json["extensions"]["VRM"]["firstPerson"]["firstPersonBoneOffset"].printFormatted();
		first_person_offset.x = json["extensions"]["VRM"]["firstPerson"]["firstPersonBoneOffset"]["x"].getNumberAsFloat();
		first_person_offset.y = json["extensions"]["VRM"]["firstPerson"]["firstPersonBoneOffset"]["y"].getNumberAsFloat();
		first_person_offset.z = json["extensions"]["VRM"]["firstPerson"]["firstPersonBoneOffset"]["z"].getNumberAsFloat();
		//printf("First person : %d at (%f,%f,%f)\n", first_person_bone, first_person_offset.x, first_person_offset.y,first_person_offset.z);
	}

	// Extension for VRM 1.0
	if (json["extensions"]["VRMC_vrm"]["humanoid"]["humanBones"].defined()) {
		VRM_version = 1;
		std::map<std::string, Variant> human_bones = json["extensions"]["VRMC_vrm"]["humanoid"]["humanBones"].getObject();
		//printf("Found VRM 1.0 human bones:\n");
		for (auto& [name, bone] : human_bones) {
			human_bone[name] = bone["node"].getInt();
			//printf("%s : %d \n", name.c_str(), human_bone[name] );
		}
		first_person_bone = human_bone["head"];
		Variant offset = json["extensions"]["VRMC_vrm"]["firstPerson"]["lookAt"]["offsetFromHeadBone"];
		first_person_offset.x = offset[0].getNumberAsFloat();
		first_person_offset.y = offset[1].getNumberAsFloat();
		first_person_offset.z = offset[2].getNumberAsFloat();
		//printf("Head bone: %d Offset: %f, %f, %f\n", first_person_bone, first_person_offset.x, first_person_offset.y, first_person_offset.z) ;
	}

	if (json["extensions"]["VRMC_springBone"].defined()) {

		std::vector<Variant> json_colliders = json["extensions"]["VRMC_springBone"]["colliders"].getVariantArray();
		for (Variant& jc : json_colliders) {
			if (jc["shape"]["sphere"].defined()) {
				SphereCollider sc;
				sc.node = jc["node"].getInt();
				sc.radius = jc["shape"]["sphere"]["radius"].getNumberAsFloat();
				sc.offset.x = jc["shape"]["sphere"]["offset"][0].getNumberAsFloat();
				sc.offset.y = jc["shape"]["sphere"]["offset"][1].getNumberAsFloat();
				sc.offset.z = jc["shape"]["sphere"]["offset"][2].getNumberAsFloat();
				//printf("Sphere collider found node: %d radius %f, poffset: %f,%f,%f\n", sc.node, sc.radius, sc.offset.x, sc.offset.y, sc.offset.z);
				colliders.push_back(sc);
			}
			else {
				printf("Non-sphere collider found in VRM spring bones extension? Skipping.\n");
			}
		}

		std::vector<Variant> json_collider_groups = json["extensions"]["VRMC_springBone"]["colliderGroups"].getVariantArray();
		for (Variant& jcg : json_collider_groups) {
			CollisionGroup cg;
			cg.name = jcg["name"].getString();
			std::vector<Variant> cl = jcg["colliders"].getVariantArray();
			//printf("collision group %s has %d elements.\n", cg.name.c_str(), (int)cl.size());
			for (Variant& c : cl) {
				cg.colliders.push_back(c.getInt());
			}
			collision_groups.push_back(cg);
		}

		std::vector<Variant> json_springs = json["extensions"]["VRMC_springBone"]["springs"].getVariantArray();
		for (Variant& jchain : json_springs) {
			SpringChain chain;
			chain.name = jchain["name"].getString();
			std::vector<Variant> json_joints = jchain["joints"].getVariantArray();
			for (Variant& j_joint : json_joints) {
				//j_joint.printFormatted();
				SpringJoint joint;
				joint.node = j_joint["node"].getInt();
				if (j_joint["hitRadius"].defined()) { // the final node only has node
					joint.radius = j_joint["hitRadius"].getNumberAsFloat();
					joint.stiffness = j_joint["stiffness"].getNumberAsFloat();
					joint.drag = j_joint["dragForce"].getNumberAsFloat();
					joint.gravity.x = j_joint["gravityDir"][0].getNumberAsFloat();
					joint.gravity.y = j_joint["gravityDir"][1].getNumberAsFloat();
					joint.gravity.z = j_joint["gravityDir"][2].getNumberAsFloat();
					joint.gravity *= j_joint["gravityPower"].getNumberAsFloat();
					//printf("Joint: node = %d, radius = %f, stiffness = %f, drag = %f, gravity = %f,%f,%f\n", joint.node, joint.radius, joint.stiffness, joint.drag, joint.gravity.x, joint.gravity.y, joint.gravity.z);
					chain.joints.push_back(joint);
				}
			}

			if(jchain["colliderGroups"].defined()){
				std::vector<Variant> collider_groups = jchain["colliderGroups"].getVariantArray() ;
				for (Variant& j_group : collider_groups) {
					chain.collider_groups.push_back(j_group.getInt()) ;
				}
			}
		
			spring_chains.push_back(chain);
		}

	}


}

// Compacts the given vertices and sets the model to them
void GLTF::setModel(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles){
    this->vertices = vertices;
    this->triangles = triangles;

    vector<bool> unset_normal ;
    vector<bool> referenced ;
    for(int k=0; k<vertices.size(); k++){
        unset_normal.push_back(glm::length(vertices[k].normal) < 0.01);
        referenced.push_back(false);
    }

    // set undefined normals by summing up touching triangles
    for(int k=0; k < this->triangles.size(); k++){
        Triangle& t = this->triangles[k] ;
        vec3 n = this->getNormal(t);
        if(unset_normal[t.A]){
            this->vertices[t.A].normal += n ;
        }
        if(unset_normal[t.B]){
            this->vertices[t.B].normal += n ;
        }
        if(unset_normal[t.C]){
            this->vertices[t.C].normal += n ;
        }
        referenced[t.A] = true;
        referenced[t.B] = true;
        referenced[t.C] = true;
    }

    // removed vertices not in triangles
    vector<int> new_index ;
    int i = 0 ;
    vector<Vertex> new_vertices ;
    for(int k=0;k<this->vertices.size();k++){
        new_index.push_back(i);
        if(referenced[k]){
            this->vertices[k].normal = glm::normalize(this->vertices[k].normal) ; //normalize normals
            new_vertices.push_back(this->vertices[k]);
            i++;
        }  
    }


    for(int k=0; k < this->triangles.size(); k++){
        Triangle& t = this->triangles[k] ;
        t.A = new_index[t.A];
        t.B = new_index[t.B];
        t.C = new_index[t.C];
    }
    this->vertices = new_vertices ;
    
    
    computeInvMatrices();
    setStiffnessByDepth();
    this->model_changed = true;
    this->position_changed = true;

    
    applyTransforms();
   

    //printf("Total vertices: %d\n",(int) this->vertices.size());
    //printf("Total triangles: %d\n",(int) this->triangles.size());
}

// hashes a vertex to allow duplicates to be detected and merged
int GLTF::hashVertex(vec3 v){
    return Variant(v).hash();
}

// hashes a vertex to allow duplicates to be detected
        // rounds vertex to tolerance first
int64_t GLTF::hashVertex(const glm::vec3& v, float tolerance) {
   return  (int64_t)(v.x / tolerance) ^ (((int64_t)(v.y / tolerance) * 1337) << 16) ^ (((int64_t)(v.z / tolerance) * 7) << 32);
}

vec3 GLTF::getNormal(Triangle t){
    vec3 AB = this->vertices[t.B].position - this->vertices[t.A].position;
    vec3 AC = this->vertices[t.C].position - this->vertices[t.A].position;
    return glm::normalize(glm::cross(AB,AC));
}

void GLTF::setTetraModel(glm::vec3 center, float size){
    Vertex A, B, C, D ;
    A.position = vec3(center[0] + size ,center[1] + size, center[2]-size);
    A.weights = vec4(1,0,0,0);
    A.color_mult = vec4(0,0,0,1) ; // black
    B.position = vec3(center[0] - size,center[1] + size,center[2]-size);
    B.color_mult = vec4(0,1,0,1) ; // green
    B.weights = vec4(1,0,0,0);
    C.position = vec3(center[0], center[1] - size, center[2]-size);
    C.color_mult = vec4(0,0,1,1) ; // blue
    C.weights = vec4(1,0,0,0);
    D.position = vec3(center[0], center[1], center[2] + size);
    D.color_mult = vec4(1,0,0,1) ; // red
    D.weights = vec4(1,0,0,0);

    vector<Vertex> v ;
    v.push_back(A);
    v.push_back(B);
    v.push_back(C);
    v.push_back(D);

    vector<Triangle> t;
    t.push_back({1,0,2,0});
    t.push_back({3,1,2,0});
    t.push_back({0,3,2,0});
    t.push_back({0,1,3,0});
    
    Material m ;
    materials[0] = m;
    transform = mat4(1);
    nodes =  vector<Node>();
    Node n ;
    n.transform = mat4(1);
    nodes.push_back(n);
    root_nodes = vector<int>();
    root_nodes.push_back(0);

    setModel(v, t);
}


// Sets the model to a single axis aligned bounding box (can be used as a placeholder or for debugging without a model)
void GLTF::setBoundingBoxModel(const glm::vec3& min, const glm::vec3& max, const glm::vec4 color ){
	vector<vec3> vertices;
	vertices.emplace_back(min.x, min.y, min.z); // 0
	vertices.emplace_back(max.x, min.y, min.z); // 1
	vertices.emplace_back(min.x, max.y, min.z); // 2
	vertices.emplace_back(max.x, max.y, min.z); // 3
	vertices.emplace_back(min.x, min.y, max.z); // 4
	vertices.emplace_back(max.x, min.y, max.z); // 5
	vertices.emplace_back(min.x, max.y, max.z); // 6
	vertices.emplace_back(max.x, max.y, max.z); // 7

	vector<vector<int>> faces;
	faces.push_back(vector<int>({ 0, 4, 6, 2 })); // min x
	faces.push_back(vector<int>({ 1, 3, 7, 5 })); // max x
	faces.push_back(vector<int>({ 0, 1, 5, 4 })); // min y
	faces.push_back(vector<int>({ 2, 6, 7, 3 })); // max y
	faces.push_back(vector<int>({ 0, 2, 3, 1 })); // min z
	faces.push_back(vector<int>({ 4, 5, 7, 6 })); // max z

	vector<Vertex> v;
	vector<Triangle> t;

	for(auto& p : vertices){
		Vertex A ;
		A.position = p ;
		A.weights = vec4(1, 0, 0, 0);
		A.color_mult = color ;
		v.push_back(A);
	}

	for(auto& f : faces){
		t.push_back({f[0],f[1],f[2],0}) ;
		t.push_back({f[2],f[3],f[0],0 });
	}

	Material m;
	materials[0] = m;
	transform = mat4(1);
	nodes = vector<Node>();
	Node n;
	n.transform = mat4(1);
	nodes.push_back(n);
	root_nodes = vector<int>();
	root_nodes.push_back(0);

	setModel(v, t);
}

// Sets the model to a polyhedron of the given color (Can be used to generate visuals for ConvexShape objects)
void GLTF::setPolyhedronModel(const std::vector<glm::vec3>& vertices,const std::vector<std::vector<int>>& faces, glm::vec4 color){
    vector<Vertex> v ;
    vector<Triangle> t;
    for(const vector<int>& face : faces){
        const vec3& A = vertices.at(face.at(0)) ;
        const vec3& B = vertices.at(face.at(1)) ;
        const vec3& C = vertices.at(face.at(2)) ;
        vec3 normal = glm::normalize(glm::cross(B - A, C - A));
        vector<int> new_face ;
        for(int k=0;k<face.size();k++){ // duplicate vertices by face so as not to smooth normals
            Vertex p;
            p.position = vertices[face[k]];
            p.weights = vec4(1,0,0,0);
            p.color_mult = color;
            p.normal = normal ;
            new_face.push_back((int)v.size());
            v.push_back(p);
        }

        for(int k=1; k < new_face.size()-1;k++){
            t.push_back({new_face[0], new_face[k], new_face[k+1], 0});
        }
        
    }

    Material m ;
    materials[0] = m;
    transform = mat4(1);
    nodes =  vector<Node>();
    Node n ;
    n.transform = mat4(1);
    nodes.push_back(n);
    root_nodes = vector<int>();
    root_nodes.push_back(0);
    boneless = true;
    setModel(v, t);

}

// Given a ray in model space (p + v*t) return the t value of the nearest collision
// with the given triangle
// return negative if no collision
float EPSILON = 0.00001f;
float GLTF::trace(Triangle tri, const vec3 &p, const vec3 &v){
    vector<Vertex>& x = this->vertices ;
    vec3& A = x[tri.A].transformed_position ;
    vec3 AB = x[tri.B].transformed_position - A ;
    vec3 AC = x[tri.C].transformed_position - A ;
    vec3 h = glm::cross(v, AC);
    float a = glm::dot(AB, h);
	if (a < EPSILON){
        return -1;
    }
	float f = 1.0f / a;
    vec3 s = p - A ;
    float u = glm::dot(s, h) * f ;
	if (u < 0 || u > 1){
        return -1;
    }
    vec3 q = glm::cross(s, AB);
    float w = glm::dot(v, q) * f ;
	if (w < 0 || u + w > 1){
        return -1;
    }
    float t = glm::dot(AC, q) * f ;
    if( t > EPSILON){
        return t;
    }else{
        return -1;
    }
}

// Given a ray in model space (p + v*t) return the t value of the nearest collision with the bounding box
        // return negative if no collision
float GLTF::rayTraceBoundingBox(const glm::vec3& p, const glm::vec3& v) {
	return ::rayTraceBoundingBox(p,v,min,max); // in Utilities.h
}

// Given a ray in model space (p + v*t) return the t value of the nearest collision
// return negative if no collision
float GLTF::rayTrace(const vec3 &p, const vec3 &v){
    if (rayTraceBoundingBox(p, v) < 0) {
        return -1.0f;
    }

    last_traced_tri = -1 ;
    float min_t = std::numeric_limits<float>::max() ;
    for(int k=0;k<this->triangles.size();k++){
        float t = this->trace(this->triangles[k],p,v);
        if(t > 0 && t < min_t){
            min_t = t ;
            last_traced_tri  = k ;
        }
    }
    if(min_t < std::numeric_limits<float>::max() ){
        return min_t ;
    }
    return -1;
}

// Returns the index of the closest vertex to the given point
int GLTF::getClosestVertex(const glm::vec3 &p){
    int best = 0;
    vec3 diff = p-vertices[0].transformed_position;
    float best_dist = glm::dot(diff, diff);
    for(int k=1;k<vertices.size();k++){
        diff = p-vertices[k].transformed_position;
        float dist = glm::dot(diff, diff);
        if(dist < best_dist){
            best_dist = dist ;
            best = k ;
        }
    }

    return best ;
}

// Computes node transform matrices from their components and nesting
void GLTF::computeNodeMatrices(int node_id, const glm::mat4& transform){
    Node& node = nodes[node_id];
    node.transform = transform ;
    node.transform = glm::translate(node.transform, node.translation);
    node.transform *= glm::mat4_cast(node.rotation);
    node.transform = glm::scale(node.transform, node.scale);
    
    for(int k=0;k<node.children.size();k++){
        computeNodeMatrices(node.children[k], node.transform);
    }
	node.bone_to_model = node.transform ;
    node.transform = node.transform * node.mesh_to_bone;
}

void GLTF::computeNodeMatrices(){
    for(int k=0;k<root_nodes.size();k++){
        computeNodeMatrices(root_nodes[k], this->transform);
    }
}

 // Computes base vertices for skinned vertices so they can later use apply node transforms
void GLTF::computeInvMatrices(){
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        node.mesh_to_bone = mat4(1) ;
        node.bone_to_mesh = mat4(1) ;
    }
    for(int k=0;k<root_nodes.size();k++){
        computeNodeMatrices(root_nodes[k], glm::mat4(1.0f));
    } 
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        if(joint_inverse_bind_matrix.find(node_id) != joint_inverse_bind_matrix.end()){
            node.mesh_to_bone = joint_inverse_bind_matrix[node_id];
            node.bone_to_mesh = glm::inverse(node.mesh_to_bone);
        }else{
            // Root transform should not be included in inverse bind
            int root = node_id;
            while(nodes[root].parent!=-1){
                root = nodes[root].parent ;
            }
            node.bone_to_mesh = glm::inverse(nodes[root].transform) * node.transform ;
            node.mesh_to_bone = glm::inverse(node.bone_to_mesh) ;
        }
    }
}

void GLTF::setStiffnessByDepth(){
    for(int k=0;k<root_nodes.size();k++){
        setStiffnessByDepth(root_nodes[k], 1);
        nodes[root_nodes[k]].stiffness = 100000;
    } 
}
void GLTF::setStiffnessByDepth(int node_id, float stiffness){
    Node& node = nodes[node_id];
    node.stiffness = stiffness;
    for(int k=0;k<node.children.size();k++){
        setStiffnessByDepth(node.children[k], stiffness*stiffness_decay);
    }
}

// Applies current node transformed to skinned vertices
void GLTF::applyTransforms(){

    // need to flatten nodes into a vector since we're fetching them in the vertex loop
    //TODO is this still significant?
    vector<mat4> node_matrix(nodes.size()) ;
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        node_matrix[node_id] = node.transform ;
    }
    
    for(auto& v : vertices){
        vec4 p = {0,0,0,0};
        vec4 n = {0,0,0,0};
        bool has_rigging = 0 ;
        const auto& joints = v.joints ;
        const auto& weights = v.weights ;
        for(int j=0;j<4;j++){
            if(weights[j] > 0){
                //printf("joint: %d\n", joints[j]);
                mat4& transform = node_matrix[joints[j]];
                p += transform * vec4(v.position,1) * weights[j] ;
                n += transform * vec4(v.normal,0) * weights[j];
                has_rigging = true;
            }
        }
        
        if(has_rigging){
            v.transformed_position = p;
            v.transformed_normal = n ;
        }else{ // no rigging, apply transform to starting position of vertices
            v.transformed_position = this->transform*vec4(v.position,1);
            v.transformed_normal = this->transform*vec4(v.normal,0);
        }
        v.transformed_normal = glm::normalize(v.transformed_normal);
    }
    
    // caslculate AABB on transformed data
    this->min = { 9999999,9999999,9999999 };
    this->max = { -9999999,-9999999,-9999999 };
    for (int k = 0;k < this->vertices.size(); k++) {
        auto& v = this->vertices[k].transformed_position;
        if (v.x < this->min.x)this->min.x = v.x;
        if (v.y < this->min.y)this->min.y = v.y;
        if (v.z < this->min.z)this->min.z = v.z;
        if (v.x > this->max.x)this->max.x = v.x;
        if (v.y > this->max.y)this->max.y = v.y;
        if (v.z > this->max.z)this->max.z = v.z;
    }

    this->position_changed = true;
}

// Sets active pose to the base pose
void GLTF::setToBasePose(){
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        node.translation = node.base_translation;
        node.scale = node.base_scale;
        node.rotation = node.base_rotation;
    }
    computeNodeMatrices();
}

//Sets the active pose and base pose to what they were whenthe file was loaded
void GLTF::setToOriginalPose() {
    for (int node_id = 0; node_id < nodes.size(); node_id++) {
        Node& node = nodes[node_id];
        Node& o = original_pose[node_id];
        node.translation = o.translation;
        node.scale = o.scale;
        node.rotation = o.rotation;
        node.base_translation = o.base_translation;
        node.base_scale = o.base_scale;
        node.base_rotation = o.base_rotation;
    }
    computeNodeMatrices();
}

// Returns transforms for the given animation 
 // Uses current values for transforms unaffected by animation (with weight=0), does not apply transforms to vertices
std::vector<GLTF::Node> GLTF::getPose(Animation& animation, float time) {

    // Copy current pose to maintain indices order and for elements not in animation
    std::vector<Node> result;
    for (Node& n : nodes) {
        Node r;
        r.rotation = n.rotation ;
        r.scale = n.scale ;
        r.translation = n.translation ;

        r.base_rotation = n.base_rotation ;
        r.base_scale = n.base_scale;
        r.base_translation = n.base_translation;
        r.animation_weight = 0.0f;
        result.push_back(r);
    }

    if (time < 0) {
        time = 0;
    }
    if (time > animation.duration) {
        time = animation.duration;
    }


    for (int c = 0; c < animation.channels.size(); c++) {
        AnimationChannel& channel = animation.channels[c];
        // Find the first frame after current time
        int end = channel.last_read; // start from last read key for this channel
        if (channel.samples[end].first > time) { // if it's in the future
            end = 0; // start from the beginning
        }
        // walk forward to find the first frame after this time
        while (channel.samples[end].first < time && end < channel.samples.size()) {
                end++;
        }
        channel.last_read = end;
        int start = end - 1; // start frame is the frame before that

        float t = 0.5f;
        if (end == channel.samples.size()) { // if time is beyond all keyframes
            end = start; // start and end will both be the last frame
        }else if (end == 0) {
            start = 0;
        } else {
            float start_time = channel.samples[start].first;
            float end_time = channel.samples[end].first;
            t = (time - start_time) / (end_time - start_time);
        }

        if (channel.path == SCALE) {
            result[channel.node].scale = channel.samples[start].second * (1 - t) + channel.samples[end].second * t;
            result[channel.node].animation_weight = 1.0f;
        }
        else if (channel.path == TRANSLATION) {
            result[channel.node].translation = channel.samples[start].second * (1 - t) + channel.samples[end].second * t;
            result[channel.node].animation_weight = 1.0f;
        }else if (channel.path == ROTATION) {
            auto start_quat = glm::quat(channel.samples[start].second[3], channel.samples[start].second[0], channel.samples[start].second[1], channel.samples[start].second[2]);
            auto end_quat = glm::quat(channel.samples[end].second[3], channel.samples[end].second[0], channel.samples[end].second[1], channel.samples[end].second[2]);
            if (t > 1 || t < 0) {
                printf("invalid t: %f\n", t);
            }
            result[channel.node].rotation = GLTF::slerp(start_quat, end_quat, t);
            result[channel.node].animation_weight = 1.0f;
        }
    }
    
    return result;
}


// same as above but takes int index int loaded animations already on this GLTF
std::vector<GLTF::Node> GLTF::getPose(int selected_animation, float time) {
    return getPose(animations[selected_animation], time);
}

// Returns the currently active pose
std::vector<GLTF::Node> GLTF::getCurrentPose() {
    // Copy current pose to maintain indices order and for elements not in animation
    std::vector<Node> result;
    for (Node& n : nodes) {
        Node r;
        r.rotation = n.rotation;
        r.scale = n.scale;
        r.translation = n.translation;

        r.base_rotation = n.base_rotation;
        r.base_scale = n.base_scale;
        r.base_translation = n.base_translation;
        r.animation_weight = 0.0f;
        result.push_back(r);
    }
    return result;
}

//Sets the current pose to the given pose (both main and base)
void GLTF::setPose(std::vector<GLTF::Node>& pose) {

    for (int k = 0; k < pose.size(); k++) {
        nodes[k].rotation = pose[k].rotation;
        nodes[k].scale = pose[k].scale;
        nodes[k].translation = pose[k].translation;
        nodes[k].base_rotation = pose[k].base_rotation;
        nodes[k].base_scale = pose[k].base_scale;
        nodes[k].base_translation = pose[k].base_translation;
    }
    computeNodeMatrices();
}

//Sets the base pose on the given pose to the nonbase pose of the base_source
void GLTF::setBasePose(std::vector<GLTF::Node>& pose, std::vector<GLTF::Node>& base_source) {
    for (int k = 0; k < pose.size(); k++) {
        pose[k].base_rotation = base_source[k].rotation;
        pose[k].base_scale = base_source[k].scale;
        pose[k].base_translation = base_source[k].translation;
    }
}

// Interpolates from pose A at t=0 to pose B at t = 1
std::vector<GLTF::Node> GLTF::blendPose(const std::vector<GLTF::Node>& A, const std::vector<GLTF::Node>& B, float t) {
    std::vector<Node> result;
    for (const Node& n : A) {
        Node r;
        r.rotation = n.rotation;
        r.scale = n.scale;
        r.translation = n.translation;

        r.base_rotation = n.base_rotation;
        r.base_scale = n.base_scale;
        r.base_translation = n.base_translation;
        r.animation_weight = 0.0f;
        result.push_back(r);
    }

    for (int k = 0; k < result.size(); k++) {
        result[k].rotation = slerp(A[k].rotation, B[k].rotation, t);
        result[k].scale = (A[k].scale *(1.0f-t)) + (B[k].scale* t);
        result[k].translation = (A[k].translation * (1.0f - t)) + (B[k].translation * t);
        result[k].animation_weight = fmax(A[k].animation_weight, B[k].animation_weight);
    }

    return result;
}

// Interpolates from pose A at t=0 to pose B at t = 1 with weights multipled by the node weights
// Any nodes which have 0 weight total will be loaded from A
std::vector<GLTF::Node> GLTF::blendPoseWeighted(const std::vector<GLTF::Node>& A, const std::vector<GLTF::Node>& B, float t) {
    std::vector<Node> result;
    for (const Node& n : A) {
        Node r;
        r.rotation = n.rotation;
        r.scale = n.scale;
        r.translation = n.translation;

        r.base_rotation = n.base_rotation;
        r.base_scale = n.base_scale;
        r.base_translation = n.base_translation;
        r.animation_weight = 0.0f;
        result.push_back(r);
    }

    for (int k = 0; k < result.size(); k++) {
        float a = A[k].animation_weight;
        float b = B[k].animation_weight;
        float rt = 0; // if only a or neither
        if (a < 0.0001f && b > 0.0001f) { // only b
            rt = 1.0f;
        }else if (a > 0.0001f && b > 0.0001f) {// both have weight
            rt = b * t / (a * (1.0f - t) + b * t); // relative blend
        }

        result[k].rotation = slerp(A[k].rotation, B[k].rotation, rt);
        result[k].scale = (A[k].scale * (1.0f - t)) + (B[k].scale * rt);
        result[k].translation = (A[k].translation * (1.0f - t)) + (B[k].translation * rt);
        result[k].animation_weight = fmax(A[k].animation_weight, B[k].animation_weight);
    }

    return result;
}

// Blends a set of poses with weights (multiplies weights on input poses as well)
// Nodes which have no node weight will use the first pose regardless of its weight
std::vector<GLTF::Node> GLTF::blendPoseWeighted(const std::vector<std::pair<std::vector<GLTF::Node>, float>> poses) {
    std::vector<Node> result;
    for (const Node& n : poses[0].first) {
        Node r;
        r.rotation = n.rotation;
        r.scale = n.scale;
        r.translation = n.translation;

        r.base_rotation = n.base_rotation;
        r.base_scale = n.base_scale;
        r.base_translation = n.base_translation;
        r.animation_weight = n.animation_weight * poses[0].second;
        result.push_back(r);
    }

    for (int k = 0; k < result.size(); k++) {
        for (int p = 1; p < poses.size(); p++) {
            float a = result[k].animation_weight;
            float b = poses[p].first[k].animation_weight * poses[p].second;
            float rt = 0; // if only a or neither
            if (a < 0.0001f && b > 0.0001f) { // only b
                rt = 1.0f;
            }
            else if (a > 0.0001f && b > 0.0001f) {// both have weight
                rt = b / (a + b);
            }

            result[k].rotation = slerp(result[k].rotation, poses[p].first[k].rotation, rt);
            result[k].scale = (result[k].scale * (1.0f - rt)) + (poses[p].first[k].scale * rt);
            result[k].translation = (result[k].translation * (1.0f - rt)) + (poses[p].first[k].translation * rt);
            result[k].animation_weight = a + b;
        }
    }

    return result;

}

glm::quat GLTF::slerp(glm::quat A, glm::quat B, float t){
    float cosTheta = glm::dot(A,B);
    if(cosTheta < 0){
        B*=-1.0f;
    }
    if(cosTheta <.0001 || cosTheta > 0.9999){
        glm::quat result = glm::quat(
            A.w*(1-t) + B.w*t,
            A.x*(1-t) + B.x*t,
            A.y*(1-t) + B.y*t,
            A.z*(1-t) + B.z*t);
            result = glm::normalize(result);
        return result ;
    }else{
        float angle = acos(cosTheta);
        glm::quat result = (sin((1.0f - t) * angle) * A + sin(t * angle) * B) / sin(angle);
        result = glm::normalize(result);
        //printf("result: %f %f %f %f\n", result.w, result.x, result.y, result.z);
        return result ;
    }
}

glm::vec3 GLTF::applyRotation(const glm::vec3 x, const glm::quat rot){
    /*
    vec3 u = vec3(rot.x, rot.y, rot.z);
    return u * (glm::dot(u,x) *2) + x * (2*rot.w*rot.w-1) + glm::cross(u,x) * (2*rot.w) ;
    */
    float rdotx2 = 2 * (rot.x * x.x + rot.y * x.y + rot.z * x.z) ;
    float w2 = 2*rot.w ;
    float w2wm1 = 2*rot.w*rot.w-1.0f ;
    //vec3 cross = glm::cross(u,x) ;
    vec3 cross  = {rot.y * x.z - rot.z * x.y, rot.z * x.x - rot.x * x.z, rot.x*x.y - rot.y*x.x};
    float ox = rot.x * rdotx2 + x.x * w2wm1 + cross.x * w2;
    float oy = rot.y * rdotx2 + x.y * w2wm1 + cross.y * w2;
    float oz = rot.z * rdotx2 + x.z * w2wm1 + cross.z * w2;

    return vec3(ox,oy,oz);

}


// Create an IK pin to pull on the given bone local point
void GLTF::createPin(const std::string& name, int bone, glm::vec3 local_point, float weight){
    vec3 target = nodes[bone].transform * ( nodes[bone].bone_to_mesh * vec4(local_point,1)); // start by pinning in place
    //printf("target: %f, %f, %f\n", target.x, target.y, target.z);
    pins[name] = {name, bone, local_point, target, weight, glm::quat(0,0,0,1), 0};
}

// Set the target for a given pin
void GLTF::setPinTarget(const std::string& name, glm::vec3 target){
    pins[name].target = target ;
}

// Set the target for a given pin if it isn't close than target to its current location (prevents jitter)
void GLTF::setPinTarget(const std::string& name, glm::vec3 target, float tolerance) {
    if (glm::length(target - pins[name].target) > tolerance) {
        pins[name].target = target;
    }
}

// Create an IK pin to pull on the given bone local point and target ortation
// Returns starting orientation
glm::quat GLTF::createPin(const std::string& name, int bone, glm::vec3 local_point, float weight, float rot_weight){
    vec3 target = nodes[bone].transform * ( nodes[bone].bone_to_mesh * vec4(local_point,1)); // start by pinning in place

    glm::quat rot_target = nodes[bone].rotation ;
    int node_id = bone;
    node_id = nodes[node_id].parent;
    while(node_id != -1){
        rot_target = nodes[node_id].rotation * rot_target;
        node_id = nodes[node_id].parent;
    }
    rot_target = glm::quat_cast(transform) * rot_target ; 
    rot_target = glm::normalize(rot_target);

    //printf("target: %f, %f, %f\n", target.x, target.y, target.z);
    pins[name] = {name, bone, local_point, target, weight, rot_target, rot_weight};
    return rot_target ;
}

// Set the target for a given pin
void GLTF::setPinTarget(const std::string& name, glm::vec3 target, glm::quat rot_target){
    pins[name].target = target ;
    pins[name].rot_target = rot_target ;
}

void GLTF::setPinTarget(const std::string& name, glm::quat rot_target){
    pins[name].rot_target = rot_target ;
}


// delete pin
void GLTF::deletePin(const std::string& name){
    if(pins.find(name) != pins.end()){
        pins.erase(name);
    }
}

// run inverse kinematics on model to bones to attemp to satisfy pin constraints
void GLTF::applyPins(){ 
    vector<float> x0 = getX() ;
    vector<float>xf = OptimizationProblem::minimizeByLBFGS(x0, lbfgs_m, iter, step_iter, tolerance, 0.0000f); // L-BFGS converges fast
    setX(xf);
    for(int node_id=0; node_id<nodes.size(); node_id++){
        Node& bone = nodes[node_id];
        bone.rotation = glm::normalize(bone.rotation);
    }
    computeNodeMatrices();
}

// Return the current x for this object
std::vector<float> GLTF::getX(){
    vector<float> x ;
    x.reserve(nodes.size()*4);
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        x.push_back(node.rotation.w * node.stiffness);
        x.push_back(node.rotation.x * node.stiffness);
        x.push_back(node.rotation.y * node.stiffness);
        x.push_back(node.rotation.z * node.stiffness);
    }
    return x ;
}

// Set this object to a given x
void GLTF::setX(std::vector<float> x){
    int j = 0 ;
    for(int node_id=0; node_id<nodes.size(); node_id++){   
        Node& node = nodes[node_id];
        node.rotation.w = x[j]/node.stiffness;
        j++;
        node.rotation.x = x[j]/node.stiffness;
        j++;
        node.rotation.y = x[j]/node.stiffness;
        j++;
        node.rotation.z = x[j]/node.stiffness;
        j++;
    }
    //computeNodeMatrices();
}

// Returns the error to be minimized for the given input
float GLTF::error(std::vector<float> x){
    //vector<float> restore = getX();
    setX(x);
    float error = 0 ;
    for(const auto& [name, pin] : pins){

        //vec3 actual_matrix = nodes[pin.bone].transform * ( nodes[pin.bone].bone_to_mesh * dvec4(pin.local_point,1));

        vec3 actual = pin.local_point ;
        int node_id = pin.bone;
        glm::quat current_rot(0,0,0,0) ;
        current_rot.w = 1 ;
        while(node_id != -1){
            Node& bone = nodes[node_id];
            actual.x *= bone.scale.x;
            actual.y *= bone.scale.y;
            actual.z *= bone.scale.z;
            actual = GLTF::applyRotation(actual, bone.rotation);
            actual += bone.translation;
            current_rot = nodes[node_id].rotation * current_rot;
            node_id = bone.parent;

            
        }
        actual = transform * vec4(actual,1.0) ; // overall model pose transform

        //printf("error actual: %f, %f, %f\n", actual.x, actual.y, actual.z);
        vec3 diff = (actual - pin.target) ;
        error += pin.weight * glm::dot(diff, diff);
        

        glm::quat rt = pin.rot_target ;
        if(glm::dot(current_rot, rt) < 0){
            rt*=-1.0f;
        }
        glm::quat rot_diff = (current_rot-rt) ;
        error += pin.rot_weight * glm::dot(rot_diff,rot_diff)  ;
        
    }

    // enforce normalized quaternions with a barrier penalty
    for(int node_id=0; node_id<nodes.size(); node_id++){   
            Node& bone = nodes[node_id];
            float d2 = glm::dot(bone.rotation, bone.rotation);
            error += barrier_strength*(1-d2)*(1-d2);

            error += stiffness_strength * bone.stiffness * glm::dot(bone.base_rotation-bone.rotation, bone.base_rotation-bone.rotation) ;
    }
    
    return error ;
}

// Computes the gradient of a rotation's quaternion with respect to an error given gradient of x output to that error
glm::vec4 GLTF::dedq(const glm::vec3 x, const glm::quat rot, const glm::vec3 dedx){
    float dxdrx = 4 * rot.x * x.x + 2 * ( rot.y * x.y + rot.z * x.z ) ;
    float dxdry = 2 * (rot.x * x.y + x.z * rot.w) ;
    float dxdrz = 2 * ( rot.x * x.z - x.y * rot.w) ;
    float dxdrw = 4 *  rot.w * x.x + 2 * (rot.y * x.z - rot.z * x.y) ;

    float dydrx = 2 * (rot.y * x.x - x.z *rot.w) ;
    float dydry = 4 * rot.y * x.y + 2*( rot.x * x.x + rot.z * x.z);
    float dydrz = 2 * (rot.y * x.z + x.x * rot.w);
    float dydrw = 4 *  rot.w * x.y + 2 * (rot.z * x.x - rot.x * x.z) ;

    float dzdrx = 2 * (rot.z * x.x + x.y  * rot.w) ;
    float dzdry = 2 * (rot.z * x.y - x.x * rot.w) ;
    float dzdrz = 4 * rot.z * x.z + 2 * ( rot.x * x.x + rot.y * x.y);
    float dzdrw = 4 *  rot.w * x.z + 2 * (rot.x*x.y - rot.y*x.x) ;

    vec4 dedq ;
    dedq.x = dxdrx * dedx.x + dydrx * dedx.y + dzdrx * dedx.z ;
    dedq.y = dxdry * dedx.x + dydry * dedx.y + dzdry * dedx.z ;
    dedq.z = dxdrz * dedx.x + dydrz * dedx.y + dzdrz * dedx.z ;
    dedq.w = dxdrw * dedx.x + dydrw * dedx.y + dzdrw * dedx.z ;

    return dedq;
}

// Computes the gradient of a rotation's x input with respect to an error given gradient of x output to that error
glm::vec3 GLTF::dedx(const glm::vec3 x, const glm::quat rot, const glm::vec3 dedx){
    float dxdx = 2 * (rot.x*rot.x + rot.w*rot.w) - 1.0f ;
    float dxdy = 2 * (rot.x*rot.y - rot.z*rot.w) ;
    float dxdz = 2 * (rot.x*rot.z + rot.y*rot.w) ;

    float dydx = 2 * (rot.y*rot.x + rot.z*rot.w) ;
    float dydy = 2 * (rot.y*rot.y + rot.w*rot.w) - 1.0f ;
    float dydz = 2 * (rot.y*rot.z - rot.x*rot.w) ;

    float dzdx = 2 * (rot.z*rot.x - rot.y*rot.w) ;
    float dzdy = 2 * (rot.z*rot.y + rot.x*rot.w) ;
    float dzdz = 2 * (rot.z*rot.z + rot.w*rot.w) - 1.0f ;

    vec3 dedxin ;
    dedxin.x = dxdx * dedx.x + dydx * dedx.y + dzdx * dedx.z ;
    dedxin.y = dxdy * dedx.x + dydy * dedx.y + dzdy * dedx.z ;
    dedxin.z = dxdz * dedx.x + dydz * dedx.y + dzdz * dedx.z ;

    return dedxin;
}

// Returns the gradient of error about a given input
std::vector<float> GLTF::gradient(std::vector<float> x){
    
    std::vector<float> gradient ;
    gradient.resize(x.size());
    for(int k=0;k<x.size();k++){
        gradient[k] = 0.0 ;
    }

    setX(x);
    
    for(const auto& [name, pin] : pins){
        double error = 0 ;
        double rot_error = 0 ;
        // Forward propogate error
        vec3 actual = pin.local_point ;
        int node_id = pin.bone;
        vector<int> bones ;
        vector<vec3> xi;
        glm::quat current_rot(0,0,0,0) ;
        current_rot.w = 1 ;
        vector<glm::quat> r;
        r.push_back(current_rot);
        while(node_id != -1){
            bones.push_back(node_id);
            Node& bone = nodes[node_id];
            actual.x *= bone.scale.x;
            actual.y *= bone.scale.y;
            actual.z *= bone.scale.z;
            xi.push_back(actual);
            actual = GLTF::applyRotation(actual, bone.rotation);
            actual += bone.translation;
            current_rot = nodes[node_id].rotation * current_rot;
            r.push_back(current_rot);
            node_id = bone.parent;
        }
        actual = transform * vec4(actual,1.0) ; // overall model pose transform

        //printf("point: %f, %f, %f\n", actual.x, actual.y, actual.z);
        dvec3 diff = (dvec3(actual) - dvec3(pin.target)) ;
        error = pin.weight * glm::dot(diff, diff);
        vec3 dedx =  transform * dvec4(diff,0.0) * (2.0f * pin.weight) ;
        
        /*
        rot_error = pin.rot_weight * glm::dot(rot_diff,rot_diff)  ;
        glm::quat bone_rot_gradient = rot_diff *(pin.rot_weight * 2.0f)  ;
        gradient[pin.bone*4] += bone_rot_gradient.w/nodes[pin.bone].stiffness ; 
        gradient[pin.bone*4+1] += bone_rot_gradient.x/nodes[pin.bone].stiffness;
        gradient[pin.bone*4+2] += bone_rot_gradient.y/nodes[pin.bone].stiffness;
        gradient[pin.bone*4+3] += bone_rot_gradient.z/nodes[pin.bone].stiffness;
        */
        glm::quat rt = pin.rot_target ;
        if(glm::dot(current_rot, rt) < 0){
            rt*=-1.0f;
        }
        glm::quat rot_diff = (current_rot-rt) ;
        glm::quat dfdr = rot_diff *(pin.rot_weight * 2.0f) ;

        // back propogate gradient
        for(int bi = (int)bones.size()-1; bi>= 0; bi--){
            Node& bone = nodes[bones[bi]];
            // gradient of rotation of this bone
            vec4 dedq = GLTF::dedq(xi[bi], bone.rotation, dedx) ;

            gradient[bones[bi]*4] += dedq.w/bone.stiffness ; 
            gradient[bones[bi]*4+1] += dedq.x/bone.stiffness;
            gradient[bones[bi]*4+2] += dedq.y/bone.stiffness;
            gradient[bones[bi]*4+3] += dedq.z/bone.stiffness;
            
            // prepare dedex for next bone
            dedx = GLTF::dedx(xi[bi], bone.rotation, dedx) ;
            dedx.x *= bone.scale.x;
            dedx.y *= bone.scale.y;
            dedx.z *= bone.scale.z;

            // add gradient for rotation pin
            glm::quat dfdq = dfdr * glm::inverse(r[bi])   ;
            dfdr =  glm::inverse(bone.rotation) * dfdr  ;

            gradient[bones[bi]*4] += dfdq.w/bone.stiffness ; 
            gradient[bones[bi]*4+1] += dfdq.x/bone.stiffness;
            gradient[bones[bi]*4+2] += dfdq.y/bone.stiffness;
            gradient[bones[bi]*4+3] += dfdq.z/bone.stiffness;
        }
    }

    // enforce normalized quaternions with a barrier penalty
    for(int node_id=0; node_id<nodes.size(); node_id++){
            Node& bone = nodes[node_id];
            float d2 = glm::dot(bone.rotation, bone.rotation); // x is bone.rotation/stiffness so that works into the gradient
            float dqdb = 4 * (d2-1) * barrier_strength/bone.stiffness;
            gradient[node_id*4] += bone.rotation.w * dqdb ;
            gradient[node_id*4+1] += bone.rotation.x * dqdb ;
            gradient[node_id*4+2] += bone.rotation.y * dqdb ;
            gradient[node_id*4+3] += bone.rotation.z * dqdb ;
            

            // enforce node stiffness (stiffness multiplier cancels with conversion from bone to coordinates)
            gradient[node_id*4] += 2.0f*stiffness_strength*(bone.rotation.w-bone.base_rotation.w);
            gradient[node_id*4+1] += 2.0f*stiffness_strength*(bone.rotation.x-bone.base_rotation.x);
            gradient[node_id*4+2] += 2.0f*stiffness_strength*(bone.rotation.y-bone.base_rotation.y);
            gradient[node_id*4+3] += 2.0f*stiffness_strength*(bone.rotation.z-bone.base_rotation.z);

    }
    return gradient ;
}

void GLTF::fixedSpeedIK(float speed){
    vector<float> x0 = getX() ;
    vector<float> g = gradient(x0);

    for(int node_id=0; node_id<nodes.size(); node_id++){   
            Node& bone = nodes[node_id];
            // gradient of rotation of this bone
            //vec4 dedq = GLTF::dedq(xi[bi], bone.rotation, dedx) ;

            vec4 gb = vec4(g[node_id*4], g[node_id*4+1], g[node_id*4+2],g[node_id*4+3]);
            float m = glm::length(gb);
            if (m > 0.001 && bone.stiffness <= 1) {
                gb = glm::normalize(gb);
                gb *= speed; /*/bone.stiffness ;*/

                bone.rotation.w -= gb[0];
                bone.rotation.x -= gb[1];
                bone.rotation.y -= gb[2];
                bone.rotation.z -= gb[3];
                bone.rotation = glm::normalize(bone.rotation);
            }
        }
}


void GLTF::fixedSpeedRotationIK(float speed){
    for(const auto& [name, pin] : pins){
        int node_id = pin.bone;
        glm::quat current = nodes[node_id].rotation ;
        node_id = nodes[node_id].parent;
        while(node_id != -1){
            current = nodes[node_id].rotation * current;
            node_id = nodes[node_id].parent;
        }
        current = glm::quat_cast(transform) * current ; 
        current = glm::normalize(current); // scale in transform may leak into quat
        glm::quat local_rot_target = nodes[pin.bone].rotation * glm::inverse(current) * pin.rot_target  ;
        nodes[pin.bone].rotation = slerp(nodes[pin.bone].rotation, local_rot_target, speed);
        //printf("current: %f, %f, %f, %f\n", current.w, current.x, current.y, current.z);
        
        
        //printf("bone local: %f, %f, %f, %f\n", nodes[pin.bone].rotation.w, nodes[pin.bone].rotation.x, nodes[pin.bone].rotation.y, nodes[pin.bone].rotation.z);
        //printf("target local: %f, %f, %f, %f\n", local_target.w, local_target.x, local_target.y, local_target.z);
        

        /*
        node_id = pin.bone;
        glm::quat result = local_target;
        node_id = nodes[node_id].parent;
        while(node_id != -1){
            result = nodes[node_id].rotation * result;
            node_id = nodes[node_id].parent;
        }
        result = glm::quat_cast(transform) * result ;
        printf("pin.target: %f, %f, %f, %f\n", pin.target.w, pin.target.x, pin.target.y, pin.target.z);
        printf("result    : %f, %f, %f, %f\n", result.w, result.x, result.y, result.z);
        */
    }
}

glm::mat4 GLTF::getNodeTransform(std::string name){
    computeNodeMatrices();
    for(auto& n : nodes){
        //printf("%s\n",n.name.c_str());
        if(n.name == name){
            return n.transform;
        }
    }
    return mat4(0);
}

glm::vec3 GLTF::getFirstPersonPosition(){
    return nodes[first_person_bone].transform * ( nodes[first_person_bone].bone_to_mesh * vec4(first_person_offset,1)); 
}
//returns the ndoe index of the bone with the matching name or -1 if not found
int GLTF::getBoneIndex(std::string bone_name) {
    for (int k = 0; k < nodes.size(); k++) {
        if (nodes[k].name == bone_name) {
            return k;
        }
    }
    return -1;
}

// Adds the geometry from the given model to this model
void GLTF::addGeometry(std::shared_ptr<GLTF> new_geometry, const glm::mat4& transform) {
    std::unordered_map<int, int> new_vertex_id;
    for (int old_vertex_id = 0; old_vertex_id < new_geometry->vertices.size(); old_vertex_id++) {
        new_vertex_id[old_vertex_id] = (int)(vertices.size());
        Vertex copied = new_geometry->vertices[old_vertex_id];
        copied.position = transform * glm::vec4(copied.position, 1.0f);
        copied.normal = transform * glm::vec4(copied.normal, 0.0f);
        copied.normal = glm::normalize(copied.normal);
        //copied.transformed_position = transform * glm::vec4(copied.transformed_position, 1.0f);
        //copied.transformed_normal = transform * glm::vec4(copied.transformed_normal, 0.0f);
        //copied.transformed_normal = glm::normalize(copied.normal);
        vertices.push_back(copied);
    }
    for (Triangle& old_triangle : new_geometry->triangles) {
        Triangle copied = old_triangle;
        copied.A = new_vertex_id[copied.A];
        copied.B = new_vertex_id[copied.B];
        copied.C = new_vertex_id[copied.C];
        triangles.push_back(copied);
    }


}

// Merges duplicate vertices
void GLTF::dedupeVertices(float tolerance) {
    std::unordered_map<int64_t, int> hash_to_index; // hash to new index
    std::unordered_map<int, int> index_change; // old index to new index (for updating triangles rapidly)
    std::vector<Vertex> new_vertices;

    for (int old_vertex_id = 0; old_vertex_id < vertices.size(); old_vertex_id++) {
        Vertex& v = vertices[old_vertex_id];
        int64_t hash = hashVertex(v.position, tolerance) ^ hashVertex(v.normal, 0.01f);
        if (hash_to_index.find(hash) == hash_to_index.end()) {
            hash_to_index[hash] = (int)new_vertices.size();
            new_vertices.push_back(v);
        }
        index_change[old_vertex_id] = hash_to_index[hash];

    }
    //printf("Reduced vertices from %d to %d!\n", (int)vertices.size(), (int)new_vertices.size());
    vertices = new_vertices;
    for (Triangle& triangle : triangles) {
        triangle.A = index_change[triangle.A];
        triangle.B = index_change[triangle.B];
        triangle.C = index_change[triangle.C];
    }
    
}


//Creates a copy of this model with normals flipped and winding order inverted
//Such a model should appear correctly when a reflection transform is applied.
std::shared_ptr<GLTF> GLTF::createMirrorImage(){
	std::shared_ptr<GLTF> mirrored = std::make_shared<GLTF>(*this) ;
	for(Vertex& v : mirrored->vertices){
		v.normal*= -1.0f ;
		for(int k=0;k<v.morph_normal.size();k++){
			v.morph_normal[k] *= -1.0f ;
		}
	}

	for (Triangle& t : mirrored->triangles) {
		int A = t.A ;
		t.A = t.B ;
		t.B = A ;
	}
	return mirrored ;
}
