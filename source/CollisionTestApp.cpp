#include "CollisionTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"

CollisionTestApp::CollisionTestApp() {}

// Called when switching into this state before the first time run is called
void CollisionTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	//Load the model
	//scene->createModelSet(Ball::BALL_MODEL, Ball::BALL_MODEL, true);

	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 15, -5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-3 };
	float fov = 0.7f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));


	base_shape["box"] = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(0.2,0.2,0.2)));
	base_shape["tetra"] = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeTetra(glm::vec3(0, 0, 0.2), glm::vec3(0.2, 0, 0),glm::vec3(0, 0.2, 0),glm::vec3(0, 0, 0))) ;
	base_shape["cylinder"] = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeCylinder(glm::vec3(0, 0, 0.2), glm::vec3(0, 0, -0.2), 0.1f, 8)) ;

	int c = 0 ;
	for(auto& [name, base] : base_shape){
		std::shared_ptr<GLTF> model = std::make_shared<GLTF>();
		model->setPolyhedronModel(base->vertex,base->face,colors[c]) ;
		scene->createModelSet(name,model,false, true) ;
		Physics::RigidBody r(base) ;
		glm::mat4 pose = glm::translate(glm::mat4(1.0f), positions[c]) ;
		r.setPose(pose) ;
		instances.push_back(r);
		scene_ids.push_back( getTool<ScenePlugin>()->createInstance(name, pose));
		c++;
	}


	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(min, max, glm::vec4(1, 1, 1, 1));
	box = box->createMirrorImage(); // Flips winding order inside out
	scene->createModelSet("room", box);
	room_instance_id = scene->createInstance("room", glm::mat4(1.0f));

}

//Called every frame while the state is active
void CollisionTestApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	if (dt < 0 || dt > 0.5f) {
		dt = 0; // don't move on frames where something is amiss with the clock
	}
	time += dt ;
	last_run_time = current_time;

	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	float t = -1; // TODO implement raytracing to make balls clickable
	//Place the mouse particle
	glm::vec3 mouse_position;
	if (t > 0) { // collision
		particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1)); // blue
		mouse_position = window->window_target->camera_position + window->getMouseRay() * t; // hit postion
	}
	else { // no collision
		particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1)); // red
		mouse_position = window->window_target->camera_position + window->getMouseRay() * 3.0f; // arbitrary depth on no collision
	}
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particle_pose = glm::translate(particle_pose, mouse_position);
	particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
	particles->setPose(mouse_particle_id, particle_pose);


	int c = 0 ;
	if(! window->keyDown(SDLK_SPACE) && !OpenXRPlugin::ENABLED){
		for(auto& inst : instances){
			glm::mat4 pose = glm::translate(glm::mat4(1.0f), positions[c]);
			pose = glm::rotate(pose,(float)time, glm::vec3(0,1, c * 0.001f)) ;
			inst.setPose(pose);
			scene->setPose(scene_ids[c],pose) ;
			c++;
		}
	}

	if(OpenXRPlugin::ENABLED){
		OpenXRPlugin* controls = getTool<OpenXRPlugin>();
		glm::mat4 current_left_hand_pose = controls->getPose("/actions/general/in/left_pose");
		glm::mat4 current_right_hand_pose = controls->getPose("/actions/general/in/right_pose");
		int c=0 ;
		for (auto& inst : instances) {
			glm::mat4 hand_pose ;
			if(c == 0){
				hand_pose = current_left_hand_pose ;
			}else if(c == 1){
				hand_pose = current_right_hand_pose;
			}else{
				break ;
			}

			inst.setPose(hand_pose);
			c++;
		}
	}

	for(auto& id : last_display_particles){
		particles->destroyParticle(id);
	}
	last_display_particles = display_particles ;
	display_particles.clear();

	for(int iteration = 0 ; iteration < 1; iteration++){ // iterate a bunch to measure performance
	for(int k=1;k<instances.size();k++){
		for(int j=0;j<k;j++){
			auto result = Physics::detectCollision(&instances[k],0, &instances[j],0);
			if(result.size() > 0){
				/*
				glm::vec3 O(0,0,0) ;
				glm::vec3 A  = result[0].A.x ;
				glm::vec3 AB = result[0].B.x - result[0].A.x;
				glm::vec3 AC = result[0].C.x - result[0].A.x;
				glm::vec3 AD = result[1].A.x - result[0].A.x;
				glm::mat3 M(AB,AC,AD) ;
				M = glm::inverse(M) ;
				glm::vec3 bcd = M * (O-A) ;
				float b = bcd.x ;
				float c = bcd.y ;
				float d = bcd.z ;
				float a = 1 - b - c -d ;

				glm::vec3 check = a * A + b * result[0].B.x + c * result[0].C.x + d * result[1].A.x;
				printf("Check: %f, %f ,%f \n", check.x, check.y, check.z);
				glm::vec3 p1 = a * result[0].A.a + b * result[0].B.a + c * result[0].C.a + d * result[1].A.a ;
				glm::vec3 p2 = a * result[0].A.b + b * result[0].B.b + c * result[0].C.b + d * result[1].A.b ;

				glm::vec3 p = (p1 + p2)*0.5f ;
				printf("Found collision: %f, %f ,%f == %f,%f,%f\n", p1.x, p1.y, p1.z, p2.x,p2.y,p2.z);
				*/


				

				Physics::SupportPoint collision = Physics::getPenetration(result, &instances[k],0, &instances[j],0) ;

				if(iteration == 0 ){ // only update visual on first iteration
					int p_id = particles->createParticle(0);
					glm::mat4 particle_pose = glm::mat4(1.0f);
					particle_pose = glm::translate(particle_pose, collision.a);
					particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
					particles->setPose(p_id, particle_pose);
					particles->setColor(p_id, glm::vec4(0, 0, 0, 1));
					display_particles.push_back(p_id);

					p_id = particles->createParticle(0);
					particle_pose = glm::mat4(1.0f);
					particle_pose = glm::translate(particle_pose, collision.b);
					particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
					particles->setPose(p_id, particle_pose);
					particles->setColor(p_id, glm::vec4(0, 0, 0, 1));
					display_particles.push_back(p_id);

				
					p_id = particles->createParticle(0);
					glm::mat4 look = glm::lookAt(collision.a, collision.b, glm::vec3(0, 1, 0));
					float length = glm::distance(collision.a, collision.b) ;
					particle_pose = glm::mat4(1.0f);
					particle_pose = glm::translate(particle_pose, glm::vec3(0,0,-length/2));
					particle_pose = glm::scale(particle_pose, glm::vec3(particle_size*0.5f, particle_size*0.5f, length/2 + particle_size*0.5f));
					particle_pose = glm::inverse(look) * particle_pose ;

					particles->setPose(p_id, particle_pose);
					particles->setColor(p_id, glm::vec4(0, 0, 0, 1));
					display_particles.push_back(p_id);
				}
				
			}

		}
	}
	}


	updateCamera();

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void CollisionTestApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->destroyParticle(mouse_particle_id);
}

void CollisionTestApp::updateCamera() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	if (window->mouseDown(3)) { // right mouse button
		if (!mouse_down_right) {
			mouse_down_position_right = window->getMousePosition();
			camera_down_thi = camera_thi;
			camera_down_theta = camera_theta;
		}
		glm::vec2 mouse_position = window->getMousePosition();
		mouse_down_right = true;
		camera_theta = camera_down_theta + camera_x_speed * (mouse_position.x - mouse_down_position_right.x);
		camera_thi = camera_down_thi + camera_y_speed * (mouse_position.y - mouse_down_position_right.y);
		camera_thi = fmax(fmin(camera_thi, 3.14159f * 0.5f), 0.0f);
		mouse_down_position_right = window->getMousePosition();
		camera_down_thi = camera_thi;
		camera_down_theta = camera_theta;

	}
	else {
		mouse_down_right = false;
	}

	if (mouse_wheel_y_previous < window->getMouseWheelPosition().y) {
		zoom *= 0.95f;
	}
	else if (mouse_wheel_y_previous > window->getMouseWheelPosition().y) {
		zoom /= 0.95f;
	}

	if (zoom < 0.05f) {
		zoom = 0.05f ;
	}
	mouse_wheel_y_previous = window->getMouseWheelPosition().y;

	glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	glm::vec3 light_position = glm::vec3(cosf(light_theta) * cosf(light_thi), sinf(light_thi), sinf(light_theta) * cosf(light_thi)) * light_zoom;

	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 30);
}

