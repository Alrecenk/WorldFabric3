#include "Ball.h"

#include "NarballObjects.h"
#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "glm/glm.hpp"


namespace Narball {

Ball::Ball(const glm::vec3& p, int64_t g) {
	position = p;
	velocity = glm::vec3(0, 0, 0);
	match_id = g;

}

void Ball::update() {

	std::shared_ptr<const Match> grid = dynamic_pointer_cast<const Match>(read(match_id));
	if (grid == nullptr) { //if there is no field, then the match is over or an error occured
		destroyed = true; // delete this object
		return;
	}

	if (!grid->ready) { // Don't start handling collisions until the field is ready
		queue(id, time + grid->tick_interval * 2, &Ball::update); //Queue up the update to run again at a fixed but slower rate
		return;
	}

	float speed = glm::length(velocity);
	if (speed > max_speed) {
		velocity *= max_speed / speed;
	}
	else if (speed < drag * grid->tick_interval) { // drag to motionless
		velocity = glm::vec3(0, 0, 0);
	}
	else {
		velocity *= (speed - drag * grid->tick_interval) / speed; // apply drag
	}

	position += velocity * grid->tick_interval;





	asleep = speed < 0.001f;
	bool scored = false;
	//Bounce inside the field
	if (position.x + grid->ball_radius > grid->max.x) {
		if (position.z - grid->ball_radius * 0.5f > -Match::goal_size && position.z + grid->ball_radius * 0.5f < Match::goal_size) {
			scored = true;
			queue(match_id, time, &Match::scorePoints, 0, 1); // add the match point
			queue(grid->lobby_id, time, &Lobby::rewardPoint, last_touch_blue); // add to the player score
		}
		else {
			position.x = grid->max.x - grid->ball_radius;
			velocity.x *= -wall_bounce;
			if (fabs(velocity.x) > speed_for_sound) {
				sound = BALL_WALL_SOUND;
				sound_num++;
			}
		}
	}
	else if (position.x - grid->ball_radius < grid->min.x) {
		if (position.z - grid->ball_radius * 0.5f > -Match::goal_size && position.z + grid->ball_radius * 0.5f < Match::goal_size) {
			scored = true;
			queue(match_id, time, &Match::scorePoints, 1, 0); // add the match point
			queue(grid->lobby_id, time, &Lobby::rewardPoint, last_touch_red);// add to the player score
		}
		else {
			position.x = grid->min.x + grid->ball_radius;
			velocity.x *= -wall_bounce;
			if (fabs(velocity.x) > speed_for_sound) {
				sound = BALL_WALL_SOUND;
				sound_num++;
			}
		}
	}
	if (position.z + grid->ball_radius > grid->max.z) {
		position.z = grid->max.z - grid->ball_radius;
		velocity.z *= -wall_bounce;
		if (fabs(velocity.z) > speed_for_sound) {
			sound = BALL_WALL_SOUND;
			sound_num++;
		}
	}
	else if (position.z - grid->ball_radius < grid->min.z) {
		position.z = grid->min.z + grid->ball_radius;
		velocity.z *= -wall_bounce;
		if (fabs(velocity.z) > speed_for_sound) {
			sound = BALL_WALL_SOUND;
			sound_num++;
		}
	}


	if (scored) { // Don't run collision if we scored because we can't read our new cells until we've moved there
		resetBall();
		queue(id, time + grid->tick_interval, &Ball::update); //Queue up the update to run again at a fixed tick rate
		return;
	}

	//Update the Cells the ball is in so other nearby objects can find it for collisions
	std::vector<int64_t> new_cells = grid->getCells(position, grid->ball_radius);
	for (auto& new_cell : new_cells) {
		bool found = false;
		for (auto& old_cell : cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // new cell we didn't have before
			queue(new_cell, time, &Cell::add, id); // add us to it
			asleep = false;
		}
	}
	for (auto& old_cell : cells) {
		bool found = false;
		for (auto& new_cell : new_cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // old cell we aren't in anymore
			queue(old_cell, time, &Cell::remove, id); // remove us from it
			asleep = false;
		}
	}
	cells = new_cells;


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
				if (ball && (other_id > id || ball->asleep)) { // if it's a ball andit wouldn't run this code on us
					glm::vec3 to_other = ball->position - position;
					float distance = glm::length(to_other);
					if (distance < grid->ball_radius * 2 && glm::dot(velocity - ball->velocity, to_other) > 0) { // if in range
						//difference in velocity projected onto collision axis
						glm::vec3 impulse = to_other * (glm::dot(velocity - ball->velocity, to_other) / glm::dot(to_other, to_other));
						impulse *= ball_ball_hit_force;
						glm::vec3 hit_point = (position + ball->position) * 0.5f; // interp point between circles
						queue(other_id, time, &Ball::applyImpulse, impulse, hit_point);
						asleep = false; // need to wake up before impulse so it doesn't spawn a new update thread
						applyImpulse(-impulse, hit_point);// negative impulse on self

						if (glm::dot(velocity - ball->velocity, velocity - ball->velocity) > speed_for_sound * speed_for_sound) {
							sound = BALL_BALL_SOUND;
							sound_num++;
						}
					}
				}
			}
		}
	}

	if (!asleep) {
		queue(id, time + grid->tick_interval, &Ball::update); //Queue up the update to run again at a fixed tick rate
	}
}
void Ball::setVelocity(const glm::vec3& v) {
	velocity = v;
}
void Ball::setPosition(const glm::vec3& p) {
	position = p;
}

void Ball::applyImpulse(const glm::vec3& impulse, const glm::vec3& hit_point) {
	std::shared_ptr<const Match> grid = dynamic_pointer_cast<const Match>(read(match_id));
	if (grid == nullptr) { //if there is no field, then the match is over or an error occured
		return;
	}

	glm::vec3 HC = hit_point - position;
	//An impulse could arrive after after a ball reset, so ignore any hits that are obviously too far away
	if (glm::dot(HC, HC) > grid->ball_radius * grid->ball_radius) {
		return;
	}

	velocity += impulse;

	//Push the ball out of the collision point along the axis of the hit
	//collide line made from hit_point and impulse with circle
	float a = glm::dot(impulse, impulse);
	float b = 2.0f * glm::dot(HC, impulse);//quadratic formula
	float c = glm::dot(HC, HC) - grid->ball_radius * grid->ball_radius;
	float b24ac = b * b - 4 * a * c;
	if (fabs(a) > 0.01f && b24ac >= 0 && c < 0) { // hit point and line actually in ball with an impulse
		b24ac = sqrtf(b24ac);
		float s = (-b + b24ac) / (2 * a);
		float s2 = (-b - b24ac) / (2 * a);
		if (fabs(s) > fabs(s2)) { //select nearest way out from 2 collisions of line and circle
			s = s2;
		}
		position -= impulse * s;
	}
	if (asleep) {
		asleep = false;
		queue(id, time + grid->tick_interval, &Ball::update); //Queue up the update to run again at a fixed tick rate
	}
}

void Ball::narwhalHit(const glm::vec3& impulse, const glm::vec3& hit_point, const int& player_id, const bool& red_team) {
	applyImpulse(impulse, hit_point);
	if (red_team) {
		last_touch_red = player_id;
	}
	else {
		last_touch_blue = player_id;
	}
}

void Ball::destroy() {
	destroyed = true;
}


void Ball::resetBall() {
	position.x = -1.5f + random() * 3.0f;
	position.z = random() < 0.5f ? 4.0f : -4.0f;
	velocity.x = position.x < 0.0f ? MAX_RESPAWN_X_SPEED * random() : -MAX_RESPAWN_X_SPEED * random();
	velocity.z = -position.z;
	last_touch_blue = -1;
	last_touch_red = -1;
}

void Ball::print() const {
	printf("Ball at %f, %f, %f  v = %f, %f, %f\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z);
}


// Gets a ball view that enforces maximum speeds after roll back
Ball BallView::getView(const Ball& observed) {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	double observation_time = worlds->getWorldTime(NARBALL, observed.position);
	float observation_age = (float)(observation_time - observed.time);// time difference between what we're supposed to see and what we observed

	if (!fancy_interpolation) {
		last_view = observed;
		Ball view = observed;
		view.position = observed.position + observed.velocity * observation_age;
		view.time = observation_time;
		return view;
	}
	else {
		double dt = observed.time - last_view.time;
		if (dt <= 0) { // sometimes the clock moves to stay in sync with the server and this can happen
			last_view = observed;
			Ball view = observed;
			view.position = observed.position + observed.velocity * observation_age;
			view.time = observation_time;
			return view;
		}

		glm::vec3 view_position;
		glm::vec3 last_position = last_view.position + observed.velocity * last_age;
		glm::vec3 observed_position = observed.position + observed.velocity * observation_age;

		float max_distance_move = (float)(dt * Ball::max_speed * view_fudge_factor);
		glm::vec3 to_move = observed_position - last_position;
		float move_amount = glm::length(to_move);
		if (move_amount <= max_distance_move || move_amount > max_distance_move * 5) { // really fast was probably a reset we don't want to interpolate
			view_position = observed_position;
		}
		else {
			view_position = last_position + to_move * max_distance_move / move_amount;
		}
		last_view = observed;
		last_age = observation_age;
		Ball view = observed;
		view.position = view_position;
		view.time = observation_time;
		return view;
	}

}


glm::mat4 BallView::computePose(const Ball& ball) {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	double vantage_time = worlds->getWorldTime(NARBALL);

	glm::mat4 ball_pose = glm::mat4(1.0f);

	glm::vec3 position = ball.position;
	position.y = waterHeight(position.x, position.z, (float)vantage_time, water_flow) + (float)(bob_magnitude * sin(ball.id % 1000 + vantage_time * bob_rate)); // move up and down with the water

	ball_pose = glm::translate(ball_pose, position);//interpolate between moves
	ball_pose = glm::scale(ball_pose, glm::vec3(ball_radius, ball_radius, ball_radius));

	glm::vec3 spin_axis = glm::normalize(glm::vec3((ball.id & 0xff) - 128, ((ball.id >> 10) & 0xff) - 128, ((ball.id >> 20) & 0xff) - 128));
	ball_pose = glm::rotate(ball_pose, 500.0f + (float)vantage_time * 0.1f, spin_axis);
	return ball_pose;
}


void BallView::created(const Ball& observation) {
	id = observation.id;
	glm::mat4 ball_pose = computePose(observation);
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene_id = scene->createInstance(ball_model, ball_pose);
	last_sound_time = now();
}

//Update is called when an observation is made of an object that was also observed last frame on this same view
void BallView::updated(const Ball& observation) {
	Ball ball = getView(observation);
	glm::mat4 ball_pose = computePose(ball);
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->setPose(scene_id, ball_pose);

	AudioPlugin* sound = getTool<AudioPlugin>();
	if (ball.sound != -1) {
		if (last_sound_number != ball.sound_num && millisBetween(last_sound_time, now()) > 100) {
			float pitch = sound_pitch_range[ball.sound].first + randomFloat() * (sound_pitch_range[ball.sound].second - sound_pitch_range[ball.sound].first);
			sound->priorityPlay(ball.sound, ball.position, 0.5f, pitch, 2, 3);
			last_sound_time = now();
		}
	}
	last_sound_number = ball.sound_num;
	last_sound = ball.sound;

}

//Destroyed is called when an observation that was present in the last observation is no longer observed
//This view will be deleted immediately after this call (it's destructor will be called after this)
void BallView::destroyed() {
	//printf("Ball view destroyed: %ld\n", (long)id);
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(scene_id);
	cleaned = true ;
}

}