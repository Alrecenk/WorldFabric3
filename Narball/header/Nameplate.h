#ifndef _NAMEPLATE_H_
#define _NAMEPLATE_H_ 1


#include "PanelPlugin.h"
#include <string>

namespace Narball {

class Nameplate {
public:
	float size = 0.15f;
	std::string text;

	//create a nameplate for a player, makign a panle
	Nameplate(std::string name, int p_id, const std::string& font);

	//clean pu the panel on delete
	~Nameplate();

	//update the visuals for the nameplate
	void update(const glm::vec3 p, glm::vec3 camera_position, float alpha);

	void hide();

private:

	glm::vec3 position;
	glm::vec3 text_color = glm::vec3(0, 0, 0);
	glm::vec3 text_border_color = glm::vec3(1, 1, 1);
	int player = -1;
	int panel = -1;
	int element = -1;
	std::string font;

	float image_width = 0.0f;
	float image_height = 0.0f;
	glm::vec3 offset = glm::vec3(0, 0, 0.5f);

};

} // end Narball name space

#endif // #ifndef _NAMEPLATE_H_