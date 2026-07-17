#include "Glove.h"

#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "glm/glm.hpp"


namespace Chess {

	Glove::Glove(const glm::vec3& p, const std::string& model_name_set, const bool& is_white_arg) {
		position = p;
		model_name = model_name_set;
		is_white = is_white_arg;
	}

	void Glove::setPosition(const glm::vec3& p) {
		position = p;
	}

	void Glove::destroy() {
		destroyed = true;
	}

	void Glove::print() const {
		printf("Glove at %f, %f, %f\n", position.x, position.y, position.z);
	}

	void GloveView::created(std::shared_ptr<const Glove>& observation) {
		ScenePlugin* scene = getTool<ScenePlugin>();

		id = observation->id;
		pose = glm::mat4(1.0f);
		pose = glm::translate(pose, observation->position);
		scene_id = scene->createInstance(observation->model_name, pose);
	}

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void GloveView::updated(std::shared_ptr<const Glove>& observation) {
		ScenePlugin* scene = getTool<ScenePlugin>();
		pose = glm::mat4(1.0f);
		pose = glm::translate(pose, observation->position);
		if (!observation->is_white) {
			pose = glm::rotate(pose, 3.14f, glm::vec3(0, 1, 0));
		}
		scene->setPose(scene_id, pose);
	}

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void GloveView::destroyed() {
		//printf("Glove view destroyed: %ld\n", (long)id);
		ScenePlugin* scene = getTool<ScenePlugin>();
		scene->deleteInstance(scene_id);
	}

}