#include "Nameplate.h"

namespace Narball {

//create a nameplate for a player, makign a panle
Nameplate::Nameplate(std::string name, int p_id, const std::string& font) {
	text = name;
	player = p_id;
	PanelPlugin* panels = getTool<PanelPlugin>();
	std::shared_ptr<WFImage> element_image = panels->createTextImage(text, font, glm::vec4(text_color, 1), 9999);
	image_width = (float)element_image->getWidth();
	image_height = (float)element_image->getHeight();
	panel = panels->createPanel(element_image->getWidth(), element_image->getHeight(), glm::vec4(0, 0, 0, 0));
	element = panels->createElement(panel);
	panels->setElementTexture(panel, element, element_image);

	PanelPlugin::DefaultInstance inst;
	inst.bg_color_1 = glm::vec4(0, 0, 0, 0);
	inst.bg_color_2 = glm::vec4(0, 0, 0, 0);//initalize every color to transparent because we don't position it until first update
	inst.box_border_color = glm::vec4(0, 0, 0, 0);
	inst.alpha_border_color = glm::vec4(0, 0, 0, 0);
	inst.pose = PanelPlugin::getElementPose(glm::vec2(0, 0), (float)element_image->getWidth(), (float)element_image->getHeight(), element);
	panels->clearKeyFrames(panel, element);
	panels->setPanelElementInstance(panel, element, inst);


}

//clean pu the panel on delete
Nameplate::~Nameplate() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	if (panels != nullptr) {
		panels->deleteElement(panel, element);
		panels->deletePanel(panel);
	}
}

//update the visuals for the nameplate
void Nameplate::update(const glm::vec3 p, glm::vec3 camera_position, float alpha) {
	position = p + offset;
	PanelPlugin* panels = getTool<PanelPlugin>();
	PanelPlugin::DefaultInstance inst = panels->getPanelInstance<PanelPlugin::DefaultInstance>(panel);
	//inst.pose = panels->getPoseInFrontOfCamera(camera_position, position, glm::distance(camera_position, p) * 0.75f, size * image_width / image_height, size);


	glm::vec3 z = position - camera_position;
	glm::vec3 y = glm::vec3(0, 0, 1.0);
	glm::vec3 x = glm::cross(z, y);

	y = glm::cross(x, z);
	z = glm::normalize(z) * glm::distance(camera_position, p) * 0.5f;
	x = glm::normalize(x) * size * image_width / image_height;
	y = glm::normalize(y) * size;

	inst.pose = panels->getPose(camera_position + z - x * 0.5f - y * 0.5f, x, y);


	panels->setPanelInstance(panel, inst);
	//TODO alpha
}

void Nameplate::hide() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	PanelPlugin::DefaultInstance inst = panels->getPanelInstance<PanelPlugin::DefaultInstance>(panel);
	inst.pose = PanelPlugin::HIDDEN;
	panels->setPanelInstance(panel, inst);
}

}