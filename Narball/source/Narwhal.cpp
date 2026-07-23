#include "Narwhal.h"

#include "NarballObjects.h"
#include "Ball.h"
#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "ParticlePlugin.h"
#include "glm/glm.hpp"


namespace Narball {


Narwhal::Narwhal(const glm::vec3& p, int64_t g, int player) {
	position = p;
	velocity = glm::vec3(0, 0, 0);
	match_id = g;
	player_id = player;

}

void Narwhal::update() {

	if (last_control_time > 0 && time - last_control_time > kick_interval) {
		destroyed = true;
		return;
	}

	std::shared_ptr<const Match> grid = dynamic_pointer_cast<const Match>(read(match_id));
	if (grid == nullptr) { //if there is no field, then the match is over or an error occured
		destroyed = true; // delete this object
		//printf("narhwal destroyed because no grid %lld! at %lf\n", match_id, time);
		return;
	}

	// Compute turn from analog stick
	angular_velocity = 0;
	if (glm::dot(right_stick, right_stick) > 0.2) {
		float right_stick_angle = atan2(right_stick.y, right_stick.x);
		float delta_angle = right_stick_angle - facing_angle;
		if (delta_angle > 3.141f) {
			delta_angle -= 6.282f;
		}
		else if (delta_angle < -3.141f) {
			delta_angle += 6.282f;
		}

		if (delta_angle > 0.0001f) {
			angular_velocity = max_turn_speed;
		}
		else if (delta_angle < -0.0001f) {
			angular_velocity = -max_turn_speed;
		}
		float turn = angular_velocity * grid->tick_interval;
		if (fabs(turn) > fabs(delta_angle)) {
			facing_angle = right_stick_angle;
			angular_velocity = 0;
		}
		else {
			facing_angle += turn;
		}
	}

	queue(id, time + grid->tick_interval, &Narwhal::update); //Queue up the update to run again at a fixed tick rate
	if (!grid->ready || time < grid->start_time) { // if match hasn't started yet
		return; // exit without allowing movement besides turning
	}

	//Update velocity and position based on movement
	glm::vec2 facing_vector = glm::vec2(cos(facing_angle), sin(facing_angle));
	glm::vec2 acceleration = left_stick * (base_acceleration + facing_acceleration * fmax(0.0f, glm::dot(left_stick, facing_vector)));
	velocity += glm::vec3(acceleration.x, 0, acceleration.y) * grid->tick_interval;

	float speed = glm::length(velocity);
	if (speed > max_speed) {
		velocity *= max_speed / speed;
	}
	else if (speed < drag * grid->tick_interval) { // drag tor motionless
		velocity = glm::vec3(0, 0, 0);
	}
	else {
		velocity *= (speed - drag * grid->tick_interval) / speed; // apply frag
	}
	position += velocity * grid->tick_interval;


	bool hit_wall = false;
	glm::vec3 wall_hit_position;
	//Bounce inside the field
	if (position.x + body_radius > grid->max.x) {
		position.x = grid->max.x - body_radius;
		velocity.x *= -wall_bounce;
		if (fabs(velocity.x) > speed_for_sound) {
			sound = WALL_HIT_SOUND;
			sound_num++;
		}
		hit_wall = true;
		wall_hit_position = glm::vec3(grid->max.x, position.y, position.z);
	}
	else if (position.x - body_radius < grid->min.x) {
		position.x = grid->min.x + body_radius;
		velocity.x *= -wall_bounce;
		if (fabs(velocity.x) > speed_for_sound) {
			sound = WALL_HIT_SOUND;
			sound_num++;
		}
		hit_wall = true;
		wall_hit_position = glm::vec3(grid->min.x, position.y, position.z);
	}
	if (position.z + body_radius > grid->max.z) {
		position.z = grid->max.z - body_radius;
		velocity.z *= -wall_bounce;
		if (fabs(velocity.z) > speed_for_sound) {
			sound = WALL_HIT_SOUND;
			sound_num++;
		}
		hit_wall = true;
		wall_hit_position = glm::vec3(position.x, position.y, grid->max.z);
	}
	else if (position.z - body_radius < grid->min.z) {
		position.z = grid->min.z + body_radius;
		velocity.z *= -wall_bounce;
		if (fabs(velocity.z) > speed_for_sound) {
			sound = WALL_HIT_SOUND;
			sound_num++;
		}
		hit_wall = true;
		wall_hit_position = glm::vec3(position.x, position.y, grid->min.z);
	}

	if (hit_wall) {
		glm::vec3 from_wall = glm::normalize(position - wall_hit_position);
		float tail_push_alignment = facing_vector.x * from_wall.x + facing_vector.y * from_wall.z;// dot product but facing vector is in 2D so it's a bit weird
		if (tail_push_alignment > min_alignment_for_tail_push) {
			velocity += glm::vec3(facing_vector.x, 0, facing_vector.y) * wall_tail_push_strength;
		}
	}

	//Update the Cells the narwhal is in so other nearby objects can find it for collisions
	std::vector<int64_t> new_cells = grid->getCells(position, horn_length); // need to be in every cell our horn might touch
	for (auto& new_cell : new_cells) {
		bool found = false;
		for (auto& old_cell : cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // new cell we didn't have before
			queue(new_cell, time, &Cell::add, id); // add us to it
		}
	}
	for (auto& old_cell : cells) {
		bool found = false;
		for (auto& new_cell : new_cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // old cell we aren't in anymore
			queue(old_cell, time, &Cell::remove, id); // remove us from it
		}
	}
	cells = new_cells;

	//vector from body to tip of horn
	glm::vec3 horn_vector = glm::vec3(facing_vector.x * horn_length, 0, facing_vector.y * horn_length);
	bool hit_narwhal = false;
	//handle any colliding objects
	std::unordered_set<int64_t> checked; // don't check the same object twice if it shares multiple cells
	checked.insert(id); // prevents colliding with self
	for (auto& cell_id : cells) {
		std::shared_ptr<const Cell> cell = dynamic_pointer_cast<const Cell>(read(cell_id));
		if (cell) {
			for (auto& other_id : cell->contents) {
				if (checked.contains(other_id)) {
					continue;
				}
				checked.insert(other_id);
				std::shared_ptr<const Ball> ball = dynamic_pointer_cast<const Ball>(read(other_id));
				if (ball) { // if it's a ball
					glm::vec3 ball_position = ball->position + ball->velocity * (time - ball->time);
					//First check if it hits the horn
					// vector project to get nearest point in horn line to ball
					float s = glm::dot(horn_vector, ball_position - position) / (horn_length * horn_length);
					// clamp to segment
					if (s < 0) {
						s = 0;
					}
					else if (s > 1.0f) {
						s = 1.0f;
					}
					glm::vec3 hit_point = position + horn_vector * s;
					bool ball_hit_horn = false;
					if (glm::distance(hit_point, ball_position) < grid->ball_radius) { // closest point touches ball
						glm::vec3 to_hit = hit_point - position;
						glm::vec3 perp = glm::vec3(-to_hit.z, 0, to_hit.x); // vector perpendicular to horn
						glm::vec3 hit_velocity = velocity + perp * angular_velocity;
						glm::vec3 to_other = ball->position - hit_point;
						float hit_axis_speed = glm::dot(hit_velocity - ball->velocity, to_other);
						if (hit_axis_speed > 0) {// if moving toward collision, not away
							ball_hit_horn = true;
							glm::vec3 impulse = to_other * (ball_horn_hit_force * hit_axis_speed / glm::dot(to_other, to_other));
							queue(other_id, time, &Ball::narwhalHit, impulse, hit_point, player_id, color == 1);
							if (!hit_narwhal) { // don't overwrite narwhal hit sound with ball sound
								if (hit_axis_speed > speed_for_sound * 0.5f) { // only make sound if hitting hard enough
									sound = BALL_HIT_SOUND;
									sound_num++;
								}
							}
						}
					}
					//Then check if it hit the body
					if (!ball_hit_horn) { // don't do both hits on the same frame
						glm::vec3 to_other = ball_position - position;
						float distance = glm::length(to_other);
						if (distance < grid->ball_radius + body_radius && glm::dot(velocity - ball->velocity, to_other) > 0) { // if in range and moving toward each other
							//difference in velocity projected onto collision axis
							glm::vec3 impulse = to_other * (glm::dot(velocity - ball->velocity, to_other) / glm::dot(to_other, to_other));
							impulse *= ball_body_hit_force;
							glm::vec3 hit_point = (position * grid->ball_radius + ball_position * body_radius) / (grid->ball_radius + body_radius); // interp point between circles
							queue(other_id, time, &Ball::narwhalHit, impulse, hit_point, player_id, color == 1);
							if (glm::dot(velocity - ball->velocity, velocity - ball->velocity) > speed_for_sound * speed_for_sound) {// only make sound if moving fast enough
								sound = BALL_BALL_SOUND;
								sound_num++;
							}
						}
					}
				}

				std::shared_ptr<const Narwhal> other = dynamic_pointer_cast<const Narwhal>(read(other_id));
				if (other && other_id > id) { // if it's another narwhal and they won't run this code on us do regulare collision
					glm::vec3 to_other = other->position - position;
					float distance = glm::length(to_other);
					//First check if body bumps a ball
					if (distance < body_radius * 2.0f && glm::dot(velocity - other->velocity, to_other) > 0) { // if in range and moving toward each other
						//difference in velocity projected onto collision axis
						glm::vec3 impulse = to_other * (glm::dot(velocity - other->velocity, to_other) / glm::dot(to_other, to_other));
						impulse *= narwhal_body_hit_force;
						glm::vec3 hit_point = (position + other->position) * 0.5f;
						queue(other_id, time, &Narwhal::applyImpulse, impulse, hit_point);//impulse on other
						applyImpulse(-impulse, hit_point);// negative impulse on self
						if (glm::dot(velocity - other->velocity, velocity - other->velocity) > speed_for_sound * speed_for_sound) {
							sound = NARWHAL_HIT_SOUND;
							hit_narwhal = true;
							sound_num++;
						}
					}
				}
			}
		}
	}

}
void Narwhal::setVelocity(const glm::vec3& v) {
	velocity = v;
}
void Narwhal::setPosition(const glm::vec3& p) {
	position = p;
}

void Narwhal::setControls(const glm::vec2& left, const glm::vec2& right, const int& sdl_input_num) {
	left_stick = left;
	right_stick = right;
	last_control_time = time;
	input_num = sdl_input_num ;

	//AsyncPlugin::inputDisplay(input_num, 2, false);
	
}

void Narwhal::changeColor() {
	color = (color + 1) % 3;
}

void Narwhal::applyImpulse(const glm::vec3& impulse, const glm::vec3& hit_point) {
	velocity += impulse;
}

void Narwhal::print() const {
	printf("Narwhal at %f, %f, %f  v = %f, %f, %f\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z);
}



// Gets a ball view that enforces maximum speeds after roll back
Narwhal NarwhalView::getView(const Narwhal& observed) {

	WorldPlugin* worlds = getTool<WorldPlugin>();
	double observation_time = worlds->getWorldTime(NARBALL, observed.position);
	float observation_age = (float)(observation_time - observed.time);// time difference between what we're supposed to see and what we observed
	float observed_facing_angle = observed.facing_angle + observed.angular_velocity * observation_age;// extrapolate based on age
	glm::vec3 observed_position = observed.position + observed.velocity * observation_age;


	if (!fancy_interpolation) {
		last_view = observed;
		Narwhal view = observed;
		view.position = observed_position;
		view.facing_angle = observed_facing_angle;
		view.time = observation_time;
		return view;
	} else {
		double dt = observed.time - last_view.time;

		if (dt <= 0) { // sometimes the clock moves to stay in sync with the server and this can happen
			last_view = observed;
			Narwhal view = observed;
			view.position = observed_position;
			view.facing_angle = observed_facing_angle;
			view.time = observation_time;
			return view;
		}

		glm::vec3 view_position;
		float view_facing_angle = 0;
		glm::vec3 last_position = last_view.position + observed.velocity * last_age;
		float last_facing_angle = (float)(last_view.facing_angle + observed.angular_velocity * last_age);

		float delta_angle = observed_facing_angle - last_facing_angle;
		if (delta_angle > 3.141f) {
			delta_angle -= 6.282f;
		}
		else if (delta_angle < -3.141f) {
			delta_angle += 6.282f;
		}

		float max_angle_change = (float)(Narwhal::max_turn_speed * dt * view_fudge_factor);
		if (fabs(delta_angle) <= max_angle_change) {
			view_facing_angle = observed_facing_angle;
		}
		else if (delta_angle > max_angle_change) {
			view_facing_angle = last_facing_angle + max_angle_change;
		}
		else {
			view_facing_angle = last_facing_angle - max_angle_change;
		}

		float max_distance_move = (float)(dt * Narwhal::max_speed * view_fudge_factor);
		glm::vec3 to_move = observed_position - last_position;
		float move_amount = glm::length(to_move);
		if (move_amount <= max_distance_move) {
			view_position = observed_position;
		}
		else {
			view_position = last_position + to_move * max_distance_move / move_amount;
		}

		last_view = observed;
		last_age= observation_age;
		Narwhal view = observed;
		view.position = view_position;
		view.facing_angle = view_facing_angle;
		view.time = observation_time;
		return view;
	}

}


glm::mat4 NarwhalView::computePose(const Narwhal& narwhal) {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	double vantage_time = worlds->getWorldTime(NARBALL);
	glm::mat4 narwhal_pose = glm::mat4(1.0f);

	glm::vec3 position = narwhal.position;
	position.y = waterHeight(position.x, position.z, (float)vantage_time, water_flow) + (float)(bob_magnitude * sin(narwhal.id % 1000 + vantage_time * bob_rate) + narwhal_lift); // move up and down with the water

	narwhal_pose = glm::translate(narwhal_pose, position);//interpolate between moves
	narwhal_pose = glm::rotate(narwhal_pose, narwhal.facing_angle, glm::vec3(0, -1, 0));
	narwhal_pose = glm::rotate(narwhal_pose, narwhal_tilt, glm::vec3(0, 0, 1)); // tilt the narwhal so it's facing up out of the water a bit
	return narwhal_pose ;
}


void NarwhalView::created(const Narwhal& observation) {
	id = observation.id;
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();

	Narwhal narwhal = getView(observation);
	std::string model = narwhal_model[narwhal.color];
	glm::mat4 narwhal_pose = computePose(observation);
	
	
	scene_id = scene->createInstance(model, narwhal_pose);
	last_sound_time = now();
	//object_to_scene[narwhal->id]
	scene->animateInstance(scene_id, tail_animation, true); // animation 0 
	scene->animateInstance(scene_id, left_fin_animation, true); // animation 1
	scene->animateInstance(scene_id, right_fin_animation, true); // animation 2

	if (butt_bone_id == -1) {
		std::shared_ptr<GLTF> gltf_model = scene->getModelController(model);
		butt_bone_id = gltf_model->getBoneIndex(butt_bone);
	}
	butt_base_rotation = scene->createPin(scene_id, butt_bone, butt_bone_id, glm::vec3(0, 0, 0), 0.0f, 1.0f); // create a rotation IK pin on the butt
	scene->enableIK(scene_id, true);

	
	std::shared_ptr<ActionTrigger> trigger = std::shared_ptr<ActionTrigger>(new ActionTrigger());
	trigger->action_receiver = this ; // Since we only catch a universal action,the trigger geometry doesn't matter
	action_trigger_id = action_map->addTrigger(trigger) ;
}

//Update is called when an observation is made of an object that was also observed last frame on this same view
void NarwhalView::updated(const Narwhal& observation) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	double vantage_time = worlds->getWorldTime(NARBALL);
	float dt = (float)(microsBetween(last_view_time, now()) * 0.000001);
	last_view_time = now();

	Narwhal narwhal = getView(observation);
	std::string model = narwhal_model[narwhal.color];
	glm::mat4 narwhal_pose = computePose(observation);
	

	scene->setPose(scene_id, narwhal_pose);//just set the new pose
	scene->setInstanceModel(scene_id, model);

	//Set the animation speed on the narwhal fins based on how it's moving
	glm::vec2 face_vector = glm::vec2(cos(narwhal.facing_angle), sin(narwhal.facing_angle));
	glm::vec2 right_vector = glm::vec2(face_vector.y, -face_vector.x);
	float forward_acceleration = glm::dot(narwhal.left_stick, face_vector);
	float side_acceleration = glm::dot(narwhal.left_stick, right_vector);
	float tail_speed = 0.5f + (forward_acceleration > 0 ? forward_acceleration * 2.5f : 0);

	float left_fin_speed = 0.5f;
	float right_fin_speed = 0.5f;

	if (side_acceleration < 0) {
		left_fin_speed += -3.0f * side_acceleration;
	}
	else {
		right_fin_speed += 3.0f * side_acceleration;
	}
	if (forward_acceleration < 0) {
		left_fin_speed -= 3.0f * forward_acceleration;
		right_fin_speed -= 3.0f * forward_acceleration;
		tail_speed -= 0.3f;
	}

	scene->setAnimationSpeed(scene_id, 0, tail_speed);
	scene->setAnimationSpeed(scene_id, 1, left_fin_speed);
	scene->setAnimationSpeed(scene_id, 2, right_fin_speed);
	
	if(num_highlight > 0){ // only measure input delay of the local narwhal and ignore others
		scene->setInputNum(scene_id, observation.input_num);
		//AsyncPlugin::inputDisplay(narwhal.input_num, 3, false);
	}


	//Adjust the butt angle of the narwhal based on how it is spinning
	if (narwhal.angular_velocity > 0.001f) {
		butt_angle += butt_spin_rate * dt;
	}
	else if (narwhal.angular_velocity < -0.001f) {
		butt_angle -= butt_spin_rate * dt;
	}
	else if (butt_angle > butt_spin_rate * dt * 0.5f) {
		butt_angle -= butt_spin_rate * dt * 0.5f;
	}
	else if (butt_angle < -butt_spin_rate * dt * 0.5f) {
		butt_angle += butt_spin_rate * dt * 0.5f;
	}
	else {
		butt_angle = 0;
	}
	butt_angle = fmin(max_butt_spin, fmax(butt_angle, -max_butt_spin));
	if (butt_angle == 0) {
		scene->enableIK(scene_id, false);
	}
	else {
		scene->enableIK(scene_id, true);
		glm::quat target_butt_rot = glm::angleAxis(butt_angle, glm::vec3(1, 0, 0));
		target_butt_rot = glm::quat_cast(narwhal_pose) * butt_base_rotation * target_butt_rot;
		scene->setPinTarget(scene_id, butt_bone, target_butt_rot);
	}

	//Play sounds
	if (narwhal.sound != -1) {
		if (last_sound_number != narwhal.sound_num && millisBetween(last_sound_time, now()) > 100) {
			float pitch = sound_pitch_range[narwhal.sound].first + randomFloat() * (sound_pitch_range[narwhal.sound].second - sound_pitch_range[narwhal.sound].first);
			sound->priorityPlay(narwhal.sound, narwhal.position, 0.5f, pitch, 2, 3);
			last_sound_time = now();
		}
	}
	last_sound_number = narwhal.sound_num;
	last_sound = narwhal.sound;


	//Updte the nameplate
	if(!nameplates_enabled && nameplate){
		nameplate->hide();
	}else if(nameplates_enabled){

		std::string player_name = "Nameless";
		bool found_name = false;
		if (lobby && lobby->players.find(narwhal.player_id) != lobby->players.end()) {
			player_name = lobby->players.at(narwhal.player_id).name;
			found_name = true;
		}

		if(found_name && (!nameplate || nameplate->text != player_name)){
			nameplate = std::shared_ptr<Nameplate>(new Nameplate(player_name, narwhal.player_id, "arial75"));
		}else if(nameplate){
			nameplate->update(narwhal.position, camera_position, 1.0f);
		}
		
	}

	//Highlight particles
	std::queue<HighlightParticle> continuing_particles;
	while (!highlight_particles.empty()) {
		HighlightParticle p = highlight_particles.front();
		highlight_particles.pop();
		if (p.update(particles, vantage_time, narwhal.position, highlight_spin_radius)) {
			continuing_particles.push(p);
		}
	}
	if (continuing_particles.size() == 0) { // start the particles staggered so they respawn continuously
		for (int k = 1; k < num_highlight; k++) {
			continuing_particles.emplace(vantage_time, vantage_time + (highlight_lifespan * k) / num_highlight, highlight_spin_speed, highlight_size, highlight_color);
		}
	} else if (continuing_particles.size() < num_highlight) {
		continuing_particles.emplace(vantage_time, vantage_time + highlight_lifespan, highlight_spin_speed, highlight_size, highlight_color);
	}

	highlight_particles = continuing_particles;

}

//Destroyed is called when an observation that was present in the last observation is no longer observed
//This view will be deleted immediately after this call (it's destructor will be called after this)
void NarwhalView::destroyed() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	ActionMap* action_map = getTool<ActionMap>();

	scene->deleteInstance(scene_id) ;
	action_map->removeTrigger(action_trigger_id) ;
	while (!highlight_particles.empty()) {
		HighlightParticle p = highlight_particles.front();
		highlight_particles.pop();
		p.update(particles, FLT_MAX, glm::vec3(), 0); //updating with far future time causes the particles to be deleted
	}
}

void NarwhalView::receiveAction(std::shared_ptr<NarwhalControlAction>& control, std::shared_ptr<ActionTrigger>& trigger){
	if(control->player_id == last_view.player_id){ // if I am owned by the player submitting the action
		WorldPlugin* worlds = getTool<WorldPlugin>();
		worlds->queue(NARBALL,id,&Narwhal::setControls,control->left_stick, control->right_stick,control->input_num) ;
	}
}

}