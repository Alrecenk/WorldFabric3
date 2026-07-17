#include "PanelPlugin.h"



PanelPlugin::PanelPlugin(VulkanPlugin* window){
	this->window = window ;
}

// Called on every plug-in before any plug-ins are run
void PanelPlugin::initialize(){
}


void PanelPlugin::run(){
	lock.lock();
	auto current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	absolute_time += dt;
	last_run_time = current_time;
	for(auto& [panel_id, panel] : panels){
		panel->animate(absolute_time) ;
		panel->setHidden();
	}
	lock.unlock();
}

//create a panel and return its id
int PanelPlugin::createPanel(int texture_width, int texture_height, const glm::vec4& background_color){
	lock.lock();
	int panel_id = next_panel_id;
	next_panel_id++;
	panels[panel_id] = shader_set->createPanel(texture_width, texture_height, panel_id, background_color) ;
	lock.unlock();
	return panel_id ;
}

bool PanelPlugin::hasPanel(int panel_id){
	return panels.find(panel_id) != panels.end() ;
}
//delete a panel
void PanelPlugin::deletePanel(int panel_id){
	lock.lock();
	panels.erase(panel_id) ;
	lock.unlock();
}

//sets panek position in 3D space by top legft corner and vector of X and Y
void PanelPlugin::setPanelPosition(int panel_id, const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y){
	lock.lock();
	panels[panel_id]->setPosition(top_left, X , Y) ;
	lock.unlock();
}


//create a compoent on a panel in the gien layer and return its id
int PanelPlugin::createElement(int panel_id){
	lock.lock();
	int element = panels[panel_id]->createElement();
	lock.unlock();
	return element ;
}

//delete a component from a panel
void PanelPlugin::deleteElement(int panel_id, int element_id){
	lock.lock();
	if(panels.find(panel_id) != panels.end()){
		panels[panel_id]->elements.erase(element_id) ;
	}
	lock.unlock();
}


//sets the texture on an element
void PanelPlugin::setElementTexture(int panel_id, int element_id, std::shared_ptr<WFImage> image){
	panels[panel_id]->elements[element_id]->setTexture(image);
}

//set component position on panel in 2D [0,1] texture coordinates
void PanelPlugin::setElementPosition(int panel_id, int element_id, const glm::vec2& top_left, const glm::vec2& X, const glm::vec2& Y) {
	panels[panel_id]->elements[element_id]->setPosition(glm::vec3(top_left,20.0f), glm::vec3(X,0), glm::vec3(Y,0)) ;

}

//set component position on panel in 2D [0,1] texture coordinates
void PanelPlugin::setElementPosition(int panel_id, int element_id, const glm::vec2& top_left, float width, float height) {
	panels[panel_id]->elements[element_id]->setPosition(glm::vec3(top_left, 1000-element_id), glm::vec3(width,0, 0), glm::vec3(0,height, 0));

}

// Creates a matrix to be put in the instance pose that places a panel at the given location and extent
glm::mat4 PanelPlugin::getElementPose(const glm::vec2& top_left, float width, float height, int element_id) {
	return getPose(glm::vec3(top_left, 1000-element_id), glm::vec3(width, 0, 0), glm::vec3(0, height, 0));
}


void PanelPlugin::addFont(std::string name, std::string file, int size) {
	lock.lock();
	if (!ttf_init) {
		TTF_Init();
		ttf_init = true;
	}
	fonts[name] = TTF_OpenFont(file.c_str(), size);
	lock.unlock();
}

// Close a font
void PanelPlugin::deleteFont(std::string name) {
	lock.lock();
	TTF_CloseFont(fonts[name]);
	fonts.erase(name);
	lock.unlock();
}

//create an image of text using an already loaded font
std::shared_ptr<WFImage> PanelPlugin::createTextImage(const std::string& text, const std::string& font, glm::vec4 color, int wrap_length){
	if(fonts.find(font) == fonts.end()){
		printf("Atempted to render textwith a font that coulnd't be found: %s\n", font.c_str()) ;
		return nullptr ;
	}

SDL_Color s_color;
	s_color.r = (int)(255.4f * color.b);// blue and red are swapped in render text, no idea why
	s_color.g = (int)(255.4f * color.g);
	s_color.b = (int)(255.4f * color.r);
	s_color.a = (int)(255.4f * color.a);
	SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(fonts[font], text.c_str(), s_color, wrap_length);

	if (surface == nullptr) {
		printf("create render text returned null ptr! Error: %s\n", TTF_GetError());
		return nullptr ;
	}
	else {
		uint32_t width = surface->pitch / 4; // There's padding in surface->pixels not reported in surface->w, this is the true width of the image in pixels
		uint32_t height = surface->h ;
		VulkanPlugin* window = getTool<VulkanPlugin>();
		std::shared_ptr<WFImage> image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) ;
		image->setImage(surface->pixels, width, height);
		SDL_DestroySurface(surface);
		return image ;
	}

}

// Returns the dimensions of a single line of text if it were rendered with createTextImage and didn't wrap
glm::ivec2 PanelPlugin::getTextLineDimensions(const std::string& text, const std::string& font){
	glm::ivec2 dim ;
	TTF_SizeUTF8(fonts[font], text.c_str(), &dim.x, &dim.y);
	return dim;
}

PanelPlugin::~PanelPlugin() {
	std::vector<std::string> to_delete;
	for (auto& [name, font] : fonts) {
		to_delete.push_back(name);
	}

	for (auto& name : to_delete) {
		deleteFont(name);
	}
}

double PanelPlugin::getTime() {
	return absolute_time;
}

// Creates a matrix to be put in the instance pose that places a panel at the given location and extent
glm::mat4 PanelPlugin::getPose(const glm::vec3& top_left, const glm::vec3& X, const glm::vec3& Y) {
	glm::mat4 m;
	m[0] = glm::vec4(X, 0);
	m[1] = glm::vec4(Y, 0);
	m[2] = glm::vec4(glm::normalize(glm::cross(X,Y)), 0);
	m[3] = glm::vec4(top_left, 1);
	return m ;
}

// Creates a matrix to be put in the instance pose that places a panel in front of and facing a camera
glm::mat4 PanelPlugin::getPoseInFrontOfCamera(const glm::vec3& camera_position, const glm::vec3& camera_look_at, float distance, float width, float height) {
	glm::vec3 z = camera_look_at - camera_position;
	glm::vec3 y = glm::vec3(0.0001, 1, 0.0002f); // little bit of x and z in case camera is facing straight vertical
	glm::vec3 x = glm::cross(z, y);
	y = glm::cross(x, z);
	z = glm::normalize(z) * distance;
	x = glm::normalize(x) * width;
	y = glm::normalize(y) * height;

	return getPose(camera_position + z - x * 0.5f - y * 0.5f, x , y );
}


//given a ray of p + v * t
//returns the indices of the panel and element it intersects respectively
// returns -1 for element or both if the rya does not interesect anything
std::pair<int, int> PanelPlugin::rayTrace(glm::vec3 p, glm::vec3 v){
	lock.lock();
	std::pair<int,int> result = {-1,-1} ;
	float closest = FLT_MAX ;
	for (auto& [panel_id, panel] : panels) {
		glm::vec3 panel_coord = panel->rayTrace(p,v) ;
		if(panel_coord.z >= 0 && panel_coord.z < closest){
			result.first = panel_id ;
			closest = panel_coord.z ;
			for(auto& [element_id, element] : panel->elements){
				if(element->intersects(panel_coord.x*panel->target->width, (1.0f-panel_coord.y) * panel->target->height)){
					result.second = element_id ;
				}
			}
		
		}
	}
	lock.unlock();
	return result ;
}

glm::vec2 PanelPlugin::tracePanelTextureCoordinates(int panel_id, glm::vec3 p, glm::vec3 v){
	glm::vec3 panel_coord = panels[panel_id]->rayTrace(p, v);
	return glm::vec2(panel_coord.x* panels[panel_id]->target->width, (1.0f - panel_coord.y)* panels[panel_id]->target->height) ;
}

//sets the class thaty will recieve panel events
void PanelPlugin::setListener(PanelListener* listener){
	this->listener = listener ;

}

//removes the active listener
void PanelPlugin::clearListener(){
	this->listener = nullptr ;
}

//Sets the active pointer by 3D ray
//May trigger PanelListener enter and exit calls on the thread that calls this
void PanelPlugin::setPointerByRay(glm::vec3 p, glm::vec3 v){
	std::pair<int,int> new_pointer = rayTrace(p,v);
	if(listener != nullptr){
		// first exit element
		if (new_pointer.second != pointer.second) {
			if (pointer.second != -1) {
				listener->exitPanelElement(pointer.first, pointer.second);
			}
			
		}
		// then exit panel, then enter panel
		if(new_pointer.first != pointer.first){
			if(pointer.first != -1){
				listener->exitPanel(pointer.first) ;
			}
			if(new_pointer.first != -1){
				listener->enterPanel(new_pointer.first) ;
			}
		}
		//then enter element last
		if (new_pointer.second != pointer.second) {
			if (new_pointer.second != -1) {
				listener->enterPanelElement(new_pointer.first, new_pointer.second);
			}
		}
	}
	pointer = new_pointer ;
}

//TODO handle VR 3D pointer and controller based pointer ops

//Presses with the active pointer and the given button
// May trigger PanelListener press events on the thread that calls this
void PanelPlugin::pressPointer(int button){
	if(pointer.first != -1 && listener != nullptr){
		listener->pressPanel(pointer.first, pointer.second, button);
	}
}

// Releases with the active pointer and the given button
// May trigger PanelListener release events on the thread that calls this
void PanelPlugin::ReleasePointer(int button){
	if (pointer.first != -1 && listener != nullptr) {
		listener->releasePanel(pointer.first, pointer.second, button);
	}
}

// Add a link to the navigastion graph for keyboard and controller menu navigation
void PanelPlugin::createNavLink(int from_panel_id, int from_element_id, const glm::vec2& direction, int to_panel_id, int to_element_id){
	std::pair<int,int> from = {from_panel_id, from_element_id} ;
	std::pair<int, int> to = { to_panel_id, to_element_id };
	nav_graph[from].emplace_back(from,to,glm::normalize(direction)) ;
}

// Remove all nav links going from and to the given indices
void PanelPlugin::removeNavLink(int from_panel_id, int from_element_id, const glm::vec2& direction, int to_panel_id, int to_element_id){
	std::pair<int, int> from = { from_panel_id, from_element_id };
	std::pair<int, int> to = { to_panel_id, to_element_id };
	std::vector<NavLink> new_links ;
	for(NavLink& link : nav_graph[from]){ // delete all matching
		if(link.to != to){
			new_links.push_back(link) ;
		}
	}
	nav_graph[from] = new_links ;
}

void PanelPlugin::clearNavfrom(int from_panel_id, int from_element_id){
	std::pair<int, int> from = { from_panel_id, from_element_id };
	if (nav_graph.find(from) != nav_graph.end() ) {
		nav_graph[from].clear();
	}

}

//Sets the current selection (may trigger panel listener enter and exit events on call)
void PanelPlugin::setNavSelection(int panel_id, int element_id){
	if(panel_id == nav_select.first && element_id == nav_select.second){
		return ; // we're selecting what is already selected, don't need to do anything
	}
	if(nav_select.first != -1 && listener != nullptr){
		listener->exitPanelElement(nav_select.first, nav_select.second) ;
	}
	nav_select.first = panel_id;
	nav_select.second = element_id;
	if(panel_id != -1 && listener != nullptr){
		listener->enterPanelElement(panel_id, element_id);
	}
}

std::pair<int, int> PanelPlugin::getNavSelection(){
	return nav_select ;
}

//Move the navigation selection through the graph (may trigger listener enter and exit events on call)
void PanelPlugin::moveNavSelection(const glm::vec2& direction){
	if(nav_graph.find(nav_select) == nav_graph.end() || nav_graph[nav_select].size() == 0){
		return ; // no links from current selection, cannot move
	}
	std::vector<NavLink>& links = nav_graph[nav_select] ;
	std::pair<int,int> best_location = links[0].to ;
	float best_alignment = glm::dot(direction,links[0].direction) ;
	for (int k=1; k < links.size(); k++){
		float alignment = glm::dot(direction, links[k].direction) ;
		if(alignment > best_alignment){
			best_location = links[k].to ;
			best_alignment = alignment ;
		}
	}
	setNavSelection(best_location.first, best_location.second) ;
}

// Presses a button on the current nav selection, triggering a panel press event if the selection is valid
void PanelPlugin::pressNav(int button){
	if (nav_select.first != -1 && listener != nullptr) {
		listener->pressPanel(nav_select.first, nav_select.second, button);
	}

}

//Releases a button on the current nav selection, triggering a release event o nthe panel listener if the selection is valid
void PanelPlugin::releaseNav(int button){
	if (nav_select.first != -1 && listener != nullptr) {
		listener->releasePanel(nav_select.first, nav_select.second, button);
	}
}


PanelPlugin::MenuButton::MenuButton(int panel_id, const std::string& text, const std::string& action, float x, float y, bool centered, const std::string& font) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	panel = panel_id;
	element = panels->createElement(panel_id);
	this->action = action;
	this->text = text;
	std::shared_ptr<WFImage> element_image = panels->createTextImage(text, font, text_color, 9999);
	panels->setElementTexture(panel_id, element, element_image);
	if (centered) {
		x -= element_image->getWidth() * 0.5f;
	}
	base_pose = PanelPlugin::getElementPose(glm::vec2(x, y), (float)element_image->getWidth(), (float)element_image->getHeight(), element);

	//base_pose *= glm::rotate(0.05f, glm::vec3(0, 0, 1));

	PanelPlugin::DefaultInstance inst;
	inst.pose = base_pose;
	inst.bg_color_1 = back_color_1;
	inst.bg_color_2 = back_color_2;
	inst.box_border_color = border_color;
	panels->addPanelElementKeyFrame(panel_id, element, panels->getTime(), inst);

	hover_pose = PanelPlugin::getElementPose(
		glm::vec2(x - 10, y - 10),
		(float)element_image->getWidth() + 20,
		(float)element_image->getHeight() + 20,
		element);

	hover_pose *= glm::rotate(glm::mat4(1),-0.05f, glm::vec3(0, 0, 1));
}


PanelPlugin::MenuButton::~MenuButton() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	if(panels!=nullptr){
		panels->deleteElement(panel, element);
	}
}

PanelPlugin::Label::Label(int panel_id, const std::string& text, float x, float y, bool centered, const std::string& font) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	panel = panel_id;
	element = panels->createElement(panel_id);
	this->x = x;
	this->y = y;
	this->centered = centered;
	this->font = font;

	setText(text);
}

PanelPlugin::Label::~Label() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	if(panels!=nullptr){
		panels->deleteElement(panel, element);
	}
}

void PanelPlugin::Label::setText(const std::string& new_text) {
	if (text != new_text) {
		if (new_text.length() == 0) {
			text = " "; // make a blank texture instead of no texture
		}
		else {
			text = new_text;
		}
		PanelPlugin* panels = getTool<PanelPlugin>();
		std::shared_ptr<WFImage> element_image = panels->createTextImage(text, font, text_color, 9999);
		panels->setElementTexture(panel, element, element_image);
		image_width = element_image->getWidth();
		image_height = element_image->getHeight();
		//printf("%d, %d\n", image_width, image_height) ;
		float ex = x;
		if (centered) {
			ex -= image_width * 0.5f;
		}
		pose = PanelPlugin::getElementPose(glm::vec2(ex, y), (float)image_width, (float)image_height, element);
		PanelPlugin::DefaultInstance inst;
		inst.bg_color_1 = back_color_1;
		inst.bg_color_2 = back_color_2;
		inst.box_border_color = border_color;
		inst.pose = pose;
		panels->clearKeyFrames(panel, element);
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
	}
}

void PanelPlugin::Label::setColors(const glm::vec4& bg_1, const glm::vec4& bg_2, const glm::vec4& text, const glm::vec4& border){
	back_color_1 = bg_1 ;
	back_color_2 = bg_2 ;
	border_color = border ;
	text_color = text ;
	//printf("Set colors of a label!\n");
	//Force text to regenerate
	std::string new_text = this->text ;
	this->text = "asdfdksoqwhamqp075nj" ;
	setText(new_text) ;
}

// interpolate to the given position over an amount of time
void PanelPlugin::Label::moveTo(float x, float y, bool centered, double time_to_move) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	glm::ivec2 dim = panels->getPanelElementTextureDimensions(panel, element);
	float ex = x;
	if (centered) {
		ex -= dim.x * 0.5f;
	}
	pose = PanelPlugin::getElementPose(glm::vec2(ex, y), (float)dim.x, (float)dim.y, element);
	PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, element);
	inst.bg_color_1 = back_color_1;
	inst.bg_color_2 = back_color_2;
	inst.box_border_color = border_color;
	panels->clearKeyFrames(panel, element); //delete existing move and start moving from current position

	if (fabs(glm::determinant(inst.pose)) > 0.001f) {
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
	}
	inst.pose = pose;
	panels->addPanelElementKeyFrame(panel, element, panels->getTime() + time_to_move, inst);
}


PanelPlugin::TextBox::TextBox(int panel_id, const std::string& label, const std::string& text, const std::string& name, float x, float y, const std::string& font, int max_length, bool numeric) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	panel = panel_id;
	this->text = text;
	this->name = name;
	this->label = label;
	this->x = x;
	this->y = y;
	this->font = font;
	this->max_text_length = max_length;
	this->numbers_only = numeric;
	//Make the label
	label_element = panels->createElement(panel_id);
	std::shared_ptr<WFImage> label_image = panels->createTextImage(label, font, glm::vec4(0.1, 0, 0.1, 1.0), 9999);
	panels->setElementTexture(panel_id, label_element, label_image);
	label_pose = PanelPlugin::getElementPose(glm::vec2(x - (float)label_image->getWidth(), y), (float)label_image->getWidth(), (float)label_image->getHeight(), label_element);
	PanelPlugin::DefaultInstance inst;
	inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
	inst.bg_color_2 = glm::vec4(0, 0, 0, 0);
	inst.alpha_border_width = 0;
	inst.box_border_width = 0;
	inst.pose = label_pose;
	panels->addPanelElementKeyFrame(panel_id, label_element, panels->getTime(), inst);

	// make the box
	box_element = panels->createElement(panel_id);
	std::string  display = "";
	while (display.length() < max_text_length*1.2) {
		display += "_"; 
	}
	std::shared_ptr<WFImage> box_image = panels->createTextImage(display, font, glm::vec4(1.0, 1.0, 1.0, 1.0), 9999);
	panels->setElementTexture(panel_id, box_element, box_image);
	box_pose = PanelPlugin::getElementPose(glm::vec2(x, y), (float)box_image->getWidth(), (float)box_image->getHeight(), box_element);
	inst.pose = box_pose;
	inst.bg_color_1 = glm::vec4(1, 1, 1, 1);
	inst.bg_color_2 = glm::vec4(1, 1, 1, 1);
	inst.box_border_color = glm::vec4(0, 0, 0, 1);
	inst.box_border_width = 2;
	inst.alpha_border_width = 0;
	inst.alpha_border_color = glm::vec4(1, 1, 1, 0);
	//panels->addPanelElementKeyFrame(panel_id, text_element, panels->getTime(), inst);
	panels->setPanelElementInstance(panel, box_element, inst);

	//make the text element
	text_element = panels->createElement(panel_id);
	display = text;
	if (display.length() < 1) {
		display += " ";
	}
	std::shared_ptr<WFImage> text_image = panels->createTextImage(display, font, glm::vec4(0.1, 0, 0.1, 1.0), 9999);
	panels->setElementTexture(panel_id, text_element, text_image);
	text_pose = PanelPlugin::getElementPose(glm::vec2(x, y), (float)text_image->getWidth(), (float)text_image->getHeight(), text_element);
	inst.pose = text_pose;
	inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
	inst.bg_color_2 = glm::vec4(0, 0, 0, 0);
	inst.alpha_border_width = 0;
	inst.box_border_width = 0;
	//panels->addPanelElementKeyFrame(panel_id, text_element, panels->getTime(), inst);
	panels->setPanelElementInstance(panel, text_element, inst);


	// make the cursor
	cursor_element = panels->createElement(panel_id);
	cursor_width = 4;
	cursor_height = text_image->getHeight() * 4 / 5;
	std::shared_ptr<WFImage> cursor_image = std::shared_ptr<WFImage>(new WFImage(2, cursor_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
	panels->setElementTexture(panel_id, cursor_element, cursor_image);// this image is blank
	cursor_x = 10;
	cursor_y = (int)(y + (text_image->getHeight() - cursor_height) * 0.5f);
	cursor_pose = PanelPlugin::getElementPose(glm::vec2(cursor_x, cursor_y), (float)cursor_width, (float)cursor_height, cursor_element);
	inst.pose = cursor_pose;
	inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
	inst.bg_color_2 = glm::vec4(0, 0, 0, 0);
	inst.box_border_color = glm::vec4(0, 0, 0, 0);
	inst.box_border_width = 0;
	//panels->addPanelElementKeyFrame(panel_id, text_element, panels->getTime(), inst);
	panels->setPanelElementInstance(panel, cursor_element, inst);


}
PanelPlugin::TextBox::~TextBox(){
PanelPlugin* panels = getTool<PanelPlugin>();
	if (panels != nullptr) {
		panels->deleteElement(panel, label_element);
		panels->deleteElement(panel, text_element);
		panels->deleteElement(panel, box_element);
		panels->deleteElement(panel, cursor_element);
	}
}

//Clickin in a text area, selects the box and places the cursor as close as possible in the text
void PanelPlugin::TextBox::select() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	// Find he cursor index closest to where the mouse is pointing
	cursor_index = (int)text.length();
	glm::vec2 panel_coords = panels->tracePanelTextureCoordinates(panel, window->window_target->camera_position, window->getMouseRay());
	cursor_x = (int)(panel_coords.x + panels->getTextLineDimensions("a", font).x * 0.5f); // add half a character so it probably goes to closest instead of always back
	while (x + panels->getTextLineDimensions(text.substr(0, cursor_index), font).x > cursor_x && cursor_index>0) {
		cursor_index--;
	}
	window->setTyped(cursor_index, text);
}

void PanelPlugin::TextBox::deselect() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	// hide the cursor
	PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance< PanelPlugin::DefaultInstance>(panel, cursor_element);
	inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
	inst.bg_color_2 = glm::vec4(0, 0, 0, 0);


	panels->setPanelElementInstance(panel, cursor_element, inst);
}

void PanelPlugin::TextBox::setText(const std::string& new_text){
	PanelPlugin* panels = getTool<PanelPlugin>();
	// text has changed and is within length max
	if (new_text != text && new_text.length() <= max_text_length && (!numbers_only || isInt(new_text) || new_text.length() == 0)) {
		text = new_text;
		std::string display = text;
		while (display.length() < max_text_length) {
			display += " ";
		}
		std::shared_ptr<WFImage> text_image = panels->createTextImage(display, font, glm::vec4(0.1, 0, 0.1, 1.0), 9999);
		panels->setElementTexture(panel, text_element, text_image);
		text_pose = PanelPlugin::getElementPose(glm::vec2(x, y), (float)text_image->getWidth(), (float)text_image->getHeight(), text_element);
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, text_element);
		inst.pose = text_pose;
		panels->setPanelElementInstance(panel, text_element, inst);
	}
}


//should be called regularly when the textbox is selected to blink thre cursor and pick up text events
void PanelPlugin::TextBox::update() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	std::pair<int, std::string> typed = window->getTyped();
	// text has changed and is within length max
	if (typed.second != text && typed.second.length() <= max_text_length && (!numbers_only || isInt(typed.second) || typed.second.length() == 0)) {
		text = typed.second;
		cursor_index = typed.first;
		std::string display = text;
		while (display.length() < max_text_length) {
			display += " ";
		}
		std::shared_ptr<WFImage> text_image = panels->createTextImage(display, font, glm::vec4(0.1, 0, 0.1, 1.0), 9999);
		panels->setElementTexture(panel, text_element, text_image);
		text_pose = PanelPlugin::getElementPose(glm::vec2(x, y), (float)text_image->getWidth(), (float)text_image->getHeight(), text_element);
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, text_element);
		inst.pose = text_pose;
		panels->setPanelElementInstance(panel, text_element, inst);
	}
	else if (typed.second == text && typed.first != cursor_index) { // just the cursor has moved
		cursor_index = typed.first; // update it
	}
	else { //something may have happened but it wasn't a valid change
		window->setTyped(cursor_index, text); // set the window text back to our state
	}


	float s = (float)(panels->getTime() * cursor_blink_speed - (int)(panels->getTime() * cursor_blink_speed));
	PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance< PanelPlugin::DefaultInstance>(panel, cursor_element);
	if (s > 0.5f) {
		inst.bg_color_1 = glm::vec4(0, 0, 0, 1);
		inst.bg_color_2 = glm::vec4(0, 0, 0, 1);
	}
	else {
		inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
		inst.bg_color_2 = glm::vec4(0, 0, 0, 0);
	}

	cursor_x = (int)(x + panels->getTextLineDimensions(text.substr(0, cursor_index), font).x);
	cursor_pose = cursor_pose = PanelPlugin::getElementPose(glm::vec2(cursor_x, cursor_y), (float)cursor_width, (float)cursor_height, cursor_element);
	inst.pose = cursor_pose;

	panels->setPanelElementInstance(panel, cursor_element, inst);
}


//returns the string currently in the text box
std::string PanelPlugin::TextBox::getString() {
	return text;
}

// Returns the integer value if this is a numeric box
int PanelPlugin::TextBox::getInt() {
	if(text.length() == 0){
		return 0 ;
	}
	return std::stoi(text);
}