#ifndef _VULKAN_PLUGIN_H_
#define _VULKAN_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"
#include "FlagSet.h"
#include "CSVLog.h"
#include "Utilities.h" // Enables unordered_map keyed on pair

#include "SDL3/SDL.h"
#include "glew.h"
#include "glm/glm.hpp"
#include "VkBootstrap.h"


#include <vk_types.h>
#include <GL/glu.h>
#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_set>
#include <vector>


// Forward declare some things so they can cross reference each other
class VulkanPlugin;
class RenderTarget;

// This structs are just used to hold the GPU pointers that need to be destroyed when the image and buffer get deleted
struct ImageToDestroy{
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	int frame = 0;
	std::chrono::high_resolution_clock::time_point time;
};

struct BufferToDestroy{
	VkBuffer buffer;
	VmaAllocation allocation;
	int frame = 0 ;
	std::chrono::high_resolution_clock::time_point time ;
};

// Reference and info about an image that exists on the GPU for use in Vulkan
class VulkanImage {
	public:
		VkImage image = 0;
		VkImageView imageView = 0;
		VmaAllocation allocation = 0;
		VkExtent3D imageExtent ;
		VkFormat imageFormat ;
		VkImageUsageFlags usages = 0 ;
		VkImageLayout current_layout ;
		bool depth = false;

		static inline std::mutex buffer_lock;

		~VulkanImage();

		static inline std::vector<ImageToDestroy> vulkan_images_to_destroy = std::vector<ImageToDestroy>();
};

class VulkanBuffer {
	public:
		VkBuffer buffer;
		VmaAllocation allocation;
		VmaAllocationInfo info;
		VkDeviceAddress device_address; // can be used as apointer in shaders, will be set only if the buffer is flagged for shader use
		uint32_t object_count = 0;// will be set when pushing structs with pushBufferData

		static inline std::mutex buffer_lock ;

		~VulkanBuffer() ;

		static inline std::vector<BufferToDestroy> vulkan_buffers_to_destroy = std::vector<BufferToDestroy>();
};

//A Generic image that can be manipulated in any thread in the World Fabric engine
//Actual GPU commands will be defered and executed on the Vulkan thread
class WFImage {
public:
	
	WFImage(uint32_t  width, uint32_t  height, VkFormat format, VkImageUsageFlags usages);
	~WFImage();
	void setImage(void* data, uint32_t width, uint32_t height);
	void setSampler(const VkSamplerCreateInfo& samplerInfo) ;
	std::shared_ptr<VulkanImage> getVulkanImage(VulkanPlugin* r) ;
	VkSampler getSampler(VulkanPlugin* r);

	uint32_t getWidth();
	uint32_t getHeight();
	VkFormat getFormat();
	VkImageUsageFlags getUsages();

	bool requires_clearing = true ;

private:
	std::shared_ptr<VulkanImage> vulkan_image = nullptr; //will only be manipulated on the Vulkan thread
	uint32_t width;
	uint32_t height;

	bool needs_created = false;
	VkFormat format;
	VkImageUsageFlags usages;

	bool needs_data_push = false;
	Variant pending_data;

	bool needs_sampler = false;
	bool has_sampler = false;
	VkSampler texture_sampler;
	VkSamplerCreateInfo sampler_info{};

} ;


enum FragmentBlendMode
{
	OVERWRITE,
	ADDITIVE,
	ALPHA_BLEND,
	IGNORE
};


// A shader program that consists of a vertex and a fragment shader and outputs to a set of images
class TriangleShaderProgram{

	public:
		// Initialize a triangle rendering pipeline and make it avilable with the given configurations
		TriangleShaderProgram(VkDevice device,
			VkShaderModule vertex_shader,
			VkShaderModule fragment_shader,
			int push_constant_struct_size,
			int num_textures,
			VkCullModeFlagBits cull_mode,
			std::shared_ptr<RenderTarget> format_example,
			FragmentBlendMode blend_mode) ;

		// Allows creating a nonpointer variable to be intiialzied later, but don't try to use it
		TriangleShaderProgram() = default;

		// Bind this program for calls
		void bindProgram(VkCommandBuffer cmd);

		// Bind textures to this pipeline, requires the program be bound
		void bindTextures(VkCommandBuffer cmd, VkDescriptorSet texture_bindings);

		// Set the active push constants for this program, requires the program be bound
		template<typename T>
		void inline setPushConstants(VkCommandBuffer cmd, const T& push_constants){
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &push_constants);
		}
	private:
		VkPipelineLayout layout;
		VkPipeline pipeline;
		VkDevice device;
		int num_color_attachments ; // triangle shaders don't have to attach all rrender target images
};

// A compute shader program which runs on a set of images
class ScreenShaderProgram{
public:
	// Initialize a rendering pipeline and make it avilable with the given configurations
	// layout will be inferred from a set of starter images, but different matching layout images can be bound later
	ScreenShaderProgram(VkDevice device, VkShaderModule compute_shader, int push_constant_struct_size, const std::vector< std::shared_ptr<WFImage>>& images, int local_block_size);

	// Allows creating a nonpointer variable to be intiialzied later, but don't try to use it
	ScreenShaderProgram() = default;

	// Bind this program for calls
	void bindProgram(VkCommandBuffer cmd);

	// Set the images that will be used in render, 
	// binding happens automatically this is for changing what will be bound
	void setImages(VkDescriptorSet image_bindings, int w, int h);


	// Set the active push constants for this program, requires the program be bound
	template<typename T>
	void inline setPushConstants(VkCommandBuffer cmd, const T& push_constants) {
		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(T), &push_constants);
	}

	// add the run of this shader effect to the ccommand buffer
	void render(VkCommandBuffer cmd);

	// Need to call this if the rendering surface changes size
	void updateImageSize(int w, int h);

	
	VkDescriptorSet image_descriptors;
private:
	VkPipelineLayout layout;
	VkPipeline pipeline;
	VkDevice device;
	VkDescriptorSetLayout descriptor_layout;
	int local_size ; 
	int image_width ;
	int image_height ;
};


template<typename Instance>
void setPose(Instance* instance, const glm::mat4& root_pose, const std::vector<glm::mat4>& bones) {
	throw std::runtime_error("Attempting to set pose on an instance that did not override setPose!");
}



class Renderable {

public:
	int phase = 0; // Renderables are rendered in phase order
	int group = 0; // renderables in the same phase and group will be rendered with nothing in between
	bool debug_print = false ;
	bool hidden = false;
	std::recursive_mutex lock; // used to prevent modifying while datais being pushed to the GPU
	int input_num = -1 ;
	// Begin group is called once on the first entity within a group before the first render is called
	// Good for binding a program or transitioning images to the correct format if everything in the group uses the same resources
	virtual void beginGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) = 0;

	virtual void endGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) = 0;

	//render is called for each object to render it
	// you can assume that beinGroup will have been called for at leats one entity in a group
	virtual void render(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) = 0;

	// Make sure all textues are in the correct texture layout in case they were rendered to
	virtual void requireTextureLayouts(VkCommandBuffer cmd, VulkanPlugin* renderer) = 0 ;

	//Push any pending buffer changes, like model, instance, or textures
	virtual void updateBuffers(VkCommandBuffer cmd, VulkanPlugin* renderer) = 0;

	// Allows setting instances on a TriangleModel through a Renderable withut knowing the exact instant type
	// instances will need to actually match the model
	virtual void setInstances(const std::vector<std::shared_ptr<void>>& instances) = 0;

	// Returns an apprpriately typed instance whose pose can beset
	virtual std::shared_ptr<void> createInstance() = 0;

	virtual void setInstancePose(void* instance, const glm::mat4& root_pose, const std::vector<glm::mat4>& bones) = 0 ;
	
	void setInputNum(int scene_input_num) {
		input_num = scene_input_num;
	}

	void setTargets(std::unordered_set<std::shared_ptr<RenderTarget>> new_targets){
		lock.lock();
		targets = new_targets ;
		lock.unlock();
	}

	void clearTargets(){
		lock.lock();
		targets.clear();
		lock.unlock();
	}

	bool hasTarget(const std::shared_ptr<RenderTarget>& target){
		lock.lock();
		bool has = targets.find(target) != targets.end();
		lock.unlock();
		return has ;
	}

	private:
		std::unordered_set<std::shared_ptr<RenderTarget>> targets; // surfaces this renderable should be rendered to

};


class VulkanPlugin : public AsyncPlugin {

public:

	static inline std::string tag = "VulkanLink";

	static inline constexpr bool USE_VALIDATION_LAYERS = true;
	static inline constexpr unsigned int CHAIN_FRAMES = 2;
	static inline int millis_to_hold_buffer = 50; // buffers get a few milliseconds before being destroyed after going out of scope to give pending off thread GPU actions time to complete
	static inline int frames_to_hold_buffer = 3 ; // In case frame rate hitches, like when loading large models, also make sure buffers hang around for frame completion
	static inline int frame_number = 0 ; // number of frames displayed so far

	static inline std::vector<std::pair<VkSampler, std::chrono::high_resolution_clock::time_point>> samplers_to_destroy; // This is stored in the vulkan plugin to prevent the global from being duplicated for different templated models

	static inline std::vector<std::pair<VkDescriptorSet, std::chrono::high_resolution_clock::time_point>> descriptors_to_destroy;

	//static inline std::unordered_map<size_t, std::shared_ptr <VulkanBuffer>> staging_buffers ; // maps size to staging buffers so they can be saved and reused

	// SDL bookkeeping
	SDL_Window* window = nullptr;
	std::string title;
	int window_width = 1920;
	int window_height = 1080;
	bool vsync_enabled = true ;
	int target_frame_micros = 1000000 / 120; //if vsync off, attempts to hit this amount of time on each frame
	int sleep_micros = 0;
	std::chrono::high_resolution_clock::time_point last_sync_time = now();
	
	int next_renderable_id = 1 ;

	//handles for Vulkan
	VkDevice device;
	VkInstance vulkan_instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkPhysicalDevice physical_device;
	VmaAllocator VMA_allocator;

	VkQueue vulkan_queue;
	uint32_t vulkan_queue_family;

	//TODO make private?
	std::vector< std::shared_ptr<WFImage>> extra_images_to_clear ; // images besides render targets to be cleared at the begining of each frame
	std::vector <VkClearColorValue> extra_clear_values ;
	std::vector< std::shared_ptr<WFImage>> extra_depth_to_clear; // depth images besides render targets to be cleared at the beginning of each frame
	
	std::shared_ptr<RenderTarget> window_target;

	static inline int last_sdl_input_num = -1 ; // version user inputs so input latency can be measured

	// Creates a window and connects to controllers and other hardware
	VulkanPlugin(const std::string& title, bool vsync, bool fullscreen);

	// Called on every plug-in before any plug-ins are run
	// Adds an XRStatus object with the tag "xr_status" contain data other plugins can use to interact with the headset
	void initialize() override;

	void run() override;

	// returns the key code of the last key pressed
	int getLastKeyPress();

	// Returns whether a key is currently down
	bool keyDown(int key_code);

	// Returns whether a key is currently down
	bool mouseDown(int button);

	// Returns the currnet mouse position in the window
	glm::vec2 getMousePosition();

	// Returns the total net mouse wheel movement within the window
	glm::vec2 getMouseWheelPosition();

	// Returns the direction of the mouse ray from the window target position
	glm::vec3 getMouseRay();

	//Hide the mouse in the window
	void hideMouse() ;
	
	//Unhide the mouse in the window
	void showMouse();

	float getGamepadAxis(Uint32 gamepad_id, Uint32 axis_id);

	bool getGamepadButton(Uint32 gamepad_id, Uint32 button_id);

	// returns the gamepaid id and button id of the last button pressed
	std::pair<Uint32, Uint32> getLastGamepadPress();

	// Sets the current typing and string to begin typing
	void setTyped(int cursor, std::string text);

	//returns the current typing cursor and string
	std::pair<int, std::string> getTyped();

	//process a typed key code for its affect on the tpyed string or cursor position
	void processTyping(SDL_Keycode key_code);

	//process a typed characer for its affect on the tpyed string or cursor position
	void processTyping(std::string key);

	void setVSync(bool new_vsync);

	std::shared_ptr<VulkanImage>  createVulkanImage(uint32_t  width, uint32_t  height, VkFormat format, VkImageUsageFlags usages);

	void destroyVulkanImage(ImageToDestroy& image);

	std::shared_ptr<VulkanBuffer> createVulkanBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(BufferToDestroy& buffer);

	//returns memory type to use when the given flasg are required
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	//change active layout for a Vulkan image
	//used primarily to switch an image onthe GPU between reading and writing mode
	void transitionVulkanImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth);

	void requireLayout(VkCommandBuffer cmd, std::shared_ptr<VulkanImage> image, VkImageLayout layout) ;

	//Copies one vulkan into another, may also rescale
	//USe transitionVulkanImage to the get the images into the write modes to allow this
	void copyVulkanImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

	//pushes raw data in an array to an image on the GPU
	void pushImageData(std::shared_ptr<VulkanImage> image, void* data, uint32_t width, uint32_t height);

	// loads an image form a file directly into a vulkan buffer
	std::shared_ptr<WFImage> loadImageFromFile(const std::string& path);

	VkShaderModule loadShader(const unsigned char* shader_file_contents, uint32_t num_bytes);

	//allocate and set a texture or image binding for a shader
	VkDescriptorSet createImageBinding(const std::vector<std::shared_ptr<WFImage>>& images,const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags,const VkImageLayout& image_layout);

	//Returns layout for a set of images ot bind to a shader that can be used to allocate a binding
	VkDescriptorSetLayout getDescriptorLayout(const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags, int image_count);

	//allocate a binding from a descriptor pool
	//Note: you need to cal destroy binding when done to not leak this GPU memory
	VkDescriptorSet allocateBinding(const VkDescriptorSetLayout& layout, const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags, int image_count);

	//Cleans up a descriptor set created with allocateBinding
	//This will put things to be deleted onto a queue so pending instructions can still use them
	void destroyBinding(const VkDescriptorSet& binding) ;	


	//Cleans up a descriptor set created with allocateBinding
	void actuallyDestroyBinding(const VkDescriptorSet& binding);

	void clear(VkCommandBuffer cmd, std::vector<std::shared_ptr<WFImage>> output_attachments, std::vector <VkClearColorValue> output_clear, std::vector<std::shared_ptr <WFImage>> depth_attachments);

	void clear(VkCommandBuffer cmd, std::shared_ptr<VulkanBuffer> buffer) ;

	void setWindowTarget(std::shared_ptr<RenderTarget> t){
		window_target = t ;
		addRenderTarget(t);
	}

	void addRenderTarget(std::shared_ptr<RenderTarget>& t){
		lock.lock();
		active_targets.insert(t);
		lock.unlock();
	}

	void removeRenderTarget(std::shared_ptr<RenderTarget>& t) {
		if (getTool<FlagSet>() && getTool<FlagSet>()->getInt(AsyncPlugin::SHUTDOWN_FLAG) == 0) {
			lock.lock();
			active_targets.erase(t);
			lock.unlock();
		}
	}

	int addRenderable(std::shared_ptr<Renderable> r){
		lock.lock();
		int id = next_renderable_id ;
		next_renderable_id++;
		renderables[id] = r ;
		lock.unlock();
		return id ;
	}

	void removeRenderable(int id){
		if(getTool<FlagSet>() && getTool<FlagSet>()->getInt(AsyncPlugin::SHUTDOWN_FLAG) == 0){
			lock.lock();
			renderables.erase(id);
			lock.unlock();
		}
	}

	std::shared_ptr<Renderable> getRenderable(int id){
		auto f = renderables.find(id);
		if(f != renderables.end()){
			return f->second ;
		}
		return std::shared_ptr<Renderable>();
	}
	
	// Combines fetching a renderable by id and setting an instance on it into one call without checks to avoid creating new data
	//This is potentially called ALOT
	void inline setInstancePose(int renderable_id, void* instance, const glm::mat4& root_pose, const std::vector<glm::mat4>& bones){
		lock.lock();
		renderables[renderable_id]->setInstancePose(instance, root_pose, bones);
		lock.unlock();
	}

	void inline setInputNum(int renderable_id, int input_num){
		lock.lock();
		renderables[renderable_id]->setInputNum(input_num);
		lock.unlock();
	}

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);


	template<typename T>
	inline void pushBufferData(VkCommandBuffer& cmd, const std::vector<T>& input_data, std::shared_ptr<VulkanBuffer> buffer) {
		lock.lock();
		const size_t buffer_size = input_data.size() * sizeof(T);
		if(buffer_size == 0){
			lock.unlock();
			return ;
		}
		std::shared_ptr <VulkanBuffer> staging = createVulkanBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
		void* staging_data;
		vmaMapMemory(VMA_allocator, staging->allocation, &staging_data);

		memcpy(staging_data, input_data.data(), buffer_size);// copy vertex buffer into staging

		// move from staging to given buffer
		VkBufferCopy data_copy{ 0 };
		data_copy.dstOffset = 0;
		data_copy.srcOffset = 0;
		data_copy.size = buffer_size;
		vkCmdCopyBuffer(cmd, staging->buffer, buffer->buffer, 1, &data_copy); // staging goes out of scope here, but buffers always hang around for millis_to_hold_buffer so this is fine
		buffer->object_count = (uint32_t)input_data.size();
		lock.unlock();
	}

	template<typename T>
	inline void pushBufferData(const std::vector<T>& input_data, std::shared_ptr<VulkanBuffer> buffer) {
		//create Vulkan command to submit automatically (This is slow  you should pass a command buffer if you have one)
		immediateSubmit([&](VkCommandBuffer cmd) {
			pushBufferData(cmd, input_data, buffer) ;
		});
	}

	static inline bool timing_enabled = false;
	static inline int log_print_interval_seconds = 10 ;
	static inline std::shared_ptr<CSVLog> timing_log;
	std::vector<std::string> timestamp_names;
	std::map<std::string,int> calls_in  ;
	static inline auto last_log_time = now();

	std::map<std::string, double> time_totals ;
	int amount_timed = 0 ;

	//Enables detailed timings to be saved to a file
	static void enableRenderTiming(const std::string& file);
	void resetTimes(VkCommandBuffer cmd);
	void stampTime(VkCommandBuffer cmd, const std::string& name);
	void logTimes();

private:

	std::map<SDL_Keycode, bool> button_values;
	std::unordered_map<int, bool> key_state; // which keys are currently down
	std::unordered_map<Uint32, SDL_Gamepad*> connected_gamepads; // ids of all connected game pads
	std::unordered_map<std::pair<Uint32, Uint32>, float> gamepad_axis ; // index is <gamepad_id, axis id>
	std::unordered_map<std::pair<Uint32, Uint32>, bool> gamepad_button; // index is <gamepad_id, axis id>
	std::pair<int, int> last_gamepad_button = {-1,-1} ;

	int last_key = -1;
	glm::vec2 mouse_position ;
	glm::vec2 mouse_down_position;
	glm::vec2 mouse_wheel_position;
	std::map<int, bool> mouse_down ;
	bool mouse_hidden = false;
	bool last_mouse_hidden = false; 
	int typing_cursor = 0;
	std::string typed_text ;
	
	VkSurfaceKHR SDL_vulkan_surface;
	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	vkb::Swapchain vkbSwapchain ;
	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkDescriptorSet draw_image_descriptors;
	VkDescriptorSetLayout draw_image_descriptor_layout;

	vkb::Swapchain swap_chain ;

	static const int MAX_DESCRIPTORS_PER_POOL = 1 ;
	struct PoolWithInfo{
		VkDescriptorPool pool ;
		int image_count= -1 ; // number of images in binding also pool_sizes size
		int used_descriptors = 0 ;
		VkDescriptorType type;
		VkShaderStageFlags stage_flags ;
	};
	std::unordered_map<std::pair<VkDescriptorType,VkShaderStageFlags>,std::unordered_map<int, std::vector<std::shared_ptr<PoolWithInfo>>>> descriptor_pools ;//First index is number of images, so each pool only contains descriptors of the same size
	std::unordered_map < VkDescriptorSet, std::pair<std::shared_ptr<PoolWithInfo>, VkDescriptorSetLayout>> descriptor_location; // remember which pools we allocate to and our layout for easy cleanup

	
	std::unordered_set< std::shared_ptr<RenderTarget>> active_targets;

	// immediate submit structures
	VkFence main_fence ;
	VkCommandBuffer command_buffer ;
	VkCommandPool command_pool ;

	int max_time_stamps = 40 ;
	VkQueryPoolCreateInfo queryPoolInfo{};
	VkQueryPool timestamp_query_pool;


	struct FrameData {
		VkSemaphore swapchain_semaphore, render_semaphore;
		VkFence acquire_fence ;
		VkFence render_fence;
		VkCommandPool command_pool;
		VkCommandBuffer main_command_buffer;

	};

	FrameData frames[CHAIN_FRAMES]; // frames of th swap chain for buffering

	bool resize_requested = false ;
	bool minimized = false;


	std::unordered_map<int, std::shared_ptr<Renderable>> renderables;

	void initVulkan();

	void createSwapchain(uint32_t width, uint32_t height, VkFormat swap_chain_image_format, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR SDL_vulkan_surface);

	void destroySwapchain(vkb::Swapchain swapchain, VkDevice device);


	void processInput();
	void draw();
	void drawRenderables(VkCommandBuffer cmd);
	
};

//Define templated classes last so they can reference the renderer

class RenderTarget {
public:
	glm::mat4 camera_matrix;
	glm::vec3 camera_position ;
	int width = 0; // dimensions of images
	int height = 0;
	float near = -1;
	float far = -1 ;
	std::vector<std::shared_ptr<WFImage>> images; // Images for binding to shaders (could be several for defferred rendering)
	std::vector<VkClearColorValue> clear_values;
	std::shared_ptr<WFImage> depth; // depth image to be cleared at the beginning of each frame andused for depth binding in shaders
	std::shared_ptr<WFImage> final_image; // The image that will ultimately be put on screen (should be in images as this is not passed to shaders)
	bool screen_resize = false;
	bool needs_clear = false;
	bool is_rendering = false ;

	int num_fragments = 0;// render tagrets may optionally have extended fragments to support order independent transparency
	int frag_size = 0 ;
	std::shared_ptr<VulkanBuffer> fragments ;
	std::shared_ptr<VulkanBuffer> counts;

	static inline float default_near_plane = 0.1f;
	static inline float default_far_plane = 1000.0f;

	void setImages(const std::vector<std::shared_ptr<WFImage>>& i, const std::vector<VkClearColorValue>& c, const std::shared_ptr<WFImage> d, const std::shared_ptr<WFImage> f) {

		images = i;
		clear_values = c;
		depth = d;
		final_image = f;
		width = f->getWidth();
		height = f->getHeight();
	}

	void setCamera(const glm::vec3& position, const glm::mat4& matrix, float near, float far){
		camera_position = position ;
		camera_matrix = matrix ;
	}

	void setCamera(const glm::vec3& position, const glm::vec3& look_at, float fov, const glm::vec3& up){
		camera_position = position;
		glm::mat4 flip = glm::scale(glm::mat4(1.0f), glm::vec3(1, -1, 1));
		glm::mat4 look_at_matrix = glm::lookAtRH(camera_position, look_at, up);
		near = default_near_plane ;
		far = default_far_plane ;
		glm::mat4 projection = glm::perspectiveRH_ZO(fov, width / (float)height, near, far);
		camera_matrix = flip* projection * look_at_matrix ;
	}

	void setCamera(const glm::vec3& position, const glm::vec3& look_at, float fov, const glm::vec3& up, float far_plane) {
		camera_position = position;
		glm::mat4 flip = glm::scale(glm::mat4(1.0f), glm::vec3(1, -1, 1));
		glm::mat4 look_at_matrix = glm::lookAtRH(camera_position, look_at, up);
		near = default_near_plane;
		far = far_plane;
		glm::mat4 projection = glm::perspectiveRH_ZO(fov, width / (float)height, near, far);
		camera_matrix = flip * projection * look_at_matrix;
	}

	// Sets the camera so 2D values will map to texture coordnates
	void setOrthoCamera() {
		camera_position = glm::vec3(0,0,0);
		near = default_near_plane;
		far = default_far_plane;
		camera_matrix = glm::orthoLH_ZO(0.0f, (float)width, (float)height, 0.0f, near, far);
	}

	void clear(VkCommandBuffer cmd, VulkanPlugin* renderer) {
		needs_clear = true;
		if(num_fragments > 0){
			renderer->clear(cmd, fragments) ;
			renderer->clear(cmd, counts);
		}
	}

	void setViewport(VkCommandBuffer cmd, VulkanPlugin* renderer){
		//set dynamic viewport and scissor for render area
		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)(final_image->getWidth());
		viewport.height = (float)(final_image->getHeight());
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = (uint32_t)(viewport.width);
		scissor.extent.height = (uint32_t)(viewport.height);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
	}


	void createExtendedFragments(int max_fragments, int fragment_size, VulkanPlugin* renderer){
		num_fragments = max_fragments;
		frag_size = fragment_size ;// stash the fragment infoso it can be used on resize
		fragments = renderer->createVulkanBuffer(width*height*num_fragments*fragment_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY) ;
		counts = renderer->createVulkanBuffer(width * height * sizeof(int), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	}

	void enableScreenResize(bool enable){
		screen_resize = enable ;
	}

	void resize(uint32_t new_width, uint32_t new_height, VulkanPlugin* renderer){
		/* TODO resizing doesn't work, no idea why
		//printf("resizing render target images!\n");
		width = new_width;
		height = new_height ;
		
		for(int k=0;k<images.size();k++){
			bool is_final = images[k] == final_image ;
			images[k] = renderer->createVulkanImage(new_width, new_height,images[k]->imageFormat, images[k]->usages) ;
			if(is_final){
				final_image = images[k];
			}
			
		}
		depth = renderer->createVulkanImage(new_width, new_height, depth->imageFormat, depth->usages);
		if(num_fragments > 0){
			createExtendedFragments(num_fragments, frag_size,renderer);
		}
		*/
	}

	void beginRendering(VkCommandBuffer cmd, VulkanPlugin* renderer);

	void endRendering(VkCommandBuffer cmd, VulkanPlugin* renderer);
};

// holds the resources needed for a mesh
template <typename PushConstants, typename Vertex, typename Instance>
class TriangleModel : public Renderable {
public:

	//Shader program we're using, better match the templates!
	std::shared_ptr<TriangleShaderProgram> program;

	// Buffers for the model
	std::shared_ptr<VulkanBuffer> index_buffer;
	std::vector<uint32_t> indices;
	std::shared_ptr<VulkanBuffer> vertex_buffer;
	std::vector<Vertex> vertices;
	bool model_changed = false;
	bool model_size_changed = false;

	//Instance specific data
	std::shared_ptr<VulkanBuffer> instance_buffer;
	std::vector<Instance> instances;
	bool instances_changed = false; // any change to instance data requies pushign the buffer again
	bool num_instances_changed = false; // changign he size of the buffer requires changign the binding descriptor

	// Buffer to hold the command for indirect drawing
	VkBuffer draw_indirect_buffer;
	VkDeviceMemory draw_indirect_buffer_memory;
	bool draw_indirect_buffer_allocated = false;

	VkBuffer last_draw_indirect_buffer; // hold onto reference to previous so we don't clear it while it's still in use
	VkDeviceMemory last_draw_indirect_buffer_memory;

	// Texture data
	std::vector<std::shared_ptr<WFImage>> textures;
	bool textures_changed = false;
	bool has_descriptor = false;
	VkDescriptorSet texture_set_descriptor ;


	PushConstants push_constants ;
	// Locations within the above push_constants of key items
	glm::mat4* push_camera_matrix = nullptr;
	glm::vec3* push_camera_position = nullptr;
	VkDeviceAddress* push_vertex_buffer_location = nullptr;
	VkDeviceAddress* push_instance_buffer_location = nullptr;

	//locations within the above push constanrs for extended fragment data
	int* push_frame_width = nullptr ;
	int* push_frame_height = nullptr;
	int* push_frame_fragments = nullptr;
	VkDeviceAddress* push_fragment_buffer_location = nullptr;
	VkDeviceAddress* push_count_buffer_location = nullptr;


	TriangleModel(std::shared_ptr<TriangleShaderProgram> shaders) {
		program = shaders;
	}

	~TriangleModel(){
		/* TODO This crashes when the app exits, but without it we are leaking a small amount of GPU memory when a model is unloaded
		if (has_descriptor) {
			getTool<VulkanPlugin>()->destroyBinding(texture_set_descriptor);
		}
		*/
	}

	// needs to be called with addresses of the push constants on this model
	// this allows the internal elements of the model to overwrite those push constants wherever they may be
	void setConstantLocations(
		glm::mat4* matrix,
		glm::vec3* camera_position,
		VkDeviceAddress* vertex_buffer_location,
		VkDeviceAddress* instance_buffer_location){
			lock.lock();
			push_camera_matrix = matrix;
			push_camera_position = camera_position ;
			push_vertex_buffer_location = vertex_buffer_location;
			push_instance_buffer_location = instance_buffer_location;
			lock.unlock();
	}

	void setExtendedFragmentLocations(
		int* frame_width, 
		int* frame_height, 
		int* frame_fragments, 
		VkDeviceAddress* fragment_buffer_location,
		VkDeviceAddress* count_buffer_location){
		lock.lock();
		push_frame_width = frame_width ;
		push_frame_height = frame_height;
		push_frame_fragments = frame_fragments;
		push_fragment_buffer_location = fragment_buffer_location ;
		push_count_buffer_location = count_buffer_location ;
		lock.unlock();
	}

	void setModel(const std::vector<Vertex>& new_vertices, const std::vector<uint32_t>& new_indices) {
		lock.lock();
		model_size_changed |= vertices.size() != new_vertices.size() || indices.size() != new_indices.size() ;
		vertices = new_vertices;
		indices = new_indices;
		model_changed = true;
		lock.unlock();
	}

	void setTextures(const std::vector< std::shared_ptr<WFImage>>& new_images) {
		lock.lock();
		textures = new_images;
		textures_changed = true;
		lock.unlock();
	}


	void setInstances(const std::vector<Instance>& new_instances) {
		lock.lock();
		num_instances_changed |= instances.size() != new_instances.size();
		instances = new_instances;
		instances_changed = true;
		lock.unlock();
		//printf("new size: %d\n", new_instances.size()) ;
	}

	
	// Allows setting instances on a TriangleModel through a Renderable withut knowing the exact instant type
	// instances will need to actually match the model
	void setInstances(const std::vector<std::shared_ptr<void>>& new_instances) override {
		lock.lock();
		if(instances.size() != new_instances.size()){
			//printf("num instances changed\n");
			instances = std::vector<Instance>(new_instances.size()) ;
			num_instances_changed = true ;
		}
		
		//auto last_run_time = now();
		
		for(int k=0;k<new_instances.size();k++){
			instances[k] = (*static_cast<Instance*>(new_instances[k].get()));
		}

		//auto current_time = now();
		//float dt = microsBetween(last_run_time, current_time) / 1000000.0f;

		//printf("set Instances: %d size:%d time: %f\n",(int)new_instances.size(),(int)sizeof(Instance), dt) ;
		instances_changed = true;
		lock.unlock();
	}

	// Returns an apprpriately typed instance whose pose can beset
	std::shared_ptr<void> createInstance() override {
		return std::shared_ptr<Instance>(new Instance());
	}
	
	void setInstance(int index, const Instance& instance) {
		lock.lock();
		instances[index] = instance;
		instances_changed = true;
		lock.unlock();
	}

	void setInstancePose(void* instance, const glm::mat4& root_pose, const std::vector<glm::mat4>& bones) override{
		// safe cast: caller must pass the right type
		Instance* typed = static_cast<Instance*>(instance);
		setPose<Instance>(typed, root_pose, bones);
	}	

	Instance getInstance(int index) {
		return instances[index];
	}

	// Note that matrix, and buffer location will be overwritten according to Constant locations
	void setPushConstants(PushConstants& constants){
		lock.lock();
		push_constants = constants ;
		lock.unlock();
	}
	PushConstants getPushConstants() {
		return push_constants ;
	}

	void beginGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override{
		lock.lock();
		target->beginRendering(cmd, renderer);
		program->bindProgram(cmd);
		lock.unlock();
	}

	void endGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override {
	}

	//Push any pending buffer changes, like model, instance, or textures
	void updateBuffers(VkCommandBuffer cmd, VulkanPlugin* renderer) override{
		lock.lock();
		if (model_changed) {
			if(model_size_changed){
				vertex_buffer = renderer->createVulkanBuffer(vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
				index_buffer = renderer->createVulkanBuffer(indices.size() * sizeof(int32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
				model_size_changed = false ;
			}
			renderer->pushBufferData(cmd,vertices, vertex_buffer);
			renderer->pushBufferData(cmd,indices, index_buffer);

			model_changed = false;
			if (debug_print) {
				printf("model update\n");
			}
		}
		if (textures_changed) {
			if (has_descriptor) {
				renderer->destroyBinding(texture_set_descriptor);
			}
			texture_set_descriptor = renderer->createImageBinding(textures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			has_descriptor = true;
			textures_changed = false;
			if (debug_print) {
				printf("textures_updated\n");
			}
		}
		if (instances_changed) {
			if (num_instances_changed) {
				//printf("initializing new buffer size of %d in phase %d\n", (int) instances.size(), phase) ;
				instance_buffer = renderer->createVulkanBuffer(instances.size() * sizeof(Instance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
				num_instances_changed = false;
				draw_indirect_buffer_allocated = false; // the draw command needs to be updated to draw the new amount of instances
			}
			renderer->pushBufferData(cmd, instances, instance_buffer);
			instances_changed = false;
			if (debug_print) {
				printf("instances updated\n");
			}
		}
		lock.unlock();
	}

	void requireTextureLayouts(VkCommandBuffer cmd, VulkanPlugin* renderer) override {
		lock.lock();
		for(std::shared_ptr<WFImage> texture : textures){
			renderer->requireLayout(cmd,texture->getVulkanImage(renderer), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		lock.unlock();
	}

	void render(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override{
		lock.lock();
		if(debug_print){
			printf("render called\n");
		}
		
		//renderer->stampTime(cmd, "D");
		if (!vertex_buffer || !instance_buffer) {
			if (debug_print) {
				printf("skipping rendering due to empty buffer!  V: %d, i: %d\n", (int)vertices.size(), (int)instances.size());
			}
			lock.unlock();
			return; // Not necessarily an error, a model could be created before it's instances are initialized
		}


		//Overwrite the parts of the push constants that are managed internally
		*push_camera_matrix = target->camera_matrix;
		*push_camera_position = target->camera_position;
		*push_vertex_buffer_location = vertex_buffer->device_address;
		*push_instance_buffer_location = instance_buffer->device_address;


		if(push_fragment_buffer_location != nullptr && target->num_fragments > 0){
			*push_frame_width = target->width;
			*push_frame_height = target->height;
			*push_frame_fragments = target->num_fragments;
			*push_fragment_buffer_location = target->fragments->device_address ;
			*push_count_buffer_location = target->counts->device_address ;
		}
		
	
		program->setPushConstants(cmd, push_constants);

		if (!draw_indirect_buffer_allocated) {
			
			if(draw_indirect_buffer){ // if this isn't our first buffer
			
				//clear buffer we were holding onto just in case
				if(last_draw_indirect_buffer){
					vkDestroyBuffer(renderer->device, last_draw_indirect_buffer, nullptr); // delete previous buffer from GPU
					if (draw_indirect_buffer_memory) {
						vkFreeMemory(renderer->device, last_draw_indirect_buffer_memory, nullptr); // TODO should also clean these up when triangle model destructed
					}
				}

				//hold onto buffer for a bit in case it's in use
				last_draw_indirect_buffer = draw_indirect_buffer ;
				last_draw_indirect_buffer_memory = draw_indirect_buffer_memory ;
				
			}

			VkDrawIndexedIndirectCommand drawCmd{};
			drawCmd.indexCount = (uint32_t)(index_buffer->object_count);
			drawCmd.instanceCount = instance_buffer->object_count; // number of instances
			drawCmd.firstIndex = 0;
			drawCmd.vertexOffset = 0;
			drawCmd.firstInstance = 0;

			// Step 1: Create the indirect buffer
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand);
			bufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			vkCreateBuffer(renderer->device, &bufferInfo, nullptr, &draw_indirect_buffer);

			// Step 2: Allocate and bind memory
			VkMemoryRequirements memReq;
			vkGetBufferMemoryRequirements(renderer->device, draw_indirect_buffer, &memReq);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memReq.size;


			allocInfo.memoryTypeIndex = renderer->findMemoryType(memReq.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


			vkAllocateMemory(renderer->device, &allocInfo, nullptr, &draw_indirect_buffer_memory);
			vkBindBufferMemory(renderer->device, draw_indirect_buffer, draw_indirect_buffer_memory, 0);


			void* data;
			vkMapMemory(renderer->device, draw_indirect_buffer_memory, 0, sizeof(drawCmd), 0, &data);
			memcpy(data, &drawCmd, sizeof(drawCmd));
			vkUnmapMemory(renderer->device, draw_indirect_buffer_memory);
			draw_indirect_buffer_allocated = true;


			if (debug_print) {
				printf("Draw indirect initialized with %d\n", instance_buffer->object_count) ;
			}

		}
		//renderer->stampTime(cmd, "E");
		vkCmdBindIndexBuffer(cmd, index_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		if(textures.size() >0){
			program->bindTextures(cmd, texture_set_descriptor);
		}
		//renderer->stampTime(cmd,"F");
		// Make the draw call
		vkCmdDrawIndexedIndirect(cmd, draw_indirect_buffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
		//renderer->stampTime(cmd, "G");

		if (debug_print) {
			printf("draw indirect called %d ,%d\n", (int)vertices.size(), (int)instances.size());

			Variant(*push_camera_matrix).printFormatted();
			Variant(*push_camera_position).printFormatted();

			for(int k=0;k<vertices.size();k++){
				glm::vec4 clip = (*push_camera_matrix) * glm::vec4(vertices[k].position,1);
				printf("%d : %f, %f, %f, %f\n", k, clip.x, clip.y, clip.z, clip.w) ;
			}
		}

		lock.unlock();
	}

	
};


// holds the resources needed for a screen shader execution
template <typename PushConstants, typename Component>
class ScreenModel : public Renderable {
public:
	std::shared_ptr<ScreenShaderProgram> program ;
	std::shared_ptr<VulkanBuffer> component_buffer;
	std::vector<Component> components;
	bool model_changed = false;

	PushConstants push_constants  = {};
	// Locations within the above push_constants of key items
	glm::mat4* push_camera_matrix = nullptr;
	glm::vec3* push_camera_position = nullptr ;
	VkDeviceAddress* push_component_buffer_location = nullptr;

	//locations within the above push constants for extended fragment data
	int* push_frame_width = nullptr;
	int* push_frame_height = nullptr;
	int* push_frame_fragments = nullptr;
	VkDeviceAddress* push_fragment_buffer_location = nullptr;
	VkDeviceAddress* push_count_buffer_location = nullptr;


	std::vector<std::shared_ptr<WFImage>> extra_images ; // extra images ot be appended to render target for this screen shader (like light maps)
	std::unordered_map<std::shared_ptr<RenderTarget>, VkDescriptorSet> target_descriptors ; //save the bindings rather than remaking on each frame

	ScreenModel(std::shared_ptr<ScreenShaderProgram> shader){
		program = shader ;
	}

	// Note that matrix, and buiffer location will be overwritten according to Constant locations
	void setPushConstants(PushConstants& constants) {
		lock.lock();
		push_constants = constants;
		lock.unlock();
	}

	void setModel(const std::vector<Component>& new_components) {
		lock.lock();
		components = new_components;
		model_changed = true;
		lock.unlock();
	}

	// needs to be called with addresses of the push constants on this model
	// this allows the internal elements of the model to overwrite those push constants wherever they may be
	void setConstantLocations(
		glm::mat4* matrix,
		glm::vec3* camera_position,
		VkDeviceAddress* component_buffer_location) {
		lock.lock();
		push_camera_matrix = matrix;
		push_camera_position = camera_position;
		push_component_buffer_location = component_buffer_location;
		lock.unlock();
	}

	void setExtendedFragmentLocations(
		int* frame_width,
		int* frame_height,
		int* frame_fragments,
		VkDeviceAddress* fragment_buffer_location,
		VkDeviceAddress* count_buffer_location) {
		lock.lock();
		push_frame_width = frame_width;
		push_frame_height = frame_height;
		push_frame_fragments = frame_fragments;
		push_fragment_buffer_location = fragment_buffer_location;
		push_count_buffer_location = count_buffer_location;
		lock.unlock();
	}

	void setExtraImages(std::vector<std::shared_ptr<WFImage>>& extra){
		lock.lock();
		extra_images = extra ;
		target_descriptors.clear(); // have to remake the image bindings if the extra image changes
		lock.unlock();
	}

	void beginGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override {
		lock.lock();
		target->endRendering(cmd, renderer); // make sure we're not in an active render
		program->bindProgram(cmd); // bind the program

		std::vector<std::shared_ptr<WFImage>> program_images;
		for (int k = 0; k < target->images.size(); k++) {
			program_images.push_back(target->images[k]);
		}
		for (int k = 0; k < extra_images.size(); k++) {
			program_images.push_back(extra_images[k]);
		}

		if(target_descriptors.find(target) == target_descriptors.end()){
			target_descriptors[target] = renderer->createImageBinding(program_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL);
		}
		program->setImages(target_descriptors[target], target->width, target->height) ;

		// get all the images into the correct layout
		for(auto& v_image : program_images){
			renderer->requireLayout(cmd, v_image->getVulkanImage(renderer), VK_IMAGE_LAYOUT_GENERAL);
		}

		lock.unlock();
	}

	void endGroup(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override {
		// Compute shaders don't have an end command
	}

	void requireTextureLayouts(VkCommandBuffer cmd, VulkanPlugin* renderer) override {
		
	}
	//Push any pending buffer changes, like model, instance, or textures
	void updateBuffers(VkCommandBuffer cmd, VulkanPlugin* renderer) override{
		lock.lock();
		if (model_changed) {
			component_buffer = renderer->createVulkanBuffer(components.size() * sizeof(Component), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			renderer->pushBufferData(cmd, components, component_buffer);
			model_changed = false;
		}
		lock.unlock();
	}

	void render(VkCommandBuffer cmd, VulkanPlugin* renderer, std::shared_ptr<RenderTarget> target) override {
		lock.lock();
		
		*push_camera_matrix = target->camera_matrix;
		*push_camera_position = target->camera_position;
		*push_component_buffer_location = component_buffer->device_address;

		if (push_fragment_buffer_location != nullptr && target->num_fragments > 0) {
			*push_frame_width = target->width;
			*push_frame_height = target->height;
			*push_frame_fragments = target->num_fragments;
			*push_fragment_buffer_location = target->fragments->device_address;
			*push_count_buffer_location = target->counts->device_address;
		}

		program->setPushConstants(cmd, push_constants);
		program->render(cmd);
		lock.unlock();
	}

	
	// Allows setting instances on a TriangleModel through a Renderable withut knowing the exact instant type
	// instances will need to actually match the model
	void setInstances(const std::vector<std::shared_ptr<void>>& new_instances) override {
		throw std::runtime_error("Attempting to set an instances on a screen model! Why?");
	}

	// Returns an apprpriately typed instance whose pose can beset
	virtual std::shared_ptr<void> createInstance() override {
		throw std::runtime_error("Attempting to create an instances on a screen model! Why?");
	}
	
	void setInstancePose(void* instance, const glm::mat4& root_pose, const std::vector<glm::mat4>& bones) override {
		throw std::runtime_error("Attempting to set a pose with a screen modell?");
	}
};



#endif // #ifndef _VULKAN_PLUGIN_H_
