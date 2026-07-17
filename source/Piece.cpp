#include "Piece.h"

#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "ChessApp.h"
#include <print>
#include <format>

namespace Chess {

void Piece::setPosition(const glm::vec3& p) {
	position = p;
	has_moved = true;
}

bool Piece::isValidMove(const glm::vec3& destination) const {
	// Destination is on the board
	return fabs(destination.x) <= 4 && fabs(destination.z) <= 4 && destination != position;
}

void Piece::destroy() {
	destroyed = true;
}

// Destination angle must be a multiple of 45 degrees (0,45,90,135...)
std::vector<glm::vec3> Piece::squaresBetween(const glm::vec3& destination) const {
	std::vector<glm::vec3> squares;
	float from_x = position.x;
	float from_z = position.z;

	while (true) {
		if (from_x < destination.x) { from_x++; }
		if (from_x > destination.x) { from_x--; }
		if (from_z < destination.z) { from_z++; }
		if (from_z > destination.z) { from_z--; }

		if (from_x == destination.x && from_z == destination.z) return squares;
		if (fabs(from_x) > 3.5 || fabs(from_z) > 3.5) return squares;

		std::println("Checking there's no piece on {}x,{}z", from_x, from_z);

		squares.emplace_back(glm::vec3(from_x, 0, from_z));
	}
}

// Destination angle must be a multiple of 45 degrees (0,45,90,135...)
int64_t Piece::blocked_by(const glm::vec3& destination) const {
	WorldPlugin* world = getTool<WorldPlugin>();
	std::shared_ptr<const Board> board = world->observe<Board>("chess", board_id);

	for (const auto& square : squaresBetween(destination)) {
		auto maybe_piece = board->board_of_pieces.find(square);
		if (maybe_piece != board->board_of_pieces.end()) return maybe_piece->second;
	}

	return false;
}

int64_t Piece::piece_at(const glm::vec3& destination) const {
	WorldPlugin* world = getTool<WorldPlugin>();
	std::shared_ptr<const Board> board = world->observe<Board>("chess", board_id);

	auto maybe_piece = board->board_of_pieces.find(destination);
	if (maybe_piece != board->board_of_pieces.end()) return maybe_piece->second;

	return false;
}

bool Piece::moved_like_rook(const glm::vec3& destination) const {
	return position.x == destination.x || position.z == destination.z;
}

bool Piece::moved_like_bishop(const glm::vec3& destination) const {
	return fabs(position.x - destination.x) - fabs(position.z - destination.z) == 0;
}

// This is mandated by the engine
void PieceView::created(std::shared_ptr<const Piece>& observation) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();

	last_observation = observation;

	pose = glm::mat4(1.0f);
	pose = glm::translate(pose, observation->position);
	scene_id = scene->createInstance(observation->model_name, pose);
	//traceables[scene_id] = observation.id;
	
	std::shared_ptr<GLTF> model = scene->getModelController(observation->model_name);
	//Note: multiplying pose by AABB corners only works to prdouce another correct AABB here when pose contains only translation and scale
	std::shared_ptr<ActionTrigger> trigger = std::shared_ptr<ActionTrigger>(new ActionTrigger(0, pose * glm::vec4(model->min, 1), pose * glm::vec4(model->max, 1), this));
	trigger_id = action_map->addTrigger(trigger);
}

// This is mandated by the engine
//Update is called when an observation is made of an object that was also observed last frame on this same view
void PieceView::updated(std::shared_ptr<const Piece>& observation) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();

	last_observation = observation;

	pose = glm::mat4(1.0f);
	pose = glm::translate(pose, observation->position);
	scene->setPose(scene_id, pose);

	std::shared_ptr<GLTF> model = scene->getModelController(observation->model_name);
	action_map->moveTrigger(trigger_id, pose * glm::vec4(model->min, 1), pose * glm::vec4(model->max, 1));

}

// This is mandated by the engine
//Destroyed is called when an observation that was present in the last observation is no longer observed
//This view will be deleted immediately after this call (it's destructor will be called after this)
void PieceView::destroyed() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();
	scene->deleteInstance(scene_id);
	action_map->removeTrigger(trigger_id);
}

void PieceView::receiveAction(std::shared_ptr<ChessMouseAction>& action, std::shared_ptr<ActionTrigger>& trigger) {
	if (action->next_held_piece == -1 && action->clicked) { // piece not currently held and click is pressed
		float t = ChessApp::raytrace(action->origin, action->direction,scene_id,pose) ;
		if(t > 0){// Only act if the actual model was hit and not just he bounding box
			// recieveAction is called in ray order, so setting this will stop others from getting into the first if
			action->next_held_piece = last_observation->id ;
		}
	}
}

}