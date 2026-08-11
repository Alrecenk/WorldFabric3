#include "VulkanPlugin.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include <vk_types.h>
#include "vk_initializers.h"
#include "FlagSet.h"


#include <array>
#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"


#include "stb_image.h"


// Boots SteamVR and sets up openGL and links to controllers and other hardware
VulkanPlugin::VulkanPlugin(const std::string& title, bool vsync, bool fullscreen) {
		this->title = title;
		vsync_enabled = vsync ;
		// We initialize SDL and create a window with it. 
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

		window = SDL_CreateWindow(
			title.c_str(),
			window_width,
			window_height,
			window_flags
		);

		if(fullscreen){
			SDL_SetWindowFullscreen(window, SDL_TRUE);
			SDL_GetWindowSize(window, &window_width, &window_height);
		}

		if (window == NULL) {
			printf("%s - Window could not be created! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		}
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms) ;

		initVulkan();
		SDL_StartTextInput();
		async_enabled = false; // Needs to run on main thread to access vulkan
}

// Called on every plug-in before any plug-ins are run
void VulkanPlugin::initialize() {
	
}

// pushes image on render target onto window and updates button and mouse
void VulkanPlugin::run() {

	// Clear out any images and buffers whose shared_ptr handles have been lost
	VulkanBuffer::buffer_lock.lock();
	std::chrono::high_resolution_clock::time_point current_time = now();
	std::vector<BufferToDestroy> buffers_to_process = VulkanBuffer::vulkan_buffers_to_destroy; // copy to educe chance of async nonsense
	VulkanBuffer::vulkan_buffers_to_destroy.clear();
	std::vector<BufferToDestroy> vulkan_buffers_to_still_destroy;
	for (BufferToDestroy& buffer : buffers_to_process) {
		if (millisBetween(buffer.time, current_time) > millis_to_hold_buffer && frame_number >= buffer.frame + frames_to_hold_buffer) { // buffers hang around for a bit to allow pending off thread actions to complete
			destroyBuffer(buffer);
		}
		else {
			vulkan_buffers_to_still_destroy.push_back(buffer);
		}
	}
	VulkanBuffer::vulkan_buffers_to_destroy.insert(VulkanBuffer::vulkan_buffers_to_destroy.end(), vulkan_buffers_to_still_destroy.begin(), vulkan_buffers_to_still_destroy.end());
	VulkanBuffer::buffer_lock.unlock();
	VulkanImage::buffer_lock.lock();
	std::vector<ImageToDestroy> vulkan_images_to_still_destroy;
	std::vector<ImageToDestroy> images_to_process = VulkanImage::vulkan_images_to_destroy; // copy to reduce async nonsense
	VulkanImage::vulkan_images_to_destroy.clear();
	for (ImageToDestroy& image : images_to_process) {
		if (millisBetween(image.time, current_time) > millis_to_hold_buffer && frame_number >= image.frame + frames_to_hold_buffer) { // buffers hang around for a bit to allow pending off thread actions to complete
			destroyVulkanImage(image);
		}else {
			vulkan_images_to_still_destroy.push_back(image);
		}
	}
	VulkanImage::vulkan_images_to_destroy.insert(VulkanImage::vulkan_images_to_destroy.end(), vulkan_images_to_still_destroy.begin(), vulkan_images_to_still_destroy.end());
	VulkanImage::buffer_lock.unlock();

	lock.lock();

	std::vector<std::pair<VkSampler, std::chrono::high_resolution_clock::time_point>> samplers_to_still_destroy;
	for (auto& [sampler, time] : samplers_to_destroy) {
		if (millisBetween(time, current_time) > millis_to_hold_buffer) { // buffers hang around for a bit to allow pending off thread actions to complete
			vkDestroySampler(device, sampler, nullptr);
		}
		else {
			samplers_to_still_destroy.push_back({ sampler,time });
		}
	}
	samplers_to_destroy = samplers_to_still_destroy;

	std::vector<std::pair<VkDescriptorSet, std::chrono::high_resolution_clock::time_point>> descriptors_to_still_destroy;
	for (auto& [descriptor, time] : descriptors_to_destroy) {
		if (millisBetween(time, current_time) > millis_to_hold_buffer) { // buffers hang around for a bit to allow pending off thread actions to complete
			actuallyDestroyBinding(descriptor);
		}
		else {
			descriptors_to_still_destroy.push_back({ descriptor,time });
		}
	}
	descriptors_to_destroy = descriptors_to_still_destroy;

	lock.unlock();


	processInput(); // process input both before and after draw to reduce input latency

	if(!minimized){
		draw();
	}
	
	processInput(); // process input both before and after draw to reduce input latency

	lock.lock();
	if (resize_requested) {
		vkDeviceWaitIdle(device);
		SDL_GetWindowSize(window, &window_width, &window_height);
		destroySwapchain(vkbSwapchain, device);
		createSwapchain(window_width, window_height, VK_FORMAT_B8G8R8A8_UNORM, physical_device, device, SDL_vulkan_surface);
		resize_requested = false;
	}
	lock.unlock();
}



// returns the key code of the last key pressed
int VulkanPlugin::getLastKeyPress() {
	return last_key;
}

// Returns whether a key is currently down
bool VulkanPlugin::keyDown(int key_code) {
	auto it = key_state.find(key_code);
	if (it == key_state.end()) {
		return false;
	}
	else {
		return it->second;
	}
}

// Returns the currnet mouse position inte window
glm::vec2 VulkanPlugin::getMousePosition(){
	return mouse_position;
}

// Returns the total net mouse wheel movement within the window
glm::vec2 VulkanPlugin::getMouseWheelPosition(){
	return mouse_wheel_position ;
}

// Returns the 3D direction of the mouse ray from the window_target's position
glm::vec3 VulkanPlugin::getMouseRay(){
	float x = (2.0f * mouse_position.x) / window_target->width - 1.0f;
	float y = (2.0f * mouse_position.y) / window_target->height - 1.0f;
	glm::vec4 clip_point(x, y, 1.0, 1.0f);
	glm::mat4 camera_inverse = glm::inverse(window_target->camera_matrix);
	glm::vec4 world_point = camera_inverse * clip_point;
	glm::vec3 unprojected = world_point / world_point.w;
	return glm::normalize(unprojected - window_target->camera_position);
}

bool VulkanPlugin::mouseDown(int button) {
	return mouse_down[button] ;
}

//Hide the mouse in the window
void VulkanPlugin::hideMouse(){
	mouse_hidden = true ;
}

//Unhide the mouse in the window
void VulkanPlugin::showMouse(){
	mouse_hidden = false ;
}

float VulkanPlugin::getGamepadAxis(Uint32 gamepad_id, Uint32 axis_id){
	auto which_axis = std::pair<Uint32, Uint32>(gamepad_id, axis_id);
	auto found = gamepad_axis.find(which_axis);
	if(found != gamepad_axis.end()){
		return found->second ;
	}else{
		return 0 ;
	}
}

bool VulkanPlugin::getGamepadButton(Uint32  gamepad_id, Uint32 button_id){
	auto which_button = std::pair<Uint32, Uint32>(gamepad_id, button_id);
	auto found = gamepad_button.find(which_button);
	if (found != gamepad_button.end()) {
		return found->second;
	}
	else {
		return false;
	}
}

// returns the gamepaid id and button id of the last button pressed
std::pair<Uint32, Uint32 > VulkanPlugin::getLastGamepadPress(){
	return last_gamepad_button ;
}

// Sets the current typing and string to begin typing
void VulkanPlugin::setTyped(int cursor, std::string text){
	typing_cursor = cursor ;
	typed_text = text;
}

//returns the current typing cursor and string
std::pair<int, std::string> VulkanPlugin::getTyped(){
	return std::pair<int, std::string>(typing_cursor, typed_text) ;
}

//process a typed key code for its affect on the tpyed string or cursor position
void VulkanPlugin::processTyping(SDL_Keycode key_code){
	if(key_code == SDLK_BACKSPACE){
		if(typing_cursor > 0){
			typed_text.erase(typing_cursor - 1, 1);
			typing_cursor--;
		}
	}else if(key_code == SDLK_DELETE){
		if (typing_cursor < typed_text.size()){
			typed_text.erase(typing_cursor, 1);
		}
	}else if(key_code == SDLK_LEFT){
		if (typing_cursor > 0){
			typing_cursor--;
		}
	}else if (key_code == SDLK_RIGHT) {
		if (typing_cursor < typed_text.size()){
			typing_cursor++;
		}
	}else if(key_code == SDLK_HOME){
		typing_cursor = 0;
	}else if(key_code == SDLK_END){
		typing_cursor = (int)typed_text.size();
	}else if(key_code == SDLK_PASTE || (key_code == SDLK_v && SDL_GetModState() & SDL_KMOD_CTRL)){
		//printf("paste detected\n");
		char* clip = SDL_GetClipboardText();
		if(clip == nullptr){
			return ;
		}else{
			std::string str(clip);
			processTyping(str);
			SDL_free(clip) ;
		}
	}
	
}

//process a typed characer for its affect on the tpyed string or cursor position
void VulkanPlugin::processTyping(std::string keys){
	if(keys.length()>0){
		typed_text.insert(typing_cursor, keys);
		typing_cursor += (int)keys.size();
	}
}

void VulkanPlugin::setVSync(bool new_vsync) {
	if (new_vsync != vsync_enabled) {
		vsync_enabled = new_vsync;
		resize_requested = true; //This forces the creation of a new swap chain
	}
}

void VulkanPlugin::initVulkan(){

	vkb::InstanceBuilder builder; // TODO can this builder be deprecated?

	//make the vulkan instance, with basic debug features
	auto inst = builder.set_app_name(title.c_str()) ;
	if(USE_VALIDATION_LAYERS){
		inst = inst.request_validation_layers(USE_VALIDATION_LAYERS)
		.use_default_debug_messenger() ;
	}
	auto inst_ret = inst.require_api_version(1, 3, 0)
		.build();

	if(inst_ret.vk_result() != VK_SUCCESS){
		printf("Vulkan Instantiation failed, type code %d result : %d\n", inst_ret.error().value(), inst_ret.vk_result()) ;
		return ;
	}
	vkb::Instance vkb_inst = inst_ret.value();

	//grab the instance 
	vulkan_instance = vkb_inst.instance;
	debug_messenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(window, vulkan_instance, &SDL_vulkan_surface);

	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	//use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the SDL surface and supports th features we need
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	auto select_result = selector.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(SDL_vulkan_surface)
		.prefer_gpu_device_type()  // defaults to discrete
		.select();

	if(!select_result){
		printf("Required vulkan features not present!\n");
		return ;
	}

	vkb::PhysicalDevice physicalDevice = select_result.value() ;
	printf("Selected Vulkan Device : %s\n", physicalDevice.name.c_str()) ;
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice = deviceBuilder.build().value();
	// Get the VkDevice handle used in the rest of a vulkan application
	device = vkbDevice.device;
	physical_device = physicalDevice.physical_device;

	// use vkbootstrap to get a graphics queue and queue family
	vulkan_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	vulkan_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	//initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physical_device;
	allocatorInfo.device = device;
	allocatorInfo.instance = vulkan_instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &VMA_allocator);

	// Initialize the swap chain for pushing frames to the window
	createSwapchain(window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM,physical_device, device, SDL_vulkan_surface);


	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo command_pool_info = vkinit::command_pool_create_info(vulkan_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	for (int i = 0; i < CHAIN_FRAMES; i++) {
		VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &frames[i].command_pool));
		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo command_alloc_info = vkinit::command_buffer_allocate_info(frames[i].command_pool, 1);
		VK_CHECK(vkAllocateCommandBuffers(device, &command_alloc_info, &frames[i].main_command_buffer));
	}
	VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));
	// allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo command_alloc_info_2 = vkinit::command_buffer_allocate_info(command_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(device, &command_alloc_info_2, &command_buffer));

	//create synchronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to synchronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
	for (int i = 0; i < CHAIN_FRAMES; i++) {
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].render_fence));
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].acquire_fence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchain_semaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].render_semaphore));
	}
	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &main_fence));


	//Create a query pool to hold GPU timings, allowing us to measure how long rendering phases are taking
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = max_time_stamps; 
	vkCreateQueryPool(device, &queryPoolInfo, nullptr, &timestamp_query_pool);

}


void VulkanPlugin::createSwapchain(uint32_t width, uint32_t height, VkFormat swap_chain_image_format, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR SDL_vulkan_surface){
	vkb::SwapchainBuilder swapchainBuilder{ physical_device,device,SDL_vulkan_surface };
	vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = swap_chain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(vsync_enabled ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(width, height)
		.set_required_min_image_count(CHAIN_FRAMES)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	swapchain = vkbSwapchain.swapchain;
	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();
	printf("Swap chain has %d images!\n", (int)swapchainImages.size()) ;

}

void VulkanPlugin::destroySwapchain(vkb::Swapchain swapchain, VkDevice device) {
	// destroy swapchain resources
	auto views = swapchain.get_image_views().value();
	for (int i = 0; i < views.size(); i++) {
		vkDestroyImageView(device, views[i], nullptr);
	}
	vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
}

std::shared_ptr <VulkanImage> VulkanPlugin::createVulkanImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usages) {
	lock.lock();
	std::shared_ptr <VulkanImage> image = std::shared_ptr <VulkanImage>(new VulkanImage());
	image->imageExtent = {
		width,
		height,
		1
	};
	image->imageFormat = format;
	image->usages = usages ;

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;

	image_info.imageType = VK_IMAGE_TYPE_2D;

	image_info.format = format;
	image_info.extent = image->imageExtent;

	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;

	//for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;

	//optimal tiling, which means the image is stored on the best gpu format
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = usages;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(VMA_allocator, &image_info, &alloc_info, &image->image, &image->allocation, nullptr);

	//build a image-view for the draw image to use for rendering

	VkImageViewCreateInfo rview_info = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & usages ?  vkinit::imageview_create_info(image->imageFormat, image->image, VK_IMAGE_ASPECT_DEPTH_BIT) : vkinit::imageview_create_info(image->imageFormat, image->image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &image->imageView));

	image->current_layout = VK_IMAGE_LAYOUT_UNDEFINED ;
	image->depth = format == VK_FORMAT_D32_SFLOAT ;
	lock.unlock();
	return image;
}


void VulkanPlugin::pushImageData(std::shared_ptr<VulkanImage> image, void* data, uint32_t width, uint32_t height){
	lock.lock();
	size_t data_size = width * height * 4;
	std::shared_ptr<VulkanBuffer> uploadbuffer = createVulkanBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadbuffer->info.pMappedData, data, data_size);

	immediateSubmit([&](VkCommandBuffer cmd) {
		transitionVulkanImage(cmd, image->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image->depth);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = VkExtent3D{ width, height, 1 };

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		
		transitionVulkanImage(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image->depth);
		
		});
		image->current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ;
	//destroyBuffer(uploadbuffer);
	lock.unlock();
}

void VulkanPlugin::destroyVulkanImage(ImageToDestroy& image){
	vkDestroyImageView(device, image.imageView, nullptr);
	vmaDestroyImage(VMA_allocator, image.image, image.allocation);
}

// loads an image form a file directly into a vulkan buffer
std::shared_ptr<WFImage> VulkanPlugin::loadImageFromFile(const std::string& path){
	Variant image_file_bytes = Variant::loadFileBytes(path);
	int width = 0;
	int height = 0;
	int channels = 0;
	byte* pixels = stbi_load_from_memory(image_file_bytes.getByteArray(), image_file_bytes.getArrayLength(), &width, &height, &channels, 0);
	
	if(channels == 3){ // we always want to return 4 channels (RGBS) for simplicity
		byte* out_array =(byte*) malloc(width*height*4) ;
		byte* in_array = pixels ;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				out_array[4 * (x + y * width)] = in_array[3 * (x + y * width)];
				out_array[4 * (x + y * width) + 1] = in_array[3 * (x + y * width) + 1];
				out_array[4 * (x + y * width) + 2] = in_array[3 * (x + y * width) + 2];
				out_array[4 * (x + y * width) + 3] = 0xff;
			}
		}
		pixels = out_array;
		delete(in_array);
	}

	std::shared_ptr<WFImage> texture = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
	texture->setImage(pixels, (uint32_t)width, (uint32_t)height) ;
	delete(pixels);
	return texture ;
}

std::shared_ptr<VulkanBuffer> VulkanPlugin::createVulkanBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage){
	lock.lock();
	if(allocSize == 0){
		lock.unlock();
		return std::shared_ptr<VulkanBuffer>();
	}
	//printf("Creating vulkan buffer!\n");
	// allocate buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	std::shared_ptr<VulkanBuffer> newBuffer = std::shared_ptr<VulkanBuffer>(new VulkanBuffer());

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(VMA_allocator, &bufferInfo, &vmaallocInfo, &newBuffer->buffer, &newBuffer->allocation,
		&newBuffer->info));

	// If this buffer will be accessible in a shader
	if(usage & VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT){
		//Find the GPU address that we could pass to the shader for the shader to find it
		VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newBuffer->buffer };
		newBuffer->device_address = vkGetBufferDeviceAddress(device, &deviceAdressInfo);
	}
	lock.unlock();
	return newBuffer;
}

void VulkanPlugin::destroyBuffer(BufferToDestroy& buffer){
	vmaDestroyBuffer(VMA_allocator, buffer.buffer, buffer.allocation);
}


void VulkanPlugin::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function){
	lock.lock();
	VK_CHECK(vkResetFences(device, 1, &main_fence));
	VK_CHECK(vkResetCommandBuffer(command_buffer, 0));

	VkCommandBuffer cmd = command_buffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	// main_fence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(vulkan_queue, 1, &submit, main_fence));
	VK_CHECK(vkWaitForFences(device, 1, &main_fence, true, 9999999999));
	lock.unlock();
}

void VulkanPlugin::processInput(){

	// handle SDL window events
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_KEY_DOWN) {
			processTyping(event.key.keysym.sym);
			if (event.key.repeat == 0) {
				last_key = (int)event.key.keysym.sym;
				key_state[last_key] = true;
			}
		}
		else if (event.type == SDL_EVENT_KEY_UP && event.key.repeat == 0) {
			//printf("Got key up for keycode: %d \n", (int)event.key.keysym.sym);
			key_state[(int)event.key.keysym.sym] = false;
		}
		else if (event.type == SDL_EVENT_TEXT_INPUT) {
			processTyping(event.text.text);
		}
		else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mouse_down[event.button.button] = true;
			mouse_down_position = { event.button.x, event.button.y };
			//printf("Got mouse down\n");
		}
		else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			mouse_down[event.button.button] = false;
		}
		else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			mouse_position = { event.motion.x,event.motion.y };
		}
		else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mouse_wheel_position += glm::vec2({ event.wheel.x, event.wheel.y });
		}
		else if (event.type == SDL_EVENT_QUIT) {
			//TODO should probably provide a way for the app to do something when shutdown this way?
			getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
		}
		else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
			//TODO
		}
		else if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
			minimized = true;
			printf("detected minimized\n");
		}
		else if (event.type == SDL_EVENT_WINDOW_RESTORED) {
			minimized = false;
			printf("restored\n");
		}
		else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
			SDL_Gamepad* the_pad = SDL_OpenGamepad(event.gdevice.which);
			if (the_pad != nullptr) {
				connected_gamepads[event.gdevice.which] = the_pad;
				//printf("Gamepad connected: %d\n", event.gdevice.which) ;
			}
		}
		else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			last_gamepad_button = std::pair<Uint32, Uint32>(event.gbutton.which, event.gbutton.button);
			//printf("g button down\n");
			gamepad_button[last_gamepad_button] = true;
		}
		else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
			auto which_button = std::pair<Uint32, Uint32>(event.gbutton.which, event.gbutton.button);
			gamepad_button[which_button] = false;
			//printf("g button up\n");
		}
		else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
			auto which_axis = std::pair<Uint32, Uint32>(event.gaxis.which, event.gaxis.axis);
			gamepad_axis[which_axis] = event.gaxis.value / 32767.0f; // stored as signed short so map to -1 to 1
			//printf("g axis\n");
		}
	}
	last_sdl_input_num = inputStart();

	

	if (mouse_hidden && !last_mouse_hidden) {
		SDL_HideCursor();
		last_mouse_hidden = true;
	}
	if (!mouse_hidden && last_mouse_hidden) {
		SDL_ShowCursor();
		last_mouse_hidden = false;
	}
}


void VulkanPlugin::draw(){

	auto current_frame = frames[frame_number % CHAIN_FRAMES];
	//wait until the gpu has finished rendering the last frame. Timeout in micros
	VK_CHECK(vkWaitForFences(device, 1, &current_frame.render_fence, true, 500000000));
	logTimes();

	//current_frame._deletionQueue.flush();

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VK_CHECK(vkResetFences(device, 1, &current_frame.acquire_fence));
	VkResult e = vkAcquireNextImageKHR(device, swapchain, 1000000000, current_frame.swapchain_semaphore, current_frame.acquire_fence, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	VkExtent2D _drawExtent ;
	_drawExtent.height = window_height ;
	_drawExtent.width = window_width ;

	VK_CHECK(vkResetFences(device, 1, &current_frame.render_fence));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(current_frame.main_command_buffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = current_frame.main_command_buffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	resetTimes(cmd);
	stampTime(cmd, "Frame start");
	
	for(auto& target : active_targets){
		
		if(target->screen_resize && (target->width != window_width || target->height != window_height)){
			target->resize(window_width, window_height, this) ;
		}
		target->clear(cmd, this);
		
	}

	//printf("Active targets: %d\n", (int)active_targets.size()) ;

	clear(cmd, extra_images_to_clear,extra_clear_values,extra_depth_to_clear) ;
	stampTime(cmd, "clear complete");
	// Waiting for the fence from vkAcquireNextImageKHR causes us to spend our vsync downtime here, where nothing is locked
	VK_CHECK(vkWaitForFences(device, 1, &current_frame.acquire_fence, VK_TRUE, UINT64_MAX)); 
	auto current_time = now();
	int dt_micros = microsBetween(last_sync_time, current_time);
	last_sync_time = current_time;
	if (!vsync_enabled) {
		if (dt_micros > target_frame_micros) {
			sleep_micros -= 50;
		}else {
			sleep_micros += 50;
			if (sleep_micros < 0) {
				sleep_micros = 0;
			}
		}
		if (sleep_micros > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds(sleep_micros));
		}
	}

	stampTime(cmd, "waited for sync");
	drawRenderables(cmd);
	stampTime(cmd, "drawing done");

	//transtion the draw image and the swapchain image into their correct transfer layouts
	requireLayout(cmd, window_target->final_image->getVulkanImage(this), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	transitionVulkanImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false);
	
	// execute a copy from the draw image into the swapchain
	copyVulkanImage(cmd, window_target->final_image->getVulkanImage(this)->image, swapchainImages[swapchainImageIndex], _drawExtent, swapchainExtent);
	//copyVulkanImage(cmd, window_normal_image.image, swapchainImages[swapchainImageIndex], _drawExtent, swapchainExtent);
	
	// set swapchain image layout to Present so we can draw it
	transitionVulkanImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false);
	
	stampTime(cmd, "Frame end");
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the swapchain_semaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the render_semaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, current_frame.swapchain_semaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, current_frame.render_semaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// render_fence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(vulkan_queue, 1, &submit, current_frame.render_fence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the render_semaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &current_frame.render_semaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(vulkan_queue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	frame_number++;
	
}


VulkanBuffer::~VulkanBuffer() {
	buffer_lock.lock();
	vulkan_buffers_to_destroy.emplace_back(buffer,allocation, VulkanPlugin::frame_number, now());
	buffer_lock.unlock();
}

VulkanImage::~VulkanImage() {
	buffer_lock.lock();
	vulkan_images_to_destroy.emplace_back(image,imageView,allocation,VulkanPlugin::frame_number, now());
	buffer_lock.unlock();
}

void VulkanPlugin::clear(VkCommandBuffer cmd, std::vector<std::shared_ptr<WFImage>> output_attachments, std::vector <VkClearColorValue> output_clear, std::vector<std::shared_ptr <WFImage>> depth_attachments){
	//Clear colors
	
	for (int k = 0; k < output_attachments.size(); k++) {
		if (output_attachments[k]->requires_clearing) {
			auto vulkan_image = output_attachments[k]->getVulkanImage(this) ;
			VkImageSubresourceRange range{};
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.baseMipLevel = 0;
			range.levelCount = 1;
			range.baseArrayLayer = 0;
			range.layerCount = 1;
			vkCmdClearColorImage(
				cmd,
				vulkan_image->image,
				vulkan_image->current_layout,
				&output_clear[k],
				1,
				&range
			);
		}
	}
	

	// Clear depth
	for (int k = 0; k < depth_attachments.size(); k++) {
		auto vulkan_image = depth_attachments[k]->getVulkanImage(this);
		VkImageSubresourceRange range2{};
		range2.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // no stencil
		range2.baseMipLevel = 0;
		range2.levelCount = 1;
		range2.baseArrayLayer = 0;
		range2.layerCount = 1;

		VkClearDepthStencilValue clearValue{};
		clearValue.depth = 1.0f;  // far plane
		clearValue.stencil = 0;
		vkCmdClearDepthStencilImage(
			cmd,
			vulkan_image->image,
			vulkan_image->current_layout,
			&clearValue,
			1,
			&range2
		);
	}
}

void VulkanPlugin::clear(VkCommandBuffer cmd, std::shared_ptr<VulkanBuffer> buffer){
	if (!buffer || buffer->buffer == VK_NULL_HANDLE) {
		return; // nothing to clear
	}

	VkDeviceSize buffer_size = buffer->info.size;

	VkDeviceSize aligned_size = buffer_size - buffer_size%4; // round down to multiple of 4

	if (aligned_size == 0) {
		return; // nothing to clear or buffer too small
	}

	// Fill with zeros
	vkCmdFillBuffer(cmd,
		buffer->buffer,
		0,             // offset
		aligned_size,  // number of bytes (multiple of 4)
		0);
	
}

void VulkanPlugin::drawRenderables(VkCommandBuffer cmd){
	std::map<int,std::unordered_map<int,std::vector<std::shared_ptr<Renderable>>>> to_draw; // key is phases and then groups
	lock.lock(); // lock just while iterating renderables
	for(auto& [key, renderable] : renderables){
		to_draw[renderable->phase][renderable->group].push_back(renderable) ;
		renderable->updateBuffers(cmd, this);
		//inputDisplay(renderable->input_num,5, true);
	}
	stampTime(cmd, "buffer updates complete");
	lock.unlock();

	for(auto& [phase, group_map] : to_draw){ // for each user-defined phase
		
		// Wait between each phase
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  // Wait for all previous commands
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  // Block all following commands until done
			0,                                  // No special dependency flags
			0, nullptr,                         // No memory barriers
			0, nullptr,                         // No buffer barriers
			0, nullptr                          // No image barriers
		);
		//stampTime(cmd, concat("phase start ", phase));
		int calls = 0 ;
		lock.lock();
		auto active_render_targets = active_targets ;
		lock.unlock();

		

		for(auto& target : active_render_targets){ // for each render target
			target->setViewport(cmd, this);
			for (auto& [group, group_list] : group_map) { // for each group
				for (int k = 0; k < group_list.size(); k++) {
					group_list[k]->requireTextureLayouts(cmd, this); // make sure all textures to be used for the next render call are in the right layout
				}
			}
			for(auto& [group, group_list] : group_map){ // for each group
				int first= -1;
				for (int k = 0; k < group_list.size(); k++) { // for each renderable
					if(group_list[k]->hasTarget(target) && !group_list[k]->hidden){ // only draw if on active target
						if(first < 0){
							first = k ;
							group_list[k]->beginGroup(cmd,this, target); // only call begin group once per group draw
						}
						group_list[k]->render(cmd,this, target) ;
						calls++;
					}
				}
				if(first >= 0){
					group_list[first]->endGroup(cmd,this, target);
				}
				
			}
			// groups make sure they are rendering or not at start but not at end so the render cmd can be carried over for efficiency
			target->endRendering(cmd, this);
		}
		std::string stamp_tag = concat("phase end ", phase) ;
		stampTime(cmd, stamp_tag);
		calls_in[stamp_tag] = calls ;
	}
}

//TODO this function should be removed, it was generated by chatGPT, uses should be replaced with just the proper bits instead of "finding" them
// Maybe deprecate the memory allocator altogether?
uint32_t VulkanPlugin::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties){
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("Failed to find suitable memory type for Vulkan porperties!");
}

void VulkanPlugin::transitionVulkanImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth){
	VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	imageBarrier.pNext = nullptr;

	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;

	VkImageAspectFlags aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
	imageBarrier.image = image;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.pNext = nullptr;

	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}


void VulkanPlugin::requireLayout(VkCommandBuffer cmd, std::shared_ptr<VulkanImage> image, VkImageLayout layout){
	if(image->current_layout != layout){
		transitionVulkanImage(cmd, image->image, image->current_layout, layout, image->depth);
		image->current_layout = layout ;
	}
}

void VulkanPlugin::copyVulkanImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
	lock.lock();
	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_NEAREST;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
	lock.unlock();
}

VkShaderModule VulkanPlugin::loadShader(const unsigned char* shader_file_contents, uint32_t num_bytes) {
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.codeSize = num_bytes;
	create_info.pCode = (uint32_t*)shader_file_contents;
	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
		throw std::runtime_error("Failed to build required shader.");
	}
	return shader_module;
}


//allocate and set a texture or image binding for a shader
VkDescriptorSet VulkanPlugin::createImageBinding(const std::vector<std::shared_ptr<WFImage>>& images, 
	const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags, const VkImageLayout& image_layout){

	VkDescriptorSetLayout set_layout = getDescriptorLayout(descriptor_type, stage_flags, (int)images.size());
	VkDescriptorSet texture_set = allocateBinding(set_layout, descriptor_type, stage_flags, (int)images.size()) ;

	std::vector<VkDescriptorImageInfo> images_info;
	for (int k = 0; k < images.size(); k++) {
		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = image_layout;
		image_info.imageView = images[k]->getVulkanImage(this)->imageView;
		image_info.sampler = images[k]->getSampler(this);
		images_info.push_back(image_info);
	}


	VkWriteDescriptorSet descriptor_write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptor_write.dstSet = texture_set;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorType = descriptor_type;
	descriptor_write.descriptorCount = (uint32_t)images_info.size();
	descriptor_write.pImageInfo = images_info.data();

	vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);

	return texture_set;

}

//Returns layout for a set of images ot bind to a shader that can be used to allocate a binding
VkDescriptorSetLayout VulkanPlugin::getDescriptorLayout(const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags, int image_count){
	// Match the sampler layout to the way we set up the layout for the pipeline
	std::vector< VkDescriptorSetLayoutBinding> bindings;
	for (int k = 0; k < image_count; k++) {
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = (uint32_t)k;
		binding.descriptorType = descriptor_type;
		binding.descriptorCount = 1;
		binding.stageFlags = stage_flags;
		binding.pImmutableSamplers = nullptr;
		bindings.push_back(binding);
	}

	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = (uint32_t)bindings.size();
	layout_info.pBindings = bindings.data();

	VkDescriptorSetLayout set_layout;
	vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &set_layout);
	return set_layout ;
}

//allocate a binding from a descriptor pool
//Note: you need to cal destroy binding when done to not leak this GPU memory
VkDescriptorSet VulkanPlugin::allocateBinding(const VkDescriptorSetLayout& layout, const VkDescriptorType& descriptor_type, const VkShaderStageFlags& stage_flags, int image_count){
	
	//static const int MAX_DESCRIPTORS_PER_POOL = 256;
	//std::unordered_map<int, std::vector<VkDescriptorPool>> descriptor_pools;//First index is number of images, so each pool only contains descriptors of the same size
	//std::unordered_map<VkDescriptorSet, VkDescriptorPool> descriptor_location; // remember which pools we allocate to for easy cleanup

	bool found_pool = false;
	std::shared_ptr<PoolWithInfo> pool_with_info = nullptr;

	//Try to find an existing pool that matches the number of images and has space for a new allocator
	std::pair<VkDescriptorType, VkShaderStageFlags> signature = std::pair( descriptor_type, stage_flags) ;
	auto it = descriptor_pools.find(signature) ;
	if (it != descriptor_pools.end()) {
		auto it2 = it->second.find(image_count) ;
		if(it2 != it->second.end()){
			std::vector<std::shared_ptr<PoolWithInfo>>& matching_pools = it2->second ;
			for(auto& potential_pool : matching_pools){
				if(potential_pool->used_descriptors < MAX_DESCRIPTORS_PER_POOL){
					pool_with_info = potential_pool ;
					found_pool = true ;
					break ;
				}
			}
		}
	}

	if(!found_pool){
		// Create a new pool in the map
		descriptor_pools[signature][image_count].push_back(std::shared_ptr<PoolWithInfo>(new PoolWithInfo()));
		auto& pool_list = descriptor_pools[signature][image_count] ;
		pool_with_info = pool_list[pool_list.size()-1] ;
		pool_with_info->image_count = image_count ;
		pool_with_info->type = descriptor_type ;
		pool_with_info->stage_flags = stage_flags ;
		VkDescriptorPoolSize pool_size{};
		pool_size.type = descriptor_type;
		pool_size.descriptorCount = (uint32_t)image_count;
		VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // allow vkFreeDescriptorSets ;
		pool_info.maxSets = MAX_DESCRIPTORS_PER_POOL;
		pool_info.poolSizeCount = 1;
		pool_info.pPoolSizes = &pool_size;
		vkCreateDescriptorPool(device, &pool_info, nullptr, &pool_with_info->pool);
	}

	// Allocate some space for the texture set
	VkDescriptorSetAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = pool_with_info->pool ;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;
	VkDescriptorSet binding;
	vkAllocateDescriptorSets(device, &alloc_info, &binding);
	descriptor_location[binding] = {pool_with_info,layout} ;
	pool_with_info->used_descriptors++;

	return binding;
}

void VulkanPlugin::destroyBinding(const VkDescriptorSet& binding) {
	descriptors_to_destroy.emplace_back(binding, now()) ;
}

//Cleans up a descriptor set created with allocateBinding
void VulkanPlugin::actuallyDestroyBinding(const VkDescriptorSet& binding){
	auto it = descriptor_location.find(binding) ;
	if(it == descriptor_location.end()){
		printf("Attempting to destroy a descriptor that isn't in the list! Bindings must be allocated with allocateBinding for autocleanup!\n"); 
		return ;
	}
	std::shared_ptr<PoolWithInfo> pool_info = it->second.first ;

	pool_info->used_descriptors -- ;
	VkDescriptorPool pool = pool_info->pool ;
	vkFreeDescriptorSets(device, pool, 1, &binding);
	VkDescriptorSetLayout layout = it->second.second;
	vkDestroyDescriptorSetLayout(device,layout,nullptr) ;
	descriptor_location.erase(it);

	if(pool_info->used_descriptors == 0){
		std::pair<VkDescriptorType, VkShaderStageFlags> signature = std::pair(pool_info->type, pool_info->stage_flags);
		auto it = descriptor_pools.find(signature);
		if (it != descriptor_pools.end()) {
			auto it2 = it->second.find(pool_info->image_count);
			if (it2 != it->second.end()) {
				std::vector<std::shared_ptr<PoolWithInfo>>& matching_pools = it2->second;
				std::vector<std::shared_ptr<PoolWithInfo>> new_pool_list ;
				for (auto& potential_pool : matching_pools) {
					if(potential_pool->used_descriptors == 0){
						vkDestroyDescriptorPool(device,potential_pool->pool, nullptr);
					}else{
						new_pool_list.push_back(potential_pool) ;
					}
				}
				descriptor_pools[signature][pool_info->image_count] = new_pool_list ;
			}
		}
	
	}
	//TODO consider cleaning up empty pools entirely? Maybe not though, this can only grow with the number of unique shader binding allocation types.
}


void VulkanPlugin::enableRenderTiming(const std::string& file) {
	timing_log = std::shared_ptr<CSVLog>(new CSVLog(file, "action", "time(microseconds)", "render calls"));
	timing_enabled = true;
}

void VulkanPlugin::resetTimes(VkCommandBuffer cmd){
	vkCmdResetQueryPool(cmd, timestamp_query_pool, 0, max_time_stamps);
	timestamp_names.clear();
}
void VulkanPlugin::stampTime(VkCommandBuffer cmd, const std::string& name){
	if(timing_enabled && timestamp_names.size() < max_time_stamps){
		uint32_t query_index = (uint32_t)timestamp_names.size(); // advance each time you write one
		vkCmdWriteTimestamp(
			cmd,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // all prior work must complete first
			timestamp_query_pool,
			query_index
		);
		timestamp_names.push_back(name);
	}
}
void VulkanPlugin::logTimes(){
	if(timing_enabled && timestamp_names.size()>0){
		std::vector<uint64_t> timestamps((uint32_t)timestamp_names.size());
		VkResult result = vkGetQueryPoolResults(
			device,
			timestamp_query_pool,
			0,
			(uint32_t)timestamp_names.size(),
			timestamps.size() * sizeof(uint64_t),
			timestamps.data(),
			sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
		);
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physical_device, &props);
		double ns_per_tick = props.limits.timestampPeriod;
		auto current_time = now() ;
		bool printing = microsBetween(last_log_time,current_time ) > 1000000 * log_print_interval_seconds;
		amount_timed++ ;
		double total_time = 0 ;
		for(int k=1;k<timestamp_names.size();k++){
			double relative_time_ns = (timestamps[k] - timestamps[k-1]) * ns_per_tick;
			std::string label = timestamp_names[k - 1] + " to " + timestamp_names[k] ;
			time_totals[label] += relative_time_ns; // accumulate total nanoseconds/100 to avoid overflow for large intervals
			total_time += relative_time_ns;
			if(printing){
				timing_log->log(label, (long)(time_totals[label]/ (amount_timed * 1000)), calls_in[timestamp_names[k]]); // print microseconds per frame and number of calls
				time_totals[label] = 0 ;
			}
}

		std::string label = "Total command buffer time";
		time_totals[label] += total_time ; // this needs to be divided early to avoid overflow for larger intervals
		
		if(printing){
			timing_log->log(label, (long)(time_totals[label]/(amount_timed*1000)));
			time_totals[label] = 0 ;
			last_log_time = current_time ;
			amount_timed = 0 ;
		}

	}
	
}

TriangleShaderProgram::TriangleShaderProgram(
	VkDevice device,
	VkShaderModule vertex_shader, 
	VkShaderModule fragment_shader,
	int push_constant_struct_size, 
	int num_textures,
	VkCullModeFlagBits cull_mode,
	std::shared_ptr<RenderTarget> format_example,
	FragmentBlendMode blend_mode) {

	this->device = device ; // save the device so we don't have to do pass it for every function,it can't change anyway since we're binding memory through it
	
	// The program info we need to fill out to initialize the shader program
	VkGraphicsPipelineCreateInfo program_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	// Start by creating a pipeline layout that can handle the push constants and textures our shader needs
	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.pNext = nullptr;
	pipeline_layout_info.flags = 0;
	//Describe shape of push constants in the layout (it's fixed to be one struct of a given size)
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = push_constant_struct_size;
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;

	//Describe shape of image samplers used in the shader
	// We're making every texture a single sampler (not an array) and they're all in set = 0 and only in the fragment shader, so the GLSL needs to match that pattern
	std::vector< VkDescriptorSetLayoutBinding> texture_bindings ;
	for(int k=0;k<num_textures;k++){
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = k; 
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // image and sampler are grouped together
		binding.descriptorCount = 1; // no array textures
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Fragment shader access only
		binding.pImmutableSamplers = nullptr; // We'll supply the sampler later
		texture_bindings.push_back(binding) ;
	}
	// One set layout with a the sequence of bindings
	VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
	textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutInfo.bindingCount = num_textures;
	textureLayoutInfo.pBindings = texture_bindings.data();
	VkDescriptorSetLayout textureSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &textureSetLayout));

	// Add descriptor set layout for textures to the pipeline info
	pipeline_layout_info.setLayoutCount = 1; // all in one set, set = 0
	VkDescriptorSetLayout setLayouts[] = { textureSetLayout };
	pipeline_layout_info.pSetLayouts = setLayouts;

	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &layout));
	program_info.layout = layout;

	// Connect the output formats to the rendering info
	VkPipelineRenderingCreateInfo render_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

	std::vector<VkFormat> output_image_formats ;
	num_color_attachments = (int)format_example->images.size() ;
	for(int k=0;k<num_color_attachments; k++){
		output_image_formats.push_back(format_example->images[k]->getFormat());
	}

	render_info.colorAttachmentCount = (uint32_t)output_image_formats.size();
	render_info.pColorAttachmentFormats = output_image_formats.data();
	render_info.depthAttachmentFormat = format_example->depth->getFormat() ;
	program_info.pNext = &render_info;

	// Put the shaders intothe pipeline
	const char* entry_function = "main" ; // This is standard for shaders, and it doesn't need to be configurable
	
	VkPipelineShaderStageCreateInfo vertex_stage_info{};
	vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_stage_info.pNext = nullptr;
	vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT; 
	vertex_stage_info.module = vertex_shader; 
	vertex_stage_info.pName = entry_function; 

	VkPipelineShaderStageCreateInfo fragment_stage_info{};
	fragment_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_stage_info.pNext = nullptr;
	fragment_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT; 
	fragment_stage_info.module = fragment_shader; 
	fragment_stage_info.pName = entry_function;

	std::vector< VkPipelineShaderStageCreateInfo> shader_stages = { vertex_stage_info, fragment_stage_info} ;
	program_info.stageCount = (uint32_t)shader_stages.size();
	program_info.pStages = shader_stages.data();

	// Set it to draw filled triangles
	VkPipelineInputAssemblyStateCreateInfo input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE; // we don't use this,so turn it off I guess
	program_info.pInputAssemblyState = &input_assembly;


	// We could render to subsets of the outputs, but we're not gonna do that here, so just default this tothewhol frame
	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = nullptr;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	program_info.pViewportState = &viewport_state;

	// Set the rasterizer info based on our cull_mode
	VkPipelineRasterizationStateCreateInfo rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // fixed to polygon fill cause it's a triangle pipeline
	rasterizer.lineWidth = 1.f; // probably not used
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.cullMode = cull_mode ;
	program_info.pRasterizationState = &rasterizer;

	//Set up mutlisampling, disabled for now but gonna want this later TODO
	VkPipelineMultisampleStateCreateInfo multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // multisampling defaulted to no multisampling (1 sample per pixel)
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;// no alpha to coverage either
	multisampling.alphaToOneEnable = VK_FALSE;
	program_info.pMultisampleState = &multisampling ;

	// Set up the blending modes for each output
	std::vector<VkPipelineColorBlendAttachmentState> blend_mode_attachments ; // one per output attachment
	
	VkPipelineColorBlendAttachmentState color_blend = {};
	color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (blend_mode == OVERWRITE) {
		color_blend.blendEnable = VK_FALSE;
	}
	else if (blend_mode == IGNORE) {
		color_blend.blendEnable = VK_TRUE;
		color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	}
	else {
		color_blend.blendEnable = VK_TRUE;
		color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		if (blend_mode == ADDITIVE) {
			color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		}
		else if (blend_mode == ALPHA_BLEND) {
			color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		}
	}
	
	for(int k = 0 ; k < num_color_attachments;k++){
		blend_mode_attachments.push_back(color_blend);
	}
	VkPipelineColorBlendStateCreateInfo blending_info = {};
	blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blending_info.pNext = nullptr;
	blending_info.logicOpEnable = VK_FALSE;
	blending_info.logicOp = VK_LOGIC_OP_COPY;
	blending_info.attachmentCount = (uint32_t)blend_mode_attachments.size();
	blending_info.pAttachments = blend_mode_attachments.data();
	program_info.pColorBlendState = &blending_info;

	// Enable depth testing
	VkPipelineDepthStencilStateCreateInfo depth_stencil  = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = true;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // less depth means closer, no configuring this!
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;
	depth_stencil.front = {};
	depth_stencil.back = {};
	depth_stencil.minDepthBounds = 0.f;
	depth_stencil.maxDepthBounds = 1.f;
	program_info.pDepthStencilState = &depth_stencil;

	
	// We're not using this stuff and I don't know what it's for, but I guess it needs to be defined?
	VkPipelineVertexInputStateCreateInfo vertex_input_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	program_info.pVertexInputState = &vertex_input_info;
	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_info.pDynamicStates = &state[0];
	dynamic_info.dynamicStateCount = 2;
	program_info.pDynamicState = &dynamic_info;

	// Attempt to build the pipeline
	VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &program_info, nullptr, &pipeline) ;
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create triangle shader program!");
	}
}

void TriangleShaderProgram::bindTextures(VkCommandBuffer cmd, VkDescriptorSet texture_bindings){
	vkCmdBindDescriptorSets(cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		layout,
		0, // set index
		1, &texture_bindings,
		0, nullptr);
}

void TriangleShaderProgram::bindProgram(VkCommandBuffer cmd){
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

// Initialize a triangle rendering pipeline and make it avilable with the given configurations
ScreenShaderProgram::ScreenShaderProgram(VkDevice device, VkShaderModule compute_shader, int push_constant_struct_size, const std::vector< std::shared_ptr<WFImage>>& images, int local_block_size) {
	this->device = device ;
	VulkanPlugin* window = getTool<VulkanPlugin>();
		
	image_descriptors = window->createImageBinding(images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL);
	descriptor_layout = window->getDescriptorLayout(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, (int)images.size()) ;
	local_size = local_block_size ;
	image_width = images[0]->getWidth() ;
	image_height = images[0]->getHeight();

	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &descriptor_layout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = push_constant_struct_size;
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &layout));

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = compute_shader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = layout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));
	//printf("done creating program\n");
}

// Bind this program for calls
void ScreenShaderProgram::bindProgram(VkCommandBuffer cmd){
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}


void ScreenShaderProgram::render(VkCommandBuffer cmd){
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &image_descriptors, 0, nullptr);
	vkCmdDispatch(cmd, (uint32_t)std::ceil(image_width / (float)local_size), (uint32_t)std::ceil(image_height / (float)local_size), 1);
}

void ScreenShaderProgram::updateImageSize(int w, int h){
	image_width = w ;
	image_width = h ;
}

void ScreenShaderProgram::setImages(VkDescriptorSet image_bindings, int w, int h){
	image_descriptors = image_bindings ;
	image_width = w ;
	image_height = h ;
}


WFImage::WFImage(uint32_t  width, uint32_t  height, VkFormat format, VkImageUsageFlags usages){
	this->width = width;
	this->height = height ;
	this->format = format ;
	this->usages = usages ;
	needs_created = true ;

}

WFImage::~WFImage(){
	if (has_sampler) {
		VulkanPlugin::samplers_to_destroy.push_back({ texture_sampler,now() });
	}
}

void WFImage::setImage(void* data, uint32_t  width, uint32_t  height){
	this->width = width ;
	this->height =height ;
	pending_data = Variant((byte*)data, width * height * 4) ; // this is a copy, the sender can/needs to clean up the raw pointer
	needs_data_push = true ;
}


void WFImage::setSampler(const VkSamplerCreateInfo& sampler_info){
	this->sampler_info = sampler_info ;
	needs_sampler = true ;
}
std::shared_ptr<VulkanImage> WFImage::getVulkanImage(VulkanPlugin* r){
	if(needs_created){
		vulkan_image = r->createVulkanImage(width, height, format, usages);
		needs_created = false ;
	}
	if(needs_data_push){
		r->pushImageData(vulkan_image,pending_data.getByteArray(),width, height) ;
		needs_data_push = false;
	}
	return vulkan_image ;
}

VkSampler WFImage::getSampler(VulkanPlugin* r){
	if(needs_sampler){
		if(has_sampler){
			VulkanPlugin::samplers_to_destroy.push_back({ texture_sampler,now() });
		}
		vkCreateSampler(r->device, &sampler_info, nullptr, &texture_sampler);
		has_sampler = true ;
		needs_sampler = false;
	}
	return texture_sampler ;
}

uint32_t WFImage::getWidth(){
	return width ;
}
uint32_t WFImage::getHeight(){
	return height ;
}
VkFormat WFImage::getFormat(){
	return format ;
}
VkImageUsageFlags WFImage::getUsages(){
	return usages ;
}



void RenderTarget::beginRendering(VkCommandBuffer cmd, VulkanPlugin* renderer) {
	if(is_rendering){
		return ;
	}
	is_rendering = true ;
	std::vector<VkRenderingAttachmentInfo> output_info;
	for (int k = 0;k < images.size();k++) {
		renderer->requireLayout(cmd, images[k]->getVulkanImage(renderer), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); // make sure the images we're drawing to are in th drawing to layout
		VkRenderingAttachmentInfo attachment = {};
		attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachment.imageView = images[k]->getVulkanImage(renderer)->imageView;
		attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if (needs_clear) {
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachment.clearValue.color = clear_values[k];
		}
		output_info.push_back(attachment);
	}

	VkRenderingAttachmentInfo depth_info = {};
	depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_info.imageView = depth->getVulkanImage(renderer)->imageView;
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	
	depth_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	if (needs_clear) {
		depth_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_info.clearValue.depthStencil = { 1.0f, 0 }; // used for depth 0 at viewer and high for far away
		needs_clear = false;
	}

	renderer->requireLayout(cmd, depth->getVulkanImage(renderer), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL); // make sure the images we're drawing to are in th drawing to layout

	VkRenderingInfo render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.pNext = nullptr;
	render_info.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, VkExtent2D { depth->getWidth(), depth->getHeight()} };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = (uint32_t)output_info.size();
	render_info.pColorAttachments = output_info.data();
	render_info.pDepthAttachment = &depth_info;
	render_info.pStencilAttachment = nullptr;

	vkCmdBeginRendering(cmd, &render_info);

}

void RenderTarget::endRendering(VkCommandBuffer cmd, VulkanPlugin* renderer) {
	if(!is_rendering){
		return ;
	}
	is_rendering = false ;
	vkCmdEndRendering(cmd);
	
}
