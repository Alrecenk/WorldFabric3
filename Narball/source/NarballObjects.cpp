#include "NarballObjects.h"
#include "Ball.h"

#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "glm/glm.hpp"

namespace Narball{


Cell::Cell(glm::vec3 mn, glm::vec3 mx) {
	position = (mn + mx) * 0.5f;
}

//Add or remove a reference to an object in this cell
void Cell::add(const int64_t& ball_id) {
	contents.insert(ball_id);
}
void Cell::remove(const int64_t& ball_id) {
	contents.erase(ball_id);
}

void Cell::destroy() {
	destroyBalls();
	destroyed = true;
}

void Cell::destroyBalls(){
	double offset = (id % 10000) / 1000000.0; // random offset per cell prevents time collision for perfectly aligned entities
	for (int64_t content_id : contents) {
		std::shared_ptr<const Ball> ball = dynamic_pointer_cast<const Ball>(read(content_id));
		if (ball) {
			queue(content_id, time + offset, &Ball::destroy);
		}
	}
}

void Cell::print() const {
	printf("MatchCell: %f, %f, %f\n", position.x, position.y, position.z);
}

Match::Match(int64_t lob_id, glm::vec3 mn, glm::vec3 mx, int w, int h, float interval, double start, float ball_size) {
	lobby_id = lob_id ;
	min = mn;
	max = mx;
	width = w;
	height = h;
	position = (min + max) * 0.5f;
	tick_interval = interval ;
	start_time = start ;
	ball_radius = ball_size ;
}

// Build the grid
void Match::createMatch() {
	int k = 0;
	for (int x = 0; x < width; x++) {
		float minx = min.x + (max.x - min.x) * x / (float)width;
		float maxx = min.x + (max.x - min.x) * (x + 1) / (float)width;
		std::vector<int64_t> row;
		for (int z = 0; z < height; z++) {
			float minz = min.z + (max.z - min.z) * z / (float)height;
			float maxz = min.z + (max.z - min.z) * (z + 1) / (float)height;
			glm::vec3 cellmin = glm::vec3(minx, min.y, minz);
			glm::vec3 cellmax = glm::vec3(maxx, max.y, maxz);
			auto cell = std::make_shared<Cell>(cellmin, cellmax);
			int64_t cell_id = create(cell, time);
			row.push_back(cell_id);
		}
		grid.push_back(row);
	}
	ready = true;
	//printf("new grid created %lld  at %lf\n", id, time); 
	queue(id, time + 0.5, &Match::update);
	
}

void Match::scorePoints(const int& left, const int& right) {
	left_score += left;
	right_score += right;
}


void Match::update(){
	std::shared_ptr<const Lobby> lobby = dynamic_pointer_cast<const Lobby>(read(lobby_id));
	
	if(lobby->state == Lobby::STARTED && (left_score >= lobby->match_points || right_score >= lobby->match_points)){ //Ending score reached
		//Delete all the balls 
		for (auto& row : grid) {
			for (auto& cell_id : row) {
				queue(cell_id, time, &Cell::destroyBalls);
			}
		}
		//Switch the lobby to game ending state
		std::string result;
		if (left_score == right_score) {
			result = "Tie!";
		}else if (left_score > right_score) {
			result = "Red Wins!";
		}else {
			result = "Blue Wins!";
		}
		queue(lobby_id, time, &Lobby::setResult, result);

		queue(id, time + MATCH_UPDATE_TICK_INTERVAL, &Match::update); // need to keep updatng to catch ending timeout
	}else if (lobby->state == Lobby::ENDING &&  time - lobby->last_state_change_time > Lobby::TIME_AT_END_SCREEN){ // ending screen timing out
		closeMatch(); // close the match and go back to the lobby
	}else{
		queue(id, time + MATCH_UPDATE_TICK_INTERVAL, &Match::update);
	}
	
}



void Match::closeMatch(){
	
	queue(lobby_id, time, &Lobby::setState, Lobby::OPEN);

	//Destroy the grid
	for (auto& row : grid) {
		for (auto& cell_id : row) {
			queue(cell_id, time, &Cell::destroy);
		}
	}
	destroyed = true;
}

//Get the ids of the cells a ball intersects (not an event, the ball update event can call this bceause it is const)
std::vector<int64_t> Match::getCells(const glm::vec3& center, float radius) const {
	std::vector<int64_t> v;
	std::pair<int, int> min_p = getMatchCoord(glm::vec3(center.x - radius, center.y, center.z - radius));
	std::pair<int, int> max_p = getMatchCoord(glm::vec3(center.x + radius, center.y, center.z + radius));
	for (int x = min_p.first; x <= max_p.first; x++) {
		for (int z = min_p.second; z <= max_p.second; z++) {
			v.push_back(grid[x][z]);
		}
	}
	return v;
}

//returns the id of the cell a point is in
int64_t Match::getCell(const glm::vec3& p) const {
	int x = (int)((p.x - min.x) * width / (max.x - min.x));
	if (x < 0) x = 0;
	if (x > width - 1) x = width - 1;
	int z = (int)((p.z - min.z) * height / (max.z - min.z));
	if (z < 0) z = 0;
	if (z > height - 1) z = height - 1;
	return grid[x][z];
}

std::pair<int, int> Match::getMatchCoord(const glm::vec3& p) const {
	int x = (int)((p.x - min.x) * width / (max.x - min.x));
	if (x < 0) x = 0;
	if (x > width - 1) x = width - 1;
	int z = (int)((p.z - min.z) * height / (max.z - min.z));
	if (z < 0) z = 0;
	if (z > height - 1) z = height - 1;
	return { x,z };

}

void Match::print() const {
	printf("Match: min = %f,%f,%f  max= %f,%f,%f\n", min.x, min.y, min.z, max.x, max.y, max.z);
}



Lobby::Lobby(int num_balls, int num_points){
	balls = num_balls;
	match_points = num_points ;
}

void Lobby::setMatchParameters(const int& num_balls, const int& num_points){
	balls = num_balls;
	match_points = num_points ;
}

void Lobby::addPlayer(const int& player_id, const std::string& name){
	printf("%s joined.\n", name.c_str()) ;
	players[player_id].number = player_id;
	if(name.length() > 0 && name.length() < 30){
		players[player_id].name = name;
	}
	players[player_id].team = false;
	players[player_id].last_keep_alive = time;

	//start the player on the least populated team
	int num_true=0 ;
	for (auto& [ pid, player] : players){
		if(player.team){
			num_true++;
		}
	}
	players[player_id].team = num_true < players.size()-num_true ;
}

void Lobby::removePlayer(const int& player_id){
	printf("%s left.\n", players[player_id].name.c_str());
	players.erase(player_id);
}

void Lobby::setTeam(const int& player_id, const bool& team){
	players[player_id].team = team ;
}

void Lobby::rewardPoint(const int& player_id) {
	if (players.find(player_id) != players.end()) {
		players[player_id].score++ ;
	}
}

void Lobby::switchTeam(const int& player_id){
	players[player_id].team = !players[player_id].team ;
}

void Lobby::setState(const int& new_state){
	state = new_state ;
	last_state_change_time = time ;

	//Reset score when going back to lobby
	if(state == OPEN){
		for (auto& [player_id, player] : players) {
			player.score = 0 ;
		}
	}
		
}

void Lobby::setResult(const std::string& new_result) {
	//printf("lobby result set\n");
	result = new_result ;
	last_state_change_time = time;
	state = ENDING;
}

void Lobby::setTimeLobbyStarted(const double& new_time_lobby_started) {
	time_lobby_started = new_time_lobby_started;
}

void Lobby::setMinTimeInLobby(const float& new_min_time_in_lobby) {
	min_time_in_lobby = new_min_time_in_lobby;
}

void Lobby::keepAlive(const int& player_id){
	if(players.find(player_id) != players.end()){
		players[player_id].last_keep_alive = time ;
	}
}

void Lobby::kickDisconnected(){
	for(auto& [ player_id, player] : players){
		//printf("Player age:%d, %lf\n", player_id, time - player.last_keep_alive) ;
		if(time - player.last_keep_alive > kick_interval){
			removePlayer(player_id) ; // events can directly call event function on the same object safely
		}
	}
}

void Lobby::print() const {
	printf("Lobby %I64d  state:%d  \n", id, state);
}


void updateDebugPanel() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();

	int panel_width = 740 ;
	int panel_height = 130 ;
	
	if(!debug_panel_enabled){
		if(debug_panel != -1){
			debug_label.reset() ;
			panels->deletePanel(debug_panel) ;
			debug_panel = -1 ;
		}
		return ;
	}

	if(debug_panel == -1){
		debug_panel = panels->createPanel(panel_width, panel_height, { 0.0,0.0,0.0,0.0 });
		debug_label = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(debug_panel, "Debug Info", panel_width * 0.5f, 10, true, "arial50"));
		debug_label->setColors(glm::vec4(0), glm::vec4(0), glm::vec4(0,0,0,1), glm::vec4(0)) ;
		
	}else{

		total_dt+= worlds->getTimeStep();
		debug_frames++;
		if(total_dt > 1.0){
			debug_fps = (int)(debug_frames/total_dt) ;
			debug_frames = 0 ;
			total_dt = 0 ;

			std::string d = NARBALL_VERSION + " " +  concat("fps:", debug_fps) ;
			if(worlds->getPing() != 0 ){
				d+= concat(" ping:", (int)(1000 * worlds->getPing()));
			}
			//printf("%s\n", d.c_str()) ;
			debug_label->setText(d);
	
		}
	}

	int screen_x = window->window_target->width / 10;
	int screen_y = screen_x * panel_height/panel_width ;
	float vx = (2.0f * screen_x) / window->window_target->width - 1.0f;
	float vy = (2.0f * screen_y) / window->window_target->height - 1.0f;
	glm::vec4 clip_point(vx, vy, 1.0, 1.0f);
	glm::mat4 camera_inverse = glm::inverse(window->window_target->camera_matrix);
	glm::vec4 world_point = camera_inverse * clip_point;
	glm::vec3 unprojected = world_point / world_point.w;
	glm::vec3 screen_ray = glm::normalize(unprojected - window->window_target->camera_position);

	PanelPlugin::DefaultInstance inst = panels->getPanelInstance<PanelPlugin::DefaultInstance>(debug_panel);
	//inst.pose = panels->getPoseInFrontOfCamera(camera_position, position, glm::distance(camera_position, p) * 0.75f, size * image_width / image_height, size);

	glm::vec3 z = screen_ray;
	glm::vec3 x = glm::vec3(window->window_target->camera_matrix[0].x,0,0);


	float debug_depth = 5.0f ;
	float size = 0.07f ;
	glm::vec3 y = glm::cross(x, z);
	z = glm::normalize(z) * debug_depth;
	x = glm::normalize(x) * size* debug_label->image_width / debug_label->image_height;
	y = glm::normalize(y) *size;

	inst.pose = panels->getPose(window->window_target->camera_position + z - x * 0.5f - y * 0.5f, x, y);


	panels->setPanelInstance(debug_panel, inst);

	
}

}// end narball namespace