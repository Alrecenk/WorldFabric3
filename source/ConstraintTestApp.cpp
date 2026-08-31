#include "ConstraintTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"

ConstraintTestApp::ConstraintTestApp() {}

// Called when switching into this state before the first time run is called
void ConstraintTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();


	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 0.5, -0.5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-3 };
	float fov = 0.7f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));


	cell = std::make_shared<Physics::SimpleLocalPhysicsCell>() ;

	float ball_radius = 0.5f ;
	float ball_mass = 1.0f ;
	std::shared_ptr<Physics::Sphere> ball_shape = std::make_shared<Physics::Sphere>(ball_radius, ball_mass);
	scene->createModelSet(BALL_MODEL, BALL_MODEL, true);
	glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(ball_radius, ball_radius, ball_radius)) ;
	ball_type = cell->addType(ball_shape, BALL_MODEL,transform, 0.6f, 0.6f) ;

	
	float box_size = 1.0f ;
	float box_mass = 2.0f ;
	std::shared_ptr<Physics::ConvexPolyhedron> box_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(box_size, box_size, box_size), box_mass));
	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(glm::vec3(-box_size * 0.5, -box_size * 0.5f, -box_size * 0.5f), glm::vec3(box_size * 0.5f, box_size * 0.5f, box_size * 0.5f), glm::vec4(0.5, 0.5, 1, 1));
	scene->createModelSet("box", box, false, false);
	transform = glm::mat4(1.0f);
	box_type = cell->addType(box_shape, "box", transform,0.4f,0.6f );

	float wall_size = 30.0f ;
	std::shared_ptr<GLTF> wall = std::make_shared<GLTF>();
	wall->setBoundingBoxModel(glm::vec3(-wall_size * 0.5, -wall_size * 0.5f, -wall_size * 0.5f), glm::vec3(wall_size * 0.5f, wall_size * 0.5f, wall_size * 0.5f), glm::vec4(0.7f, 0.7f, 0.8f, 1));
	scene->createModelSet("wall", wall, false, false);
	std::shared_ptr<Physics::ConvexPolyhedron> wall_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(wall_size, wall_size, wall_size)));
	wall_type = cell->addType(wall_shape, "wall", transform,0.6f,0.6f);

	float rod_mass = 2.0f ;
	std::shared_ptr<Physics::ConvexPolyhedron> rod_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeCylinder(glm::vec3(0, 0, 1.0f), glm::vec3(0, 0, -1.0f), 0.5f, 16, rod_mass));
	std::shared_ptr<GLTF> model = std::make_shared<GLTF>();
	model->setPolyhedronModel(rod_shape->vertex, rod_shape->face, glm::vec4(0.6f,0.1f,0.5f,0.5f));
	scene->createModelSet("rod", model, false, true);
	rod_type = cell->addType(rod_shape, "rod", transform, 0.4f,0.2f);


	float jar_mass = 2.0f;
	float jar_scale = 0.6f ;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(jar_scale, jar_scale, jar_scale));
	scene->createModelSet(JAR_MODEL, JAR_MODEL, true);
	std::shared_ptr<GLTF> jar_model = scene->getModelController(JAR_MODEL);
	std::shared_ptr<Physics::ConvexPolyhedron> jar_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeApproximateHull(jar_model, jar_mass));
	jar_shape = std::make_shared<Physics::ConvexPolyhedron>(*(jar_shape.get()),transform, jar_mass) ;
	//std::shared_ptr<GLTF> model2 = std::make_shared<GLTF>();
	//model2->setPolyhedronModel(jar_shape->vertex, jar_shape->face, glm::vec4(0.0f, 0.5f, 0.5f, 0.5f));
	//scene->createModelSet("jar", model2, false, true);
	jar_type = cell->addType(jar_shape, JAR_MODEL, transform, 0.1f,0.6f);
	

/*
	float fox_scale = 5.0f;
	float fox_mass = 0.0f ;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(-fox_scale, -fox_scale, fox_scale));
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(FOX_MODEL, FOX_MODEL, true);
	std::shared_ptr<GLTF> fox_model = scene->getModelController(FOX_MODEL);
	//Physics::ConvexPolyhedron part = Physics::ConvexPolyhedron::makeApproximateHull(bunny_model,bunny_mass);
	std::vector<Physics::ConvexPolyhedron> fox_parts = Physics::ConvexPolyhedron::collectConvexPiecesByBone(fox_model,20,4,0.5f,0.001f);
	int k= 0 ;
	printf("Fox parts: %d\n",(int) fox_parts.size()) ;
	for(auto& part : fox_parts){
		k++;
		std::shared_ptr<Physics::ConvexPolyhedron> shape = std::make_shared<Physics::ConvexPolyhedron>(part, transform, fox_mass);
		std::string name = concat(FOX_MODEL, k) ;
		std::shared_ptr<GLTF> model2 = std::make_shared<GLTF>();
		model2->setPolyhedronModel(shape->vertex, shape->face, glm::vec4(0.4f, 0.3f, 0.3f, 1.0f));
		scene->createModelSet(name, model2, false, false);
		glm::mat4 display_m = glm::mat4(0.0f) ;
		int type = cell->addType(shape, name, display_m, 0.1f, 0.6f);
		cell->add(type,glm::vec3(0,0,0)) ;
	}
	scene->createInstance(FOX_MODEL,transform) ;
*/
	


	float bunny_scale = 1.7f;
	float bunny_mass = 8.0f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(bunny_scale, bunny_scale, bunny_scale)); 
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(BUNNY_MODEL, BUNNY_MODEL, true);
	std::shared_ptr<GLTF> bunny_model = scene->getModelController(BUNNY_MODEL);
	std::vector<Physics::ConvexPolyhedron> bunny_parts = Physics::ConvexPolyhedron::makeApproximateSurfaceHulls(bunny_model, bunny_mass,20,3);
	scene->createModelSet(BUNNY_VISUAL_MODEL, BUNNY_VISUAL_MODEL, true);
	bunny_type = cell->addType(bunny_parts, BUNNY_VISUAL_MODEL, transform , 0.1f, 0.6f);


	// Add the container blocks
	glm::vec3 mid = (min + max) * 0.5f;
	cell->add(wall_type, glm::vec3(mid.x, min.y - wall_size * 0.5f, mid.z)) ;
	cell->add(wall_type, glm::vec3(max.x + wall_size * 0.5f, mid.y, mid.z));
	cell->add(wall_type, glm::vec3(min.x - wall_size * 0.5f, mid.y, mid.z));
	cell->add(wall_type, glm::vec3(mid.x, mid.y, min.z - wall_size * 0.5f));
	cell->add(wall_type, glm::vec3(mid.x, mid.y, max.z + wall_size * 0.5f));

}

//Called every frame while the state is active
void ConstraintTestApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	
	if(dt <= 0.001f || dt > 0.5f){ 
		dt = 0.001f ; // don't move on frames where something is amiss with the clock
	}
	
	last_run_time = current_time;

	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	float t = -1 ; // TODO implement raytracing to make balls clickable
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
	particle_pose = glm::scale(particle_pose, glm::vec3(0.03, 0.03, 0.03));
	particles->setPose(mouse_particle_id, particle_pose);


	updateCamera();
	cell->runPhysicsFrame(dt, 20);
	cell->updateGraphics();


	if(millisBetween(last_ball_time,current_time) > millis_between_balls && cell->bodies.size() < max_balls){
		last_ball_time = current_time ;
		glm::vec3 pos = { min.x + (0.4f + randomFloat() * 0.2f) * (max.x - min.x),12.0f,min.z + 0.5f };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,1.0f+randomFloat() * 4.0f};
		glm::mat4 r= glm::rotate(glm::mat4(1.0f), (float)(timeMilliseconds()*0.002),glm::vec3(0,1,0)) ;
		pos = r * glm::vec4(pos,1) ;
		vel = r * glm::vec4(vel,0);
		float rand = randomFloat() ;
		int type = ball_type ;
		if(rand < 0.2f){
			type = box_type ;
		}else if(rand < 0.35f){
			type = rod_type ;
		}
		else if (rand < 0.4f) {
			type = bunny_type;
		}else if(rand< 0.65){
			type = jar_type ;
		}
		auto id = cell->add(type, pos, vel, glm::vec3(randomFloat()*2.0f-1.0f, randomFloat() * 2.0f-1.0f, randomFloat() * 2.0f-1.0f));
	}


	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void ConstraintTestApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->destroyParticle(mouse_particle_id);
}

void ConstraintTestApp::updateCamera() {
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

	if (zoom < 1.0f) {
		zoom = 1.0f;
	}
	mouse_wheel_y_previous = window->getMouseWheelPosition().y;

	glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	glm::vec3 light_position = glm::vec3(cosf(light_theta) * cosf(light_thi), sinf(light_thi), sinf(light_theta) * cosf(light_thi)) * light_zoom;

	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 35);
}