#ifndef _PANEL_PLUGIN_H_
#define _PANEL_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Utilities.h"
#include "VulkanPlugin.h"
#include "OpenXRPlugin.h"

#include "SDL3/SDL_ttf.h"
#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <any>


class PanelPlugin : public AsyncPlugin {

public:

	static inline int PANEL_ELEMENT_PHASE = 3000;
	static inline int PANEL_PHASE = 3001;
	static inline int PANEL_POST_PHASE = 3002;
	static inline glm::mat4 HIDDEN = glm::mat4(0) ;
	

	struct alignas(16) PanelVertex {
		alignas(16) glm::vec3 position;
		alignas(16) glm::vec2 texture_coord;

	};

	struct DefaultPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress vertexBuffer;
		VkDeviceAddress instanceBuffer;
	};

	struct ScreenPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress component_buffer;
	};

	struct DefaultInstance {
		glm::mat4 pose = glm::mat4(0.0f); // pose is required to be able to use setPosition functions on instances
		glm::vec4 bg_color_1 = glm::vec4(0.1, 0, 0.4, 1.0);
		glm::vec4 bg_color_2 = glm::vec4(0.8,0,0.8,0.4);//two colors for a background gradient
		glm::vec4 alpha_border_color = glm::vec4(1, 1, 1, 0.95);
		glm::vec4 box_border_color = glm::vec4(0.0,0.0,0.0,1);
		float alpha_border_width = 4 ;
		float box_border_width = 4;
	};

	class PanelListener {
	public:
		//Called when a pointer enters a panel
		virtual void enterPanel(int panel) = 0 ;

		//Called when a pointer ecits
		virtual void exitPanel(int panel) = 0;

		//Called when a pointer enters a panel element
		virtual void enterPanelElement(int panel, int element) = 0;

		//Called when a pointer exits a panle element
		virtual void exitPanelElement(int panel, int element) = 0;

		//Called when a pointer presses down on an element
		// Note press and release are not guaranteed to sync up as they won't be called if the pointer isn't on a panel
		//element will be -1 if the action is on a panel but not an element
		virtual void pressPanel(int panel, int element, int button) = 0;

		//Called when a pointer releases a press on a panelelement
		// Note press and release are not guaranteed to sync up as the ywon't be called if the pointer isn't on a panel
		//element will be -1 if the action is on a panel but not an element
		virtual void releasePanel(int panel, int element, int button) = 0;

	};

	class AbstractPanelElement {
	public:
		int layer  = 0;
		

		virtual void setPosition(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) = 0;

		virtual void setTexture(std::shared_ptr<WFImage>& texture) = 0;

		virtual glm::ivec2 getTextureDimensions() = 0 ;

		virtual void setPushConstants(const std::any push_constants) = 0;

		virtual void setInstance(const std::any instance) = 0;

		virtual std::any getInstance() = 0 ;

		virtual void addKeyFrame(double time, const std::any instance) = 0;

		virtual void animate(double time) = 0;

		//returns true if the coordinates in texture space intersects this element
		virtual bool intersects(float x, float y) = 0 ;

		virtual void clearKeyFrames() = 0 ;

		virtual void setHidden(bool hide) = 0;

	};

	class AbstractPanel{
	public:
		std::map<int,std::shared_ptr<AbstractPanelElement>> elements ; // first index is layer
		std::shared_ptr<RenderTarget> target ; // render target final image is used as texture for panel shader
		int panel_model_id = -1 ;

		int next_element = 0 ;

		virtual ~AbstractPanel() {}

		//create a new element onthis pane land return its id
		virtual int createElement() = 0 ;

		virtual void setPosition(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) = 0 ;

		virtual void setPushConstants(const std::any push_constants) = 0 ;

		virtual void setInstance(const std::any instance) = 0;

		virtual std::any getInstance()  = 0;

		virtual void addKeyFrame(double time, const std::any instance) = 0 ;

		virtual void animate(double time) = 0;

		void setBackgroundColor(glm::vec4 color);

		virtual void setHidden() = 0 ;

		// returns the local panel coordinates with depth along ray as z
		// z < 0 means no intersection
		virtual glm::vec3 rayTrace(glm::vec3 p, glm::vec3 v) = 0 ;

		virtual void clearKeyFrames() = 0;

	};

	//element contains a struct for additional data to go to the shader
	//Needs to match component shader
	template <typename PanelElementPushConstants, typename PanelElementInstance>
	class PanelElement : public AbstractPanelElement{
	public:
		std::shared_ptr<TriangleModel<PanelElementPushConstants, PanelVertex, PanelElementInstance>> model;
		PanelElementPushConstants push_constants;
		PanelElementInstance instance;
		std::map<double, PanelElementInstance> key_frames ; // used for animation
		int model_id = -1;

		PanelElement(std::shared_ptr<TriangleShaderProgram> element_shader, std::shared_ptr<RenderTarget> target, int group) {
			model = std::shared_ptr<TriangleModel<PanelElementPushConstants, PanelVertex, PanelElementInstance>>(new TriangleModel<PanelElementPushConstants, PanelVertex, PanelElementInstance>(element_shader));
			model->setConstantLocations(
				&model->push_constants.world_matrix,
				&model->push_constants.camera_position,
				&model->push_constants.vertexBuffer,
				&model->push_constants.instanceBuffer);
			std::unordered_set<std::shared_ptr<RenderTarget>> model_targets ;
			model_targets.insert(target) ;
			model->setTargets(model_targets) ;
			model->phase = PANEL_ELEMENT_PHASE ;
			model->group = group;

			instance = PanelElementInstance();
			push_constants = PanelElementPushConstants();
			model->setInstances({ instance });
			model->setPushConstants(push_constants);

			std::vector<PanelVertex> vertices = {
				{glm::vec3(0,0,0), {0,0}},
				{glm::vec3(1,0,0), {1,0}},
				{glm::vec3(0,1,0), {0,1}},
				{glm::vec3(1,1,0),{1,1}}
			};
			std::vector<uint32_t> indices = { 0,2,1,1,2,3 }; // two triangles
			model->setModel(vertices, indices);

			VulkanPlugin* window = getTool<VulkanPlugin>() ;
			model_id = window->addRenderable(model);
		}

		~PanelElement(){
			VulkanPlugin* window = getTool<VulkanPlugin>();
			if(window != nullptr){
				window->removeRenderable(model_id);
			}
		}

		void setPosition(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) override{
			
			if (model->instances.size() > 0) {
				instance = model->instances[0];
			}
			instance.pose = getPose(top_left, X, Y);
			model->setInstances({ instance });
		}

		void setTexture(std::shared_ptr<WFImage>& texture) override {
			VulkanPlugin* window = getTool<VulkanPlugin>();
			// set the texture that will be used to draw this element
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			texture->setSampler(samplerInfo);
			model->setTextures({ texture});
		}

		void setPushConstants(const std::any element_push_constants) {
			push_constants = any_cast<const PanelElementPushConstants>(element_push_constants);
			model->setPushConstants(push_constants);
		}

		void setInstance(const PanelElementInstance& instance) {
			this->instance = instance ;
			model->setInstances({ instance });
		}

		std::any getInstance(){
			return std::any(this->instance) ;
		}

		glm::ivec2 getTextureDimensions(){
			if(model && model->textures.size() > 0 ){
				return glm::ivec2(model->textures[0]->getWidth(), model->textures[0]->getHeight()) ;
			}else{
				return glm::ivec2(0,0);
			}
		}

		void setInstance(const std::any element_instance) {
			instance = any_cast<const PanelElementInstance>(element_instance);
			model->setInstances({ instance });
		}

		void addKeyFrame(double time, const std::any element_instance){
			PanelElementInstance frame = any_cast<const PanelElementInstance>(element_instance);
			key_frames[time] = frame ;
		}

		void animate(double time){
			if (key_frames.size() == 0) {
				return;
			}
			//Remove all but one frame before current time
			int s = (int)key_frames.size();
			auto it = key_frames.lower_bound(time); // first element greater than or equal to time
			if (it != key_frames.begin()) {
				auto keep = std::prev(it); // last element not greater than time so we definitely have it at time
				if (keep != key_frames.begin()) {
					key_frames.erase(key_frames.begin(), keep);//clear out keyframes we'd never use again
				}
			}
			else {//first element is ahead in time, just use it
				setInstance(key_frames.begin()->second);
				return;
			}

			if (key_frames.size() == 1) {
				setInstance(key_frames.begin()->second);
				return;
			}
			//inteprolate first two remaining elements
			auto first = key_frames.begin();
			auto second = std::next(first);
			float t = (float)((time - first->first) / (second->first - first->first));
			setInstance(interpolateObj(first->second, second->second, t));
		}

		//returns true if the coordinates in texture space intersects this element
		bool intersects(float x, float y) override{
			glm::vec4 r = glm::inverse(instance.pose) * glm::vec4(x,y,1,1);
			return r.x >= 0 && r.x <= 1 && r.y >= 0 && r.y <= 1;
		}

		void clearKeyFrames() override{
			key_frames.clear();
		}

		void setHidden(bool hide) override {
			model->hidden = hide;
		}
	};

	//Panels can contain a struct for additional data to the shader
	//Needs to match PanelShader
	template <typename PanelPushConstants, typename PanelInstance, typename PanelElementPushConstants, typename PanelElementInstance>
	class Panel : public AbstractPanel {
	public:
		std::shared_ptr<TriangleModel<PanelPushConstants, PanelVertex, PanelInstance>> model;
		PanelPushConstants push_constants; 
		PanelInstance instance ;
		std::shared_ptr<TriangleShaderProgram> element_shader ;
		std::map<double, PanelInstance> key_frames; // used for animation

		Panel(std::shared_ptr<TriangleShaderProgram> panel_shader, std::shared_ptr<TriangleShaderProgram> element_shader, int texture_width, int texture_height, int group, const glm::vec4& background_color ){
			//create a triangle model to draw the final panel onto the screen
			model = std::shared_ptr<TriangleModel<PanelPushConstants, PanelVertex, PanelInstance>>( new TriangleModel<PanelPushConstants, PanelVertex, PanelInstance>(panel_shader));
			model->setConstantLocations(
				&model->push_constants.world_matrix,
				&model->push_constants.camera_position,
				&model->push_constants.vertexBuffer,
				&model->push_constants.instanceBuffer);
			//Attach the model to be drawn to the screens
			model->phase = PANEL_PHASE;
			model->group = group ;
			VulkanPlugin* window = getTool<VulkanPlugin>();
			std::unordered_set<std::shared_ptr<RenderTarget>> model_targets ;

			model_targets.insert(window->window_target);
			
			if (OpenXRPlugin::ENABLED) {
				OpenXRPlugin* xr = getTool<OpenXRPlugin>();
				model_targets.insert(xr->left_eye_target);
				model_targets.insert(xr->right_eye_target);
			}
			model->setTargets(model_targets);
			panel_model_id = window->addRenderable(model);
			//Initialize the panel texture render target we will draw elements into
			VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			std::shared_ptr<WFImage> color_image = std::shared_ptr<WFImage>(new WFImage(texture_width, texture_height, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));
			std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(texture_width, texture_height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
			target = std::shared_ptr<RenderTarget>(new RenderTarget());
			target->setImages({ color_image}, 
				{ {background_color.r,background_color.g,background_color.b,background_color.a} }, 
				depth_image, 
				color_image);
			target->setOrthoCamera();
			// set the panel to draw with the image of its render target
	
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			color_image->setSampler(samplerInfo);
			model->setTextures({ color_image });

			window->addRenderTarget(target);// make sure it gets cleared every frame
			instance = PanelInstance() ;
			push_constants = PanelPushConstants();
			model->setInstances({ instance });
			model->setPushConstants(push_constants) ;


			std::vector<PanelVertex> vertices = {
				{glm::vec3(0,0,0), {0,0}},
				{glm::vec3(1,0,0), {1,0}},
				{glm::vec3(0,1,0), {0,1}},
				{glm::vec3(1,1,0),{1,1}}
			};
			std::vector<uint32_t> indices = { 0,2,1,1,2,3 }; // two triangles
			model->setModel(vertices, indices);

			this->element_shader = element_shader ;
			//printf("panel renderable id: %d targets: %d \n", panel_model_id, (int) window->getRenderable(panel_model_id)->targets.size()) ;

		}

		~Panel() {
			VulkanPlugin* window = getTool<VulkanPlugin>();
			if(window!=nullptr){
				window->removeRenderable(panel_model_id);
				window->removeRenderTarget(target) ;
			}
		}

		int createElement() override {
			int id = next_element ;
			next_element++;
			elements[id] = std::shared_ptr<AbstractPanelElement>(new PanelElement<PanelElementPushConstants, PanelElementInstance>(element_shader, target, id));
			return id ;
		}

		void setPosition(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) override{
			PanelInstance inst;
			if (model->instances.size() > 0) {
				inst = model->instances[0];
			}
			inst.pose = getPose(top_left, X, Y);
			this->instance = inst ;
			model->setInstances({ inst });
		}

		void setPushConstants(const std::any panel_push_constants){
			push_constants = any_cast<const PanelPushConstants>(panel_push_constants);
			model->setPushConstants(push_constants);
		}

		void setInstance(const PanelInstance& instance) {
			this->instance = instance ;
			model->setInstances({ instance });
		}

		void setInstance(const std::any panel_instance) {
			instance = any_cast<const PanelInstance>(panel_instance);
			this->instance = instance;
			model->setInstances({instance}) ;
		}

		std::any getInstance() {
			return std::any(this->instance);
		}

		void addKeyFrame(double time, const std::any  element_instance) {
			PanelInstance frame = any_cast<const PanelInstance>(element_instance);
			key_frames[time] = frame;
		}

		void clearKeyFrames() override {
			key_frames.clear();
		}

		void animate(double time) {
			//animate all the elements on the panel
			for (auto& [id, element] : elements) {
				element->animate(time);
			}

			if(key_frames.size() == 0){
				return ;
			}
			//Remove all but one frame before current time
			int s = (int)key_frames.size();
			auto it = key_frames.lower_bound(time); // first element greater than or equal to time
			if (it != key_frames.begin()) {
				auto keep = std::prev(it); // last element not greater than time so we definitely have it at time
				if (keep != key_frames.begin()) {
					key_frames.erase(key_frames.begin(), keep);//clear out keyframes we'd never use again
				}
			}else{//first element is ahead in time, just use it
				setInstance(key_frames.begin()->second);
				return ;
			}
			
			if (key_frames.size() == 1) {
				setInstance(key_frames.begin()->second);
				return ;
			}
			//inteprolate first two remaining elements
			auto first = key_frames.begin();
			auto second = std::next(first) ;
			float t = (float)((time- first->first)/ (second->first - first->first)) ;
			setInstance(interpolateObj(first->second, second->second, t));
			
		}


		// returns the local panel coordinates with depth along ray as z
		// z < 0 means no intersection
		glm::vec3 rayTrace(glm::vec3 p, glm::vec3 v) override{
			glm::vec3 X = glm::vec3(instance.pose[0]) ;
			glm::vec3 Y = glm::vec3(instance.pose[1]);
			glm::vec3 O = glm::vec3(instance.pose[3]);

			// mak a plane equation out of the panel's pose matrix
			glm::vec3 N = glm::cross(X,Y) ;
			float d = -glm::dot(N,O) ;
			// intersect the ray with the plane
			float s = (-d - glm::dot(N,p)) / glm::dot(N, v) ;
			
			glm::vec3 r = p + v*s;
			float x = glm::dot(r-O,X) / glm::dot(X,X) ;
			float y = glm::dot(r - O, Y) / glm::dot(Y, Y);
			float z = s ;
			if(x < 0 || x > 1 || y < 0 || y > 1 || z < 0){
				z = -1 ; // did not actually collide
			}
			return glm::vec3(x,y,z) ;
		}

		void setHidden() override {
			model->hidden = instance.pose == HIDDEN;
			// Don't draw the elements if the panel is hidden
			for(auto& [id,element] : elements){
				element->setHidden(model->hidden) ;
			}
		}
	};



	class AbstractShaderSet {
	public:
		std::shared_ptr<TriangleShaderProgram> element_program; // draws the components onto the panels
		std::shared_ptr<TriangleShaderProgram> panel_program; // draws the panels onto an image in the screen's render targets
		std::shared_ptr<ScreenShaderProgram> screen_program;// Composes the rendered panels over the existing screen
		
		
		virtual std::shared_ptr<AbstractPanel> createPanel(int texture_width, int texture_height, int group, const glm::vec4& background_color) = 0;

		virtual void setScreenPushConstants(const std::any push_constants) = 0;

		virtual void setScreenInstance(const std::any screen_instance) = 0;

	};

	//Shader set holds all of the templates needed for the shaders and provides abstract methods to create other elements with proper templates
	template <typename PanelPushConstants, typename PanelInstance, typename PanelElementPushConstants, typename PanelElementInstance, typename ScreenPushConstants, typename ScreenInstance>
	class ShaderSet : public AbstractShaderSet {
	public:
		ScreenPushConstants screen_push_constants;
		ScreenInstance screen_instance ;

		std::shared_ptr<ScreenModel<ScreenPushConstants, ScreenInstance>> screen_model ;

		std::shared_ptr<AbstractPanel> createPanel(int texture_width, int texture_height, int group, const glm::vec4& background_color) override{
			return std::shared_ptr<AbstractPanel>(new Panel<PanelPushConstants, PanelInstance, PanelElementPushConstants, PanelElementInstance>(panel_program, element_program, texture_width, texture_height, group, background_color));
		}

		void setScreenPushConstants(const std::any push_constants) override {
			screen_push_constants = any_cast<const ScreenPushConstants>(push_constants);
			screen_model->setPushConstants(screen_push_constants) ;
		}

		void setScreenInstance(const std::any instance) override {
			screen_instance = any_cast<const ScreenInstance>(instance);
			screen_model->setModel({screen_instance});
		}
	};




	class MenuButton {
	public:
		std::string action;
		std::string text;
		int panel;
		int element;
		glm::mat4 base_pose;
		glm::mat4 hover_pose;
		glm::vec4 text_color = glm::vec4(0.1, 0, 0.1, 1.0);
		glm::vec4 border_color = glm::vec4(0.9, 0.5, 0.2, 1.0);
		glm::vec4 back_color_1 = glm::vec4(0.1, 0, 0.4, 0.8); //TODO add a way to actually override these
		glm::vec4 back_color_2 = glm::vec4(0.7, 0, 0.7, 0.5);


		MenuButton() {} // default constructor allows map of buttons
		MenuButton(int panel_id, const std::string& text, const std::string& action, float x, float y, bool centered, const std::string& font);
		~MenuButton();
	};

	class Label {
	public:
		std::string text;
		int panel;
		int element;
		glm::mat4 pose;
		float x;
		float y;
		bool centered;
		std::string font;
		int image_width = -1;
		int image_height = -1 ;
		glm::vec4 text_color = glm::vec4(0.1, 0, 0.1, 1.0);
		glm::vec4 border_color = glm::vec4(0.9, 0.5, 0.2, 1);
		glm::vec4 back_color_1 = glm::vec4(0.1, 0, 0.4, 1.0);
		glm::vec4 back_color_2 = glm::vec4(0.8, 0, 0.8, 0.4);

		Label() {} // default constructor allows map of buttons
		Label(int panel_id, const std::string& text, float x, float y, bool centered, const std::string& font);

		~Label();

		void setText(std::string text);

		// interpolate to the given position over an amount of time
		void moveTo(float x, float y, bool centered, double time_to_move);

		void setColors(const glm::vec4& bg_1, const glm::vec4& bg_2, const glm::vec4& text, const glm::vec4& border);
	};

	class TextBox {
	public:
		int panel;
		int label_element;
		int text_element;
		int cursor_element;
		int box_element;
		std::string name;
		std::string text;
		std::string label;
		float x;
		float y;
		glm::mat4 label_pose;
		glm::mat4 text_pose;
		glm::mat4 box_pose;
		glm::mat4 cursor_pose;
		double last_update_time = 0;
		int max_text_length = 12;
		bool numbers_only = false;
		int cursor_index = 0;
		int cursor_width = 4;
		int cursor_x;
		int cursor_y;
		int cursor_height;
		std::string font;
		float cursor_blink_speed = 2.0f;
		bool editable = true ;

		TextBox() {}// default constructor allows map of buttons

	
		~TextBox();

		TextBox(int panel_id, const std::string& label, const std::string& text, const std::string& action, float x, float y, const std::string& font, int max_length, bool numeric);

		//Clickin ina text area, selects the box and places the cursor as close as possible in the text
		void select();

		void deselect();


		void setText(const std::string& new_text) ;

		//should be called regularly when the textbox is selected to blink thre cursor and pick up text events
		void update();

		//returns the string currently in the text box
		std::string getString();

		// Returns the integer value if this is a numeric box
		int getInt();
	};

//A navigation link for moving around a menu with a controller or keyboard
	struct NavLink{
		std::pair<int, int> from ; // panel and element
		std::pair<int, int> to; // panel and element
		glm::vec2 direction ; 
	};

	PanelPlugin(VulkanPlugin* window);

	~PanelPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

	//create a panel and return its id
	int createPanel(int texture_width, int texture_height, const glm::vec4& background_color);

	//returns if a panel exists
	bool hasPanel(int panel_id);

	//delete a panel
	void deletePanel(int panel_id);

	//sets panek position in 3D space by top legft corner and vector of X and Y
	void setPanelPosition(int panel_id, const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y);
	
	//create a compoent on a panel in the gien layer and return its id
	int createElement(int panel_id);

	//delete a component from a panel
	void deleteElement(int panel_id, int component_id);

	//sets the image on a component
	void setElementTexture(int panel_id, int element_id, std::shared_ptr<WFImage> image);

	//set component position on panel in 2D texture coordinates
	void setElementPosition(int panel_id, int element_id, const glm::vec2& top_left, const glm::vec2& X, const glm::vec2& Y);

	// sets panel element position in standard orientation
	void setElementPosition(int panel_id, int element_id, const glm::vec2& top_left, float width, float height) ;

	// Add a font that can be used to create text elements
	void addFont(std::string name, std::string file, int size);

	// Rmmove and close a font
	void deleteFont(std::string name);

	//create an image of text using an already loaded font
	std::shared_ptr<WFImage> createTextImage(const std::string& text, const std::string& font, glm::vec4 color, int wrap_length);

	// Returns the dimensions of a single line of text if it were rendered with createTextImage and didn't wrap
	glm::ivec2 getTextLineDimensions(const std::string& text, const std::string& font);

	//returns the current time in seconds since app start that will be used for keyframe animation
	double getTime();

	//Creates a matrix for the instance pose of a panel at the given location
	static glm::mat4 getPose(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) ;

	// Creates a matrix to be put in the instance pose that places a panel in front of and facing a camera
	static glm::mat4 getPoseInFrontOfCamera(const glm::vec3& camera_position, const glm::vec3& camera_look_at, float distance, float width, float height);

	//Creates a matrix for the instance pose of a panel element at the given location
	static glm::mat4 getElementPose(const glm::vec2& top_left, float width, float height, int element_id) ;

	//given a ray of p + v * t
	//returns the indices of the panel and element it intersects respectively
	// returns -1 for element or both if the rya does not interesect anything
	std::pair<int,int> rayTrace(glm::vec3 p, glm::vec3 v);


	glm::vec2 tracePanelTextureCoordinates(int panel_id, glm::vec3 p, glm::vec3 v) ;

	//sets the class thaty will recieve panel events
	void setListener(PanelListener* listener);

	//removes the active listener
	void clearListener();

	//Sets the active pointer by 3D ray
	//May trigger PanelListener enter and exit calls on the thread that calls this
	void setPointerByRay(glm::vec3 p, glm::vec3 v);

	//TODO handle VR 3D pointer and controller based pointer ops

	//Presses with the active pointer and the given button
	// May trigger PanelListener press events on the thread that calls this
	void pressPointer(int button);

	// Releases with the active pointer and the given button
	// May trigger PanelListener release events on the thread that calls this
	void ReleasePointer(int button);

	// Add a link to the navigastion graph for keyboard and controller menu navigation
	void createNavLink(int from_panel_id, int from_element_id, const glm::vec2& direction, int to_panel_id, int to_element_id) ;

	// Remove all nav links going from and to the given indices
	void removeNavLink(int from_panel_id, int from_element_id, const glm::vec2& direction, int to_panel_id, int to_element_id) ;

	//Remove all nav links going from the given element
	void clearNavfrom(int from_panel_id, int from_element_id) ;

	//Sets the current selection (may trigger panel listener enter and exit events on call)
	void setNavSelection(int panel_id, int element_id) ;

	std::pair<int,int> getNavSelection();

	//Move the navigation selection through the graph (may trigger listener enter and exit events on call)
	void moveNavSelection(const glm::vec2& direction);

	// Presses a button on the current nav selection, triggering a panel press event if the selection is valid
	void pressNav(int button) ;

	//Releases a button on the current nav selection, triggering a release event o nthe panel listener if the selection is valid
	void releaseNav(int button) ;

	//Sets extra shader data for panel
	template <typename PanelInstance>
	void setPanelInstance(int panel_id, const PanelInstance& data){
		lock.lock();
		panels[panel_id]->setInstance(data) ;
		lock.unlock();
	}

	// set push constants for panel shader
	template <typename PanelPushConstants>
	void setPanelPushConstants(int panel_id, const PanelPushConstants& push_constants){
		lock.lock();
		panels[panel_id]->setPushConstants(push_constants);
		lock.unlock();
	}

	//set extra shader data for component on panel
	template <typename PanelElementInstance>
	void setPanelElementInstance(int panel_id, int element_id, const PanelElementInstance& data){
		lock.lock();
		panels[panel_id]->elements[element_id]->setInstance(data) ;
		lock.unlock();
	}

	// set push constants for component shader
	template <typename PanelElementPushConstants>
	void setPanelElementPushConstants(int panel_id, int element_id, const PanelElementPushConstants& push_constants){
		lock.lock();
		panels[panel_id]->elements[element_id]->setPushConstants(push_constants);
		lock.unlock();
	}

	//set extra shader data for component on panel
	template <typename PanelElementInstance>
	void setScreenInstanceData(const PanelElementInstance& data) {
		shader_set->setScreenInstance(data);
	}

	// set push constants for component shader
	template <typename PanelElementPushConstants>
	void setScreenPushConstants(const PanelElementPushConstants& push_constants) {
		shader_set->setScreenPushConstants(&push_constants);
	}

	//Sets extra shader data for panel
	template <typename PanelInstance>
	void addPanelKeyFrame(int panel_id, double time, const PanelInstance& data) {
		lock.lock();
		panels[panel_id]->addKeyFrame(time, data);
		lock.unlock();
	}

	//set extra shader data for component on panel
	template <typename PanelElementInstance>
	void addPanelElementKeyFrame(int panel_id, int element_id, double time, const PanelElementInstance& data) {
		lock.lock();
		panels[panel_id]->elements[element_id]->addKeyFrame(time, data);
		lock.unlock();
	}

	template <typename PanelPushConstants, typename PanelInstance, typename PanelElementPushConstants, typename PanelElementInstance, typename ScreenPushConstants, typename ScreenInstance>
	void setShaders(std::shared_ptr<TriangleShaderProgram> element_program, // draws the components onto the panels
			std::shared_ptr<TriangleShaderProgram> panel_program, // draws the panels onto an image in the screen's render targets
			std::shared_ptr<ScreenShaderProgram> screen_program,// Composes the rendered panels over the existing screen
			const ScreenInstance& first_post_component
	){

		auto s = std::shared_ptr<ShaderSet<PanelPushConstants, PanelInstance, PanelElementPushConstants, PanelElementInstance, ScreenPushConstants,  ScreenInstance>>( new 
			ShaderSet<PanelPushConstants, PanelInstance, PanelElementPushConstants, PanelElementInstance, ScreenPushConstants, ScreenInstance>());

		s->screen_model = std::shared_ptr<ScreenModel<ScreenPushConstants, ScreenInstance>>(new ScreenModel<ScreenPushConstants, ScreenInstance>(screen_program));
		s->screen_model->setModel({ first_post_component });
		s->screen_model->setConstantLocations(&s->screen_model->push_constants.world_matrix, &s->screen_model->push_constants.camera_position, &s->screen_model->push_constants.component_buffer);
		s->screen_model->phase = PANEL_POST_PHASE ;
		std::unordered_set<std::shared_ptr<RenderTarget>> screen_model_targets ;
		screen_model_targets.insert(window->window_target);
		if (OpenXRPlugin::ENABLED) {
			OpenXRPlugin* xr = getTool<OpenXRPlugin>();
			screen_model_targets.insert(xr->left_eye_target);
			screen_model_targets.insert(xr->right_eye_target);
		}
		s->screen_model->setTargets(screen_model_targets) ;
		screen_model_id = window->addRenderable(s->screen_model);

		shader_set = s ;
		shader_set->panel_program = panel_program;
		shader_set->element_program = element_program ;
		shader_set->screen_program = screen_program;

	}


	template<typename PanelInstance> 
	PanelInstance getPanelInstance(int panel){
		PanelInstance instance = any_cast<const PanelInstance>(panels[panel]->getInstance());
		return instance ;
	}

	template<typename PanelElementInstance>
	PanelElementInstance getPanelElementInstance(int panel, int element) {
		PanelElementInstance instance = any_cast<const PanelElementInstance>(panels[panel]->elements[element]->getInstance());
		return instance;
	}

	glm::ivec2 getPanelElementTextureDimensions(int panel, int element) {
		return panels[panel]->elements[element]->getTextureDimensions();
	}

	void clearKeyFrames(int panel){
		lock.lock();
		panels[panel]->clearKeyFrames();
		lock.unlock();
	}

	void clearKeyFrames(int panel, int element) {
		lock.lock();
		panels[panel]->elements[element]->clearKeyFrames();
		lock.unlock();
	}

		std::chrono::high_resolution_clock::time_point last_run_time = now();
		double absolute_time = 0;
		PanelListener* listener = nullptr;
	private:
		VulkanPlugin* window ;
		std::map<int, std::shared_ptr<AbstractPanel>> panels ;
		std::shared_ptr<AbstractShaderSet> shader_set ; //holds the shader programsand their templates
		int screen_model_id = -1 ; // id of screen model renderable in vulkan plugin
		int next_panel_id = 0 ;
		std::unordered_map <std::string, TTF_Font*> fonts;
		bool ttf_init = false; // whether true type fonts library has been initialized
		std::pair<int,int> pointer = {-1,-1} ; // current element location of pointer	
		
		std::map<std::pair<int,int>, std::vector<NavLink>> nav_graph ; // graph describing how to mov with a controller (needs to be built by the app)
		std::pair<int, int> nav_select = { -1,-1 }; // current element location of the navigation selection

	

};

//getStructure is required for auto interpolation and must be in global space to work
auto static getStructure(PanelPlugin::DefaultInstance& obj) {
	return std::tie(obj.pose, obj.alpha_border_color, obj.alpha_border_width,obj.bg_color_1,obj.bg_color_2,obj.box_border_color,obj.box_border_width);
}

#endif // #ifndef _PANEL_PLUGIN_H_