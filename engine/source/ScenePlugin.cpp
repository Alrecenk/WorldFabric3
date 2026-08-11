#include "ScenePlugin.h"
#include "Polygon.h"


ScenePlugin::ScenePlugin(VulkanPlugin* r, OpenXRPlugin* x) {
	renderer = r ;
	xr = x ;
	instances.reserve(2000);
}


//Create an instance of an existing model
int ScenePlugin::createInstance(const std::string& model_name, const glm::mat4& pose) {
	lock.lock();
	max_id++;
	int instance_id = max_id ;
	int model_set_id = name_to_model_set[model_name] ;
	instances[instance_id].models[0] = model_set_id;
	instances[instance_id].pose = pose;
	instances[instance_id].last_animation_update_time = now();
	//instances[instance_id].inv_pose = glm::inverse(pose);
	if (hasModel(model_name)) {
		instances[instance_id].skeleton = model_sets[model_set_id].gltf_model;
		instances[instance_id].bone_data = instances[instance_id].skeleton->getBoneVector();
		GLTFModelSet& model_set = model_sets[model_set_id]; // Use the mode lon the rendere to infer the type to call the templated setPose function
		instances[instance_id].main_instance = renderer->getRenderable(model_set.main_model_ids[0])->createInstance();
		if (SHADOWS_ENABLED) {
			instances[instance_id].shadow_instance = renderer->getRenderable(model_set.shadow_model_ids[0])->createInstance();
		}
	}else{
		printf("Attempted to create an instance when the model wasn't found: %s\n", model_name.c_str()) ;
	}
	instances[instance_id].cacheModelSets(this);
	lock.unlock();
	return instance_id ;
}

//Adds a model to an instance (the skeletons must match the base of that instance)
// Returns the index of the newly added model on the instance
int ScenePlugin::addModelToInstance(const int instance_id, const std::string& model_name) {
	if (hasInstance(instance_id)) {
		//instances[instance_id].lock.lock();
		int index = instances[instance_id].next_model_id++;
		instances[instance_id].models[index] = name_to_model_set[model_name];
		instances[instance_id].cacheModelSets(this);
		//instances[instance_id].lock.unlock();
		return index;
	}
	else {
		printf("Attempting to add model to instance that doesn't exist (%d) !\n", instance_id);
		return -1;
	}
}



// Removes a model from the given instance
void ScenePlugin::removeModelFromInstance(const int instance_id, const std::string& model_name) {
	if (hasInstance(instance_id)) {
		//instances[instance_id].lock.lock();
		int to_erase = -1;
		int erase_model_set_id = name_to_model_set[model_name] ;
		for (auto& [index, model] : instances[instance_id].models) {
			if (model == erase_model_set_id) {
				to_erase = index;
				break;
			}
		}
		if (to_erase != -1) {
			instances[instance_id].models.erase(to_erase);
		}

		instances[instance_id].cacheModelSets(this);
		//instances[instance_id].lock.unlock();
	}
	else {
		printf("Attempting to remove model from instance that doesn't exist (%d) !\n", instance_id);
	}
}


//Sets the first model on an instance
//Short hand for the functionality of add and remove model from instance when you always want there to be only one model
void ScenePlugin::setInstanceModel(const int instance_id, const std::string& model_name) {
	if (hasInstance(instance_id) && instances[instance_id].models[0] != name_to_model_set[model_name]) {
		instances[instance_id].models[0] = name_to_model_set[model_name];
		instances[instance_id].cacheModelSets(this);
	}
}

// Adds an animation and makes it referencable by name
void ScenePlugin::addAnimation(std::string animation_name, std::shared_ptr<GLTF> model, int index) {
	lock.lock();
	animations[animation_name].first = model;
	animations[animation_name].second = index;
	lock.unlock();
}

// Adds an animation and makes it referencable by name
void ScenePlugin::addAnimation(const std::string& animation_name, const std::string& glb_file_path, int index) {
	std::shared_ptr<GLTF> gltf_model;
	if (loaded_glb_file_cache.find(glb_file_path) != loaded_glb_file_cache.end()) {
		gltf_model = loaded_glb_file_cache[glb_file_path];
	}
	else {
		Variant file_data = Variant::loadFileBytes(glb_file_path);
		gltf_model = std::shared_ptr<GLTF>(new GLTF);
		gltf_model->setModel(file_data.getByteArray(), file_data.getArrayLength());
		gltf_model->transform = glm::mat4(1);
		loaded_glb_file_cache[glb_file_path] = gltf_model;
	}
	addAnimation(animation_name, gltf_model, index);
}

void ScenePlugin::matchAnimationToModel(const std::string& animation_name, const std::string& model_name){
	auto anim_gltf = getAnimationController(animation_name);
	auto model_gltf = getModelController(model_name);
	anim_gltf->transform = model_gltf->transform;
}

// Adds an animation to the given instance and returns the index of that animation on the instance
int ScenePlugin::animateInstance(int instance_id, const std::string& animation_name, bool looping) {
	//instances[instance_id].lock.lock();
	int index = instances[instance_id].next_animation_id++;
	instances[instance_id].animations[index] = AnimationInstance(animation_name, looping);
	//instances[instance_id].lock.unlock();
	return index;
}

// Adds an animation to the given instance and returns the index of that animation on the instance
int ScenePlugin::animateInstance(int instance_id, const std::string& animation_name, bool looping, float start_time, float speed) {
	//instances[instance_id].lock.lock();
	int index = instances[instance_id].next_animation_id++;
	instances[instance_id].animations[index] = AnimationInstance(animation_name, looping);
	instances[instance_id].animations[index].time = start_time;
	instances[instance_id].animations[index].speed = speed;
	//instances[instance_id].lock.unlock();
	return index;
}

// Sets the weight of animation vs other animations (IK will still override)
void ScenePlugin::setAnimationWeight(int instance_id, int animation_instance_id, float weight) {
	instances[instance_id].animations[animation_instance_id].weight = weight;
}

float ScenePlugin::getAnimationWeight(int instance_id, int animation_instance_id) {
	return instances[instance_id].animations[animation_instance_id].weight ;
}

// Sets the speed of an animation which is already playing
void ScenePlugin::setAnimationSpeed(int instance_id, int animation_instance_id, float speed){
	instances[instance_id].animations[animation_instance_id].speed = speed;
}

float ScenePlugin::getAnimationSpeed(int instance_id, int animation_instance_id){
	return instances[instance_id].animations[animation_instance_id].speed;
}

// Returns whether a nonloopinganimation has completed
bool ScenePlugin::animationDone(int instance_id, int animation_instance_id) {
	return instances[instance_id].animations[animation_instance_id].done;

}

//Removes an animation instance from an instance
void ScenePlugin::clearAnimation(int instance_id, int animation_instance_id) {
	//instances[instance_id].lock.lock();
	instances[instance_id].animations.erase(animation_instance_id);
	//instances[instance_id].lock.unlock();
}

//Removes an animation instance from an instance and applies a tranformation to entire instance atthe same time
void ScenePlugin::clearAnimation(int instance_id, int animation_instance_id, glm::mat4 transform) {
	//instances[instance_id].lock.lock();
	instances[instance_id].animations.erase(animation_instance_id);
	instances[instance_id].pose *= transform;
	//instances[instance_id].inv_pose = glm::inverse(instances[instance_id].pose);
	//instances[instance_id].lock.unlock();
}

//Sets the world pose of an instance
std::vector<GLTF::Node> ScenePlugin::getAnimationPose(const std::string& animation_name, float time) {
	return animations[animation_name].first->getPose(animations[animation_name].second, time);
}


void ScenePlugin::setPose(const int instance_id, const glm::mat4& pose) {
	if (hasInstance(instance_id)) {
		//instances[instance_id].lock.lock();
		instances[instance_id].pose = pose;
		//instances[instance_id].inv_pose = glm::inverse(pose);
		//instances[instance_id].lock.unlock();
	}
	else {
		printf("Setting pose on instance that does not exist! %d", instance_id);
		throw std::runtime_error("Setting pose on instance that does not exist!");
	}
}

void ScenePlugin::setPose(const int  instance_id, const glm::mat4& pose, const std::vector<glm::mat4>& bone_data) {
	//instances[instance_id].lock.lock();
	instances[instance_id].pose = pose;
	//instances[instance_id].inv_pose = glm::inverse(pose);
	instances[instance_id].bone_data = bone_data;
	//instances[instance_id].lock.unlock();
}

// Returns the bone data of the given instance on the last frame
std::vector<glm::mat4> ScenePlugin::getBoneData(const int instance_id) {
	if (hasInstance(instance_id)) {
		return instances[instance_id].bone_data;
	}else{
		return std::vector<glm::mat4>() ;
	}
}

// Overrides the orientation of a specific bone on a specific instance
void ScenePlugin::overrideBoneOrientation(const int instance_id, int bone, glm::quat orientation) {
	instances[instance_id].bone_override[bone].orientation = orientation;
	instances[instance_id].bone_override[bone].orientation_override = true;
}

// Overrides the orientation of a specific bone on a specific instance
void ScenePlugin::disableOverrideBoneOrientation(const int instance_id, int bone) {
	instances[instance_id].bone_override[bone].orientation_override = false;
}

// Overrides the scale of a specific bone on a specific instance
void ScenePlugin::overrideBoneScale(const int instance_id, int bone, glm::vec3 scale) {
	instances[instance_id].bone_override[bone].scale = scale;
	instances[instance_id].bone_override[bone].scale_override = true;
}



//gets the world pose of an instance
glm::mat4 ScenePlugin::getPose(const int instance_id) {
	return instances[instance_id].pose;
}


void ScenePlugin::deleteInstance(const int instance_id) {
	//TODO buffer deletes to avoid locking
	lock.lock();
	instances.erase(instance_id);
	lock.unlock();
}

//Used for tracking input latency to visual display
void ScenePlugin::setInputNum(const int instance_id, const int input_num){
	lock.lock();
	instances[instance_id].input_num = input_num ;
	lock.unlock();
}

// returns a pointer to the GLTF for the given model (Can be used for IK, raytracing, or physics collisions)
std::shared_ptr<GLTF> ScenePlugin::getModelController(const std::string& model_name) {
	return model_sets[name_to_model_set[model_name]].gltf_model;
}


std::shared_ptr<GLTF> ScenePlugin::getModelController(int instance_id, int model_index) {
	return model_sets[instances[instance_id].models[model_index]].gltf_model;
}

std::shared_ptr<GLTF> ScenePlugin::getAnimationController(const std::string& animation_name) {
	return animations[animation_name].first;
}

// Called on every plug-in before any plug-ins are run
void ScenePlugin::initialize() {
	printf("Scene plugin initialized.\n");
}

// Returns whether a model by the given name currently exists
bool ScenePlugin::hasModel(const std::string& model_name) {
	return model_sets.find(name_to_model_set[model_name]) != model_sets.end();
}

// Returns whether a model by the given name currently exists
bool ScenePlugin::hasModel(const int& model_set_id) {
	return model_sets.find(model_set_id) != model_sets.end();
}

// Deletes a model from this scene and the renderer plugin
void ScenePlugin::deleteModel(const std::string& model_name) {

	if (!hasModel(model_name)) {
		printf("Attempting to delete model %s, but it's not there!\n", model_name.c_str());
		return;
	}
	lock.lock();
	//TODO remove model from vulkan plugin
	printf("Model removal from scene 2 isn't implemented! Should probably do that.\n");
	model_sets.erase(name_to_model_set[model_name]);
	name_to_model_set.erase(model_name);
	lock.unlock();
}


// Returns whether an instance by the given name currently exists
bool ScenePlugin::hasInstance(const int  instance_id) {
	return instances.find(instance_id) != instances.end();
}

int ScenePlugin::modelCount(const int instance_id) {
	if (!hasInstance(instance_id)) {
		return 0;
	}
	return (int)(instances[instance_id].models.size());
}


int ScenePlugin::size() {
	int count = 0;
	for (auto& [id, instance] : instances) {
		count += modelCount(id);
	}
	return count;
}



// Runs the plugin, which updates linked instances in Vulkan to match the scene
void ScenePlugin::run() {

	lock.lock();
	//Buffer the group transforms
	for (auto& [id, g] : group_transforms) {
		g.second = g.first;
	}
	lock.unlock();

	OpenXRPlugin* xr = getTool<OpenXRPlugin>();

	glm::vec3 view_position ;
	if (OpenXRPlugin::ENABLED) {
		view_position = (xr->left_eye_target->camera_position + xr->right_eye_target->camera_position) * 0.5f;
	}else {
		view_position = renderer->window_target->camera_position ;
	}

	// Used to group the instances by GPU triangle models
	static std::unordered_map<int, std::vector<std::shared_ptr<void>>> gpu_instances;
	static std::unordered_map<int, std::vector<std::shared_ptr<void>>> shadow_instances;

	lock.lock();

	auto current_time = now();
	
	float dt = microsBetween(last_scene_run_time, current_time)*0.000001f ;
	last_scene_run_time = current_time ;
	for (auto& [name, instance] : instances) {
		//instance.lock.lock();
		if (instance.cached_model_sets.size() == 0) {
			//instance.lock.unlock();
			continue;
		}
		glm::mat4 instance_pose = instance.pose;
		if (group_transforms.find(instance.transform_group) != group_transforms.end()) {
			instance_pose = group_transforms[instance.transform_group].second * instance_pose;
		}

		auto& gltf_model = instance.skeleton;
		if (!gltf_model) {
			if (hasModel(instance.models[0])) {
				instance.skeleton = model_sets[instance.models[0]].gltf_model;
				instance.bone_data = instance.skeleton->getBoneVector();
				gltf_model = instance.skeleton;
			}
			else {
				printf("Instance without skeleton!? id: %d \n", name);
				//instance.lock.unlock();
				continue;
			}
		}

		if (gltf_model->boneless == 0) {
			
			if (instance.animations.size() > 0) { // if animating
				//gltf_model->setToOriginalPose();
				std::vector<std::pair<std::vector<GLTF::Node>, float>> poses;
				poses.push_back(std::pair<std::vector<GLTF::Node>, float>(gltf_model->original_pose, 0.0f)); // add original base pose as root pose of weight 0 for consistency across frames
				for (auto& [p, anim_instance] : instance.animations) {
					if (anim_instance.weight > 0) {
						auto& anim = animations[anim_instance.animation_name];
						auto& animation = anim.first->animations[anim.second];

						float dt = millisBetween(instance.last_animation_update_time, current_time) / 1000.0f;
						anim_instance.time += anim_instance.speed * dt;
						if (anim_instance.time > animation.duration) {
							if (anim_instance.looping) {
								anim_instance.time -= (int)(anim_instance.time / animation.duration) * animation.duration;
							}
							else {
								anim_instance.time = animation.duration; // lock on final frame
								anim_instance.done = true;
							}
						}
						auto pose = getAnimationPose(anim_instance.animation_name, anim_instance.time);
						poses.push_back(std::pair<std::vector<GLTF::Node>, float>(pose, anim_instance.weight));
					}
				}

				instance.last_animation_update_time = current_time;
				//printf("animating %s on %s  time: %f\n", anim_instance.animation_name.c_str(), instance.model_name.c_str(), anim_instance.time);
				auto pose = GLTF::blendPoseWeighted(poses);
				if (instance.IK_enabled && IK_ENABLED) { // IK and animating
					GLTF::setBasePose(pose, pose); // Sets the animations pose as the base pose for IK regularization
					gltf_model->setPose(pose);
					gltf_model->pins = instance.pins;
					gltf_model->applyPins(); // Run IK
				}
				else { // Animating but does not have Ik enabled
					gltf_model->setPose(pose);
				}
				instance.last_pose = gltf_model->getCurrentPose();
				instance.bone_data = gltf_model->getBoneVector();
			}
			else {// not animating
				if (instance.IK_enabled && IK_ENABLED) { // has IK but is not playing any animations
					GLTF::setBasePose(instance.last_pose, gltf_model->original_pose); // use original pose for regularization if no defined base
					gltf_model->setPose(instance.last_pose);
					gltf_model->pins = instance.pins;
					gltf_model->applyPins(); // Run IK
					instance.last_pose = gltf_model->getCurrentPose();
					instance.bone_data = gltf_model->getBoneVector();
				}				
				instance.last_animation_update_time = current_time; // need to update this in case we start an animation next frame
			}

			if (instance.bone_override.size() > 0) {
	
				for (auto& [bone_id, bone_override] : instance.bone_override) {
					if (bone_override.scale_override) {
						gltf_model->nodes[bone_id].scale = bone_override.scale;
					}
					if (bone_override.orientation_override) {
						gltf_model->nodes[bone_id].rotation = bone_override.orientation;
					}

				}

				gltf_model->computeNodeMatrices();
				instance.bone_data = gltf_model->getBoneVector();
			}

			if (instance.spring_bones.size() > 0) {
				simulateSpringBones(instance, instance_pose, dt, last_dt);
			}
				
		}


		// Put this instances models into the map for gpu instances
		bool first_model_set = true ;
		//for(auto& [m, model_set_id] : instance.models){
			//GLTFModelSet& model_set = model_sets[model_set_id] ;
		for(int k=0;k<instance.cached_model_sets.size();k++){
			GLTFModelSet* model_set = instance.cached_model_sets[k] ;
			
			// Put the pose and bone data onto the instances that go to the GPU 
			if(first_model_set){ // all models ina set must have th same instance type so we use the first one to infer it for setting poses	
				renderer->setInstancePose(model_set->main_model_ids[0], instance.main_instance.get(), instance_pose, instance.bone_data);
				renderer->setInputNum(model_set->main_model_ids[0], instance.input_num) ;
				//AsyncPlugin::inputDisplay(instance.input_num, 4, false);
				if (SHADOWS_ENABLED) {
					//renderer->getRenderable(model_set.shadow_model_ids[0])->setInstancePose(instance.shadow_instance.get(), instance_pose, instance.bone_data);
					renderer->setInstancePose(model_set->shadow_model_ids[0], instance.shadow_instance.get(), instance_pose, instance.bone_data);
				}
				first_model_set = false;
			}

			for(int k=0;k<model_set->main_model_ids.size();k++){
				gpu_instances[model_set->main_model_ids[k]].emplace_back(instance.main_instance) ;
			}

			if (SHADOWS_ENABLED) {
				for (int k = 0; k < model_set->shadow_model_ids.size(); k++) {
					shadow_instances[model_set->shadow_model_ids[k]].emplace_back(instance.shadow_instance);
				}
			}
			
		}

		//instance.lock.unlock();
	}

	lock.unlock();


	std::unordered_set<std::shared_ptr<RenderTarget>> main_targets ;
	main_targets.insert(renderer->window_target);
	if (OpenXRPlugin::ENABLED) {
		main_targets.insert(xr->left_eye_target);
		main_targets.insert(xr->right_eye_target);
	}


	std::unordered_set<std::shared_ptr<RenderTarget>> shadow_targets;
	for(auto& [light_id, light] : lights){
		//TODO apply transform group
		for(int k=0;k<light.targets.size();k++){
			shadow_targets.insert(light.targets[k]);
		}
	}
	
	//Update the models on the renderer
	for(auto& [renderable_id, instance_list] : gpu_instances){
		std::shared_ptr<Renderable> r = renderer->getRenderable(renderable_id);
		if(instance_list.size() > 0){
			r->setTargets(main_targets);
			r->setInstances(instance_list) ;
			instance_list.clear(); // keep the vectors allocated since we need them every frame but empty them
		}else{
			r->clearTargets();
		}
	}
	//Update the models on the renderer
	for (auto& [renderable_id, instance_list] : shadow_instances) {
		std::shared_ptr<Renderable> r = renderer->getRenderable(renderable_id);
		if(instance_list.size() > 0){
			r->setTargets(shadow_targets);
			r->setInstances(instance_list);
			instance_list.clear();
		}else{
			r->clearTargets();
		}
	}

	last_dt = dt ;
}


//Fetch an approximate bounding shape of the given model (useful for cursor triggers and physics objects)
std::shared_ptr<ConvexShape> ScenePlugin::getBoundingShape(const std::string& model_name) {
	lock.lock();
	std::vector<glm::dvec3> points;
	std::shared_ptr<GLTF> gltf = model_sets[name_to_model_set[model_name]].gltf_model;
	points.reserve(gltf->vertices.size());
	for (auto& v : gltf->vertices) {
		points.push_back(v.transformed_position);
	}
	std::vector<Polygon> polys = Polygon::buildApproximateHull(points, 32, 3);
	lock.unlock();
	return std::shared_ptr<ConvexShape>(new ConvexShape(polys));
}

// Creates an IK pin on the given instance
// Returns starting orientation
glm::quat ScenePlugin::createPin(const int instance_id, const std::string& pin_name, int bone, glm::vec3 local_point, float weight, float rot_weight) {
	//instances[instance_id].lock.lock();
	std::shared_ptr<GLTF> gltf = instances[instance_id].skeleton;
	gltf->setToOriginalPose();
	glm::quat initial_rot = gltf->createPin(pin_name, bone, local_point, weight, rot_weight);
	instances[instance_id].pins[pin_name] = gltf->pins[pin_name];
	gltf->deletePin(pin_name);
	//instances[instance_id].lock.unlock();
	return initial_rot;
}

// Set the target for a given pin
void ScenePlugin::setPinTarget(const int instance_id, const std::string& pin_name, glm::vec3 target) {
	//instances[instance_id].lock.lock();
	glm::mat4 inv_pose = glm::inverse(instances[instance_id].pose);
	instances[instance_id].pins[pin_name].target = inv_pose * glm::vec4(target, 1.0f);
	//instances[instance_id].lock.unlock();

}

// Set the rotation target for a given pin
void ScenePlugin::setPinTarget(const int instance_id, const std::string& pin_name, glm::quat rot_target) {
	//instances[instance_id].lock.lock();
	glm::mat4 inv_pose = glm::inverse(instances[instance_id].pose);
	instances[instance_id].pins[pin_name].rot_target = glm::quat_cast(inv_pose) * rot_target;
	//instances[instance_id].lock.unlock();
}

// Sets the position and rotation target to the best match for the given matrixc
void ScenePlugin::setPinTarget(const int instance_id, const std::string& pin_name, glm::mat4 target) {
	glm::vec4 pos = target * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	setPinTarget(instance_id, pin_name, glm::vec3(pos));

	glm::quat target_rot = glm::normalize(glm::quat_cast(target));
	if (fabs(glm::length(target_rot) - 1.0f) < 0.01f) {
		setPinTarget(instance_id, pin_name, target_rot);
	}
}

void ScenePlugin::deletePin(const int instance_id, const std::string& pin_name) {
	//instances[instance_id].lock.lock();
	instances[instance_id].pins.erase(pin_name);
	//instances[instance_id].lock.unlock();
}

// Returns whether the given pin exists
bool ScenePlugin::hasPin(const int instance_id, const std::string& pin_name) {
	auto& pins = instances[instance_id].pins;
	return !(pins.find(pin_name) == pins.end());
}

void ScenePlugin::enableIK(const int instance_id, bool ik_enabled) {
	instances[instance_id].IK_enabled = ik_enabled;
}

// Returns a pin's position in world coordinates
glm::vec3 ScenePlugin::getPinPosition(const int instance_id, const std::string& pin_name) {
	lock.lock();
	GLTF::Pin& pin = instances[instance_id].pins[pin_name];
	std::shared_ptr<GLTF> skeleton = instances[instance_id].skeleton;
	glm::vec3 point = instances[instance_id].pose * (skeleton->nodes[pin.bone].transform * (skeleton->nodes[pin.bone].bone_to_mesh * glm::vec4(pin.local_point, 1.0f)));
	lock.unlock();
	return point;
}

void ScenePlugin::setGroupTransform(int id, glm::mat4 pose) {
	group_transforms[id].first = pose;
}

void ScenePlugin::setTransformGroup(int instance_id, int group) {
	instances[instance_id].transform_group = group;
}



// Create a model set that can be attached to instances
	// This assumethe models are already loaded onto the vulkanplugin with the given ids
void ScenePlugin::createModelSet(const std::string& name, std::shared_ptr<GLTF> gltf, std::vector<int> model_ids, std::vector<int> shadow_model_ids, bool translucent){
	model_sets[next_model_id] = GLTFModelSet();
	GLTFModelSet& mset = model_sets[next_model_id];
	mset.translucent = translucent ;
	name_to_model_set[name] = next_model_id;
	next_model_id++;
	mset.gltf_model = gltf ;
	mset.main_model_ids = model_ids;
	mset.shadow_model_ids = shadow_model_ids;
}


//Convenience method to create a ModelSet from a gltf file using the default GLTF Vertex and Instance and default shaders
	// Creates and adds the GLTF models to the Vulkan Renderer
void ScenePlugin::createModelSet(const std::string& name, const std::string& glb_file_path, bool normalize, bool translucent) {
	std::shared_ptr<GLTF> gltf_model = std::shared_ptr<GLTF>(new GLTF);
	Variant file_data = Variant::loadFileBytes(glb_file_path);
	gltf_model->setModel(file_data.getByteArray(), file_data.getArrayLength());
	createModelSet(name, gltf_model, normalize, translucent) ;
}


//Convenience method to create a ModelSet from a gltf using the default GLTF Vertex and Instance and default shaders
// Creates and adds the GLTF models to the Vulkan Renderer
void ScenePlugin::createModelSet(const std::string& name, std::shared_ptr<GLTF>& gltf_model, bool normalize, bool translucent){
	
	model_sets[next_model_id] = GLTFModelSet();
	GLTFModelSet& mset = model_sets[next_model_id];
	mset.translucent = translucent ;
	name_to_model_set[name] = next_model_id;
	next_model_id++;
	
	
	mset.gltf_model = gltf_model ;
	if (normalize) {
		mset.gltf_model->transform = mset.gltf_model->getNormalizationTransform();
	}else{
		mset.gltf_model->transform = glm::mat4(1.0f);
	}
	mset.gltf_model->setToBasePose();
	mset.gltf_model->computeNodeMatrices();
	mset.gltf_model->applyTransforms();

	int min_max_bones = 10000;
	std::shared_ptr<AbstractShaderSet>& selected_set = mset.shader_set;
	if(translucent){
		for (int k = 0; k < default_translucent_shaders.size(); k++) {
			std::shared_ptr<AbstractShaderSet>& set = default_translucent_shaders[k];
			if (set->max_bones >= mset.gltf_model->nodes.size() && set->max_bones < min_max_bones) {
				selected_set = set;
				min_max_bones = set->max_bones;
			}
		}
	}else{
		for (int k = 0; k < default_triangle_shaders.size(); k++) {
			std::shared_ptr<AbstractShaderSet>& set = default_triangle_shaders[k];
			if (set->max_bones >= mset.gltf_model->nodes.size() && set->max_bones < min_max_bones) {
				selected_set = set;
				min_max_bones = set->max_bones;
			}
		}
	}

	if(min_max_bones != selected_set->max_bones){
		throw std::runtime_error("ScenePlugin couldn't find a default shader set supporting the number of bones on loaded model!");
	}
	//printf("Selected for maximum bones: %d\n", min_max_bones) ;

	auto main_model_set = selected_set->loadGLTF(mset.gltf_model, renderer, selected_set->main_program); // use the specialized to infer the triangle model templates

	for (int k = 0; k < main_model_set.size(); k++) {
		main_model_set[k]->phase = translucent ? TRANSLUCENT_PHASE : MAIN_PHASE;
		main_model_set[k]->group = min_max_bones + (translucent ? 100000 : 0); // group with everything else using this shader
		mset.main_model_ids.push_back(renderer->addRenderable(main_model_set[k]));
	}

	if (SHADOWS_ENABLED) {
		auto shadow_model_set = selected_set->loadGLTF(mset.gltf_model, renderer, selected_set->shadow_program);
		for (int k = 0; k < shadow_model_set.size(); k++) {
			shadow_model_set[k]->phase = SHADOW_PHASE;
			shadow_model_set[k]->group = min_max_bones + (translucent ? 100000 : 0);// group with everything else using this shader
			mset.shadow_model_ids.push_back(renderer->addRenderable(shadow_model_set[k]));
		}
	}
	
}

//Sets the morph weights on a model to be applied to all instances
void ScenePlugin::setMorphWeights(std::string model_name, std::vector<float> weights){
	auto& model_set = model_sets[name_to_model_set[model_name]] ;
	model_set.shader_set->setMorphModels(model_set.gltf_model, getTool<VulkanPlugin>() , weights) ;
}


//Load any spring bone data from the skeleton of the given instance
	// and enable it on that instance
void ScenePlugin::enableVRMSpringBones(int instance_id, float gravity_strength, float collider_scale, float min_spring_collision_radius){
	if (!hasInstance(instance_id)) {
		printf("Attempting to enable spring bones on an instance that doesn't exist!\n");
		return;
	}
	Instance& instance = instances[instance_id];
	instance.skeleton->computeNodeMatrices() ;
	glm::mat4 instance_pose = instance.pose;
	if (group_transforms.find(instance.transform_group) != group_transforms.end()) {
		instance_pose = group_transforms[instance.transform_group].second * instance_pose;
	}
	for(GLTF::SpringChain& chain : instance.skeleton->spring_chains){
		//printf("Chain: %s\n", chain.name.c_str()) ;
		for(int k=0;k<chain.joints.size() - 1;k++){ // we use the position o the next node as local point, so we get one less spring than the chain length
			SpringBone spring ;
			spring.node = chain.joints[k].node;
			//The VRM has these values but they don't map meaningfully to our implementation :(
			//spring.stiffness = chain.joints[k].stiffness;
			//spring.acceleration = chain.joints[k].gravity*gravity_strength ;
			//spring.drag = chain.joints[k].drag ;

			//stiffness parameter doesn't directly map to our exponential decay spring system
			// But empirically these numbers are reasonable
			if(chain.joints[k].stiffness >= 0.7f){
				spring.half_return_time = 0.3f ;
			}else if(chain.joints[k].stiffness >= 0.4f){
				spring.half_return_time = 0.8f;
			}else{
				spring.half_return_time = 1.0f;
			}

			spring.acceleration = glm::vec3(0, -gravity_strength, 0);
			spring.local_point = instance.skeleton->nodes[chain.joints[k+1].node].base_translation ;
			//printf("base translation: %f, %f, %f\n", spring.local_point.x, spring.local_point.y, spring.local_point.z) ;
			//spring.world_point = instance_pose * (instance.skeleton->nodes[spring.node].bone_to_model * glm::vec4(spring.local_point, 1.0f));
			spring.reset = true ;

			spring.collision_radius = chain.joints[k].radius * collider_scale ;
			if(spring.collision_radius < min_spring_collision_radius){
				spring.collision_radius = min_spring_collision_radius ;
			}
		
			for (int c = 0 ; c < instance.skeleton->colliders.size(); c++) {
				spring.colliders.push_back(c);
			}
			instance.spring_bones[spring.node] = spring;
		}
	}

	for(GLTF::SphereCollider& vrm_collider : instance.skeleton->colliders){
		Capsule scene_collider ;
		scene_collider.node = vrm_collider.node ;
		scene_collider.local_center = {vrm_collider.offset, glm::vec3(0, 0, 0)};
		scene_collider.local_radius = vrm_collider.radius * collider_scale ;
		
		//printf("Loaded collider %d - node : %d, radius : %f, center : %f,%f,%f\n",(int) instance.colliders.size(),scene_collider.node, scene_collider.local_radius, scene_collider.local_center.x, scene_collider.local_center.y, scene_collider.local_center.z);
		instance.colliders.push_back(scene_collider);
		
	}

	for(GLTF::CollisionGroup& group : instance.skeleton->collision_groups){
		printf("Group =  %s:", group.name.c_str());
		for(int k : group.colliders){
			printf("%d,", k) ;
		}
		printf("\n");

	}


}

// Remove any active spring bones on the given instance
void ScenePlugin::clearSpringBones(int instance_id){
	if(!hasInstance(instance_id)){
		return ;
	}
	Instance& instance = instances[instance_id] ;
	instance.spring_bones.clear();
	
}

std::vector<std::pair<glm::vec3, float>> ScenePlugin::getSpringBoneWorldPositions(int instance_id){
	std::vector<std::pair<glm::vec3, float>> points ;
	if (!hasInstance(instance_id)) {
		return points ;
	}
	Instance& instance = instances[instance_id];
	for(auto& [node, spring] : instance.spring_bones){
		
		GLTF::Node& node = instance.skeleton->nodes[spring.node];
		//Where the bone would be pointing if it were not springy
		
		glm::mat4 instance_pose = instance.pose;
		if (group_transforms.find(instance.transform_group) != group_transforms.end()) {
			instance_pose = group_transforms[instance.transform_group].second * instance_pose;
		}

		glm::mat4 bone_to_world = instance_pose * node.bone_to_model;
		glm::vec3 bone_zero = bone_to_world * glm::vec4(0, 0, 0, 1.0f);
		glm::vec3 target = bone_to_world * glm::vec4(spring.local_point, 1.0f);
	
		float target_length = glm::distance(bone_zero, target);
		float world_radius = spring.collision_radius * target_length / glm::length(spring.local_point);
		
		points.push_back({ spring.world_point,world_radius});
	}
	return points ;
}

std::vector<glm::vec3> ScenePlugin::getSpringBoneTargetPositions(int instance_id) {
	std::vector<glm::vec3> points;
	if (!hasInstance(instance_id)) {
		return points;
	}
	Instance& instance = instances[instance_id];
	for (auto& [node, spring] : instance.spring_bones) {
		points.push_back(spring.last_target);
	}
	return points;
}

std::vector<ScenePlugin::Capsule> ScenePlugin::getSpringBoneColliders(int instance_id){
	return instances[instance_id].colliders ;
}


//Simulates spring bones on an instance that has them
// The instance should already have had all other posing performed on its skeleton before this is called
void ScenePlugin::simulateSpringBones(Instance& instance,glm::mat4& instance_pose, float dt, float prev_dt){
	instance.skeleton->computeNodeMatrices();
	//Update colliders on model for the current pose
	for(Capsule& collider : instance.colliders){
		GLTF::Node& node = instance.skeleton->nodes[collider.node];
		glm::mat4 bone_to_world = instance_pose * node.bone_to_model;
		collider.world_center.first = bone_to_world*glm::vec4(collider.local_center.first,1) ;
		collider.world_center.second = bone_to_world * glm::vec4(collider.local_center.second, 1);
		collider.world_radius = collider.local_radius * glm::distance(collider.world_center.first, collider.world_center.second)/glm::distance(collider.local_center.first, collider.local_center.second); 
	}
	// Time ratio is used to adjust verlet integration for variable framerate
	float time_ratio = std::min(std::max(0.5f,dt / prev_dt), 2.0f); //clamped to prevent jumps on frame hitching
	for (auto& [node_id, spring] : instance.spring_bones) {
			GLTF::Node& node = instance.skeleton->nodes[spring.node] ;
			//Calculate where the bone would be pointing if it were not springy
			glm::mat4 bone_to_world = instance_pose * node.bone_to_model ;
			glm::vec3 bone_zero = bone_to_world * glm::vec4(0,0,0, 1.0f);
			
			GLTF::Node& parent = instance.skeleton->nodes[instance.skeleton->nodes[spring.node].parent];
			glm::mat4 unrotate = glm::mat4_cast(glm::inverse(node.rotation) * node.base_rotation );
			glm::mat4 base_to_world = instance_pose * node.bone_to_model * unrotate ;
			glm::vec3 target = base_to_world * glm::vec4(spring.local_point, 1.0f);
			spring.last_target = target ;
			if (spring.reset) {
				spring.world_point = target ;
				spring.prev_world_point = target;
				spring.reset = false;
			}
			//Half-lives for spring stiffness and drag createp predictable exponetial decay of movement
			float stiffness_factor = 1.0f -expf(-logf(2) * dt/ spring.half_return_time);
			float drag_factor = expf(-logf(2) * dt / spring.half_velocity_time);
			//Verlet integration with spring and applied forces
			glm::vec3 velocity_step = (spring.world_point - spring.prev_world_point) * drag_factor * time_ratio;
			glm::vec3 external_acceleration = spring.acceleration * (dt * dt);
			glm::vec3 spring_offset = (target - spring.world_point) * stiffness_factor;
			glm::vec3 next_world_point = spring.world_point + spring_offset + velocity_step + external_acceleration;
			
			//Handle constraints after integration
			//Scale to be correct length
			float target_length = glm::distance(bone_zero, target);
			float length = glm::distance(bone_zero, next_world_point);
			next_world_point = bone_zero + (next_world_point - bone_zero) * target_length / length;
			
			//Resolve collisions
			float spring_world_radius = spring.collision_radius * target_length / glm::length(spring.local_point);
			for(int c : spring.colliders){
				next_world_point += resolveCollision(next_world_point, spring_world_radius, instance.colliders[c]);
			}

			spring.prev_world_point = spring.world_point;
			spring.world_point = next_world_point;
			
			//Align bone to face the spring point
			glm::dvec3 to_target = glm::normalize(glm::dvec3(target) - glm::dvec3(bone_zero)) ;
			glm::dvec3 to_point = glm::normalize(glm::dvec3(spring.world_point) - glm::dvec3(bone_zero)) ;
			double dot = glm::dot(to_target, to_point) ;

			if(abs(dot) < 0.99997){ // don't rotate at all if it's moot
				//Get rotation in world space
				glm::quat world_delta = glm::angleAxis((float)acos(dot), glm::vec3(glm::normalize(glm::cross(to_target,to_point)))) ;
				glm::quat world_rot_old = glm::quat_cast(base_to_world);
				glm::quat world_rot_new = world_delta * world_rot_old;
				//get parent rotation
				glm::mat4 parent_bone_to_world = instance_pose * instance.skeleton->nodes[node.parent].bone_to_model ;
				glm::quat parent_world_rot = glm::quat_cast(parent_bone_to_world) ;
				// make the bone rotation what is needed to get the world rotation after the parent's
				node.rotation = glm::inverse(parent_world_rot) * world_rot_new ;
				node.rotation = glm::normalize(node.rotation) ;
				//Recompute matrices for this node and its children
				instance.skeleton->computeNodeMatrices(spring.node,instance.skeleton->nodes[node.parent].bone_to_model) ;
			}
			
	}
	instance.bone_data = instance.skeleton->getBoneVector();
}


// Returns the vector to move A so it does not collide with B in world coordinates
	//Returns (0,0,0) if the yare already not colliding
std::pair<bool,glm::vec3> ScenePlugin::resolveCollision(const Capsule& A,const Capsule& B){
	glm::vec3 u = A.world_center.second - A.world_center.first; // Direction of Capsule A core
	glm::vec3 v = B.world_center.second - B.world_center.first; // Direction of Capsule B core
	glm::vec3 w = A.world_center.first - B.world_center.first; // Direction from B(0) to A(0)

	//The bits that ultimately go into Cramer's rule for finding nearest point on lines
	float a = glm::dot(u, u);         // Squared length of segment A
	float b = glm::dot(u, v);
	float c = glm::dot(v, v);         // Squared length of segment B
	float d = glm::dot(u, w);
	float e = glm::dot(v, w);
	float denom = a * c - b * b;
	float s, t;

	//printf("A:%f, B%f, c:%f, d:%f, e:%f, denom: %f\n", a,b,c,d,e,denom) ;

	if (fabs(a) <= 1e-6 && fabs(c) <= 1e-6) { // Both Capsules are actually spheres
		s = 0 ;
		t = 0 ;
	}else if(fabs(a) <= 1e-6){ // A is asphere
		s = 0 ;
		t = glm::clamp(e / c, 0.0f, 1.0f) ; // vector projection
	}else if(fabs(c) <= 1e-6){// B is a sphere
		s = glm::clamp(-d / a, 0.0f, 1.0f);
		t = 0;
	}else if (denom < 1e-8f) { // Both are capsules and are parallel
		s = 0.5f; // Pick arbitrary start of A
		t = glm::clamp(e / c, 0.0f, 1.0f); // (if t is out of range s will get recomputed in the next step ot be closest to t)
		
	}else{// Both are proper capsules
		s = (b * e - c * d) / denom; // cramer's rule
		t = (a * e - b * d) / denom;
		//printf("S and T before clamp: %f, %f\n", s, t);
		
	}
	//Reproject with the endpoints if the solution wasn't on the [0,1] line segmnets
	if (s <= 0.0f) {
		s = 0.0f;
		t = glm::clamp(e / c, 0.0f, 1.0f);
	}
	else if (s >= 1.0f) {
		s = 1.0f;
		t = glm::clamp((b + e) / c, 0.0f, 1.0f);
	}
	if (t <= 0.0f) {
		t = 0.0f;
		s = glm::clamp(-d / a, 0.0f, 1.0f);
	}
	else if (t >= 1.0f) {
		t = 1.0f;
		s = glm::clamp((b - d) / a, 0.0f, 1.0f);
	}
	

	// Calculate final closest points
	glm::vec3 closest_A = A.world_center.first + s * u;
	glm::vec3 closest_B = B.world_center.first + t * v;
	//Check for collision
	glm::vec3 delta = closest_A - closest_B; // It's A - B because we want to move A
	float dist2= glm::dot(delta, delta);
	float radius_sum = A.world_radius + B.world_radius;
	if (dist2 < radius_sum * radius_sum) {
		float dist = sqrtf(dist2);
		return {true,(delta / dist) * (radius_sum - dist)};
	}
	// No collision
	return {false,glm::vec3()}; //return 0,0,0
}


// Returns the vector to move a sphere so it does not collide with B in world coordinates
//Returns (0,0,0) if the yare already not colliding
glm::vec3 ScenePlugin::resolveCollision(const glm::vec3 sphere_center, const float& sphere_radius, const Capsule& capsule) {
	glm::vec3 u = capsule.world_center.second - capsule.world_center.first;
	glm::vec3 v = sphere_center - capsule.world_center.first;

	// Project sphere center onto the line segment of the capsule
	float t = glm::dot(v, u) / glm::dot(u, u);
	t = glm::clamp(t, 0.0f, 1.0f);

	glm::vec3 closestPointOnSegment = capsule.world_center.first + t * u;
	glm::vec3 delta = sphere_center - closestPointOnSegment;
	float dist2 = glm::dot(delta, delta);
	float radius_sum = sphere_radius + capsule.world_radius;

	if (dist2 < radius_sum*radius_sum) {
		float dist = sqrtf(dist2) ;
		glm::vec3 normal = (dist < 1e-6f) ? glm::vec3(0, 1, 0) : (delta / dist);
		return normal * (radius_sum - dist);
	}

	return glm::vec3(0.0f);
}