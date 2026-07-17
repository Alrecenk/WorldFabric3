#include "OpenXRPlugin.h"

#include <filesystem>

// Boots SteamVR and sets up openGL and links to controllers and other hardware
OpenXRPlugin::OpenXRPlugin(std::string action_file_path)
	: m_pHMD(NULL)
	, m_iTrackedControllerCount(0)
	, m_iTrackedControllerCount_Last(-1)
	, m_iValidPoseCount(0)
	, m_iValidPoseCount_Last(-1)
	, m_strPoseClasses("") {
	if (ENABLED) {
		memset(m_rDevClassChar, 0, sizeof(m_rDevClassChar)); // TODO what does this even do?

		action_file = action_file_path;

		if (!BInit()) {
			//Shutdown();
			//data.add(new RuntimeFlag("shutdown", 1));
			printf("OpenXR failed to initialize, trying to continue but probably gonna crash.\n");
		}
		SDL_StartTextInput();
		SDL_HideCursor();
	}

}

// Called on every plug-in before any plug-ins are run
// Adds an XRStatus object with the tag "xr_status" contain data other plugins can use to interact with the headset
void OpenXRPlugin::initialize() {
	
	if (ENABLED) {
		//data.add(left_eye);
		//data.add(right_eye);
		printf("OpenXR plugin initialized.\n");
	}else {
		printf("OpenXR is initialized but disabled. Calls to it may crash.\n");
	}

	async_enabled = false; // Needs to run on main thread
}

void OpenXRPlugin::run() {
	if (!ENABLED) {
		return;
	}

	if (left_eye_target) { //eye targets are set-up externally so it's possible this gets called before that happens
		VulkanPlugin* renderer = getTool<VulkanPlugin>();
		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.0f;
		bounds.uMax = 1.0f;
		bounds.vMin = 0.0f;
		bounds.vMax = 1.0f;

		vr::VRVulkanTextureData_t vulkanData;

		vulkanData.m_pDevice = renderer->device;
		vulkanData.m_pPhysicalDevice = renderer->physical_device;
		vulkanData.m_pInstance = renderer->vulkan_instance;
		vulkanData.m_pQueue = renderer->vulkan_queue;
		vulkanData.m_nQueueFamilyIndex = renderer->vulkan_queue_family;

		vulkanData.m_nWidth = left_eye_target->final_image->getWidth();
		vulkanData.m_nHeight = left_eye_target->final_image->getHeight();
		vulkanData.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
		vulkanData.m_nSampleCount = 1;

		vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto };

		renderer->immediateSubmit([&](VkCommandBuffer cmd) {
			renderer->requireLayout(cmd, left_eye_target->final_image->getVulkanImage(renderer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			renderer->requireLayout(cmd, right_eye_target->final_image->getVulkanImage(renderer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			});
		vulkanData.m_nImage = (uint64_t)left_eye_target->final_image->getVulkanImage(renderer)->image;
		vr::VRCompositor()->Submit(vr::Eye_Left, &texture, &bounds);

		vulkanData.m_nImage = (uint64_t)right_eye_target->final_image->getVulkanImage(renderer)->image;
		vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &bounds);


		//Update the headset pose for the next frame
		UpdateHMDMatrixPose();

		//glm::mat4 y_flip = glm::scale(glm::mat4(1), glm::vec3(1,-1,1)); 
		//head_pose = y_flip*raw_pose*y_flip ;

		left_eye_target->camera_matrix = m_mat4ProjectionLeft * m_mat4eyePosLeft * inverse_head_pose;
		left_eye_target->camera_position = head_pose * glm::inverse(m_mat4eyePosLeft) * glm::vec4(0, 0, 0, 1);

		right_eye_target->camera_matrix = m_mat4ProjectionRight * m_mat4eyePosRight * inverse_head_pose;
		right_eye_target->camera_position = head_pose * glm::inverse(m_mat4eyePosRight) * glm::vec4(0, 0, 0, 1);
	}
	//printf("OpenXR plugin updated render target camera matrices\n");
	/*
	printf("VR projection:\n");
	Variant(m_mat4eyePosLeft).printFormatted();
	printf("left eye position: %f, %f, %f\n", left_eye->camera_position.x, left_eye->camera_position.y, left_eye->camera_position.z);
	glm::vec4 a = left_eye->camera_matrix * glm::vec4(left_eye->camera_position, 1.0f);
	printf ("left eye projected on left eye w : %f\n", a.w);
	*/

	//Poll the controller state
	//Update all action sets
	for (auto& [key, value] : action_sets) {
		vr::VRActiveActionSet_t active_action_set = { 0 };
		active_action_set.ulActionSet = value;
		vr::VRInput()->UpdateActionState(&active_action_set, sizeof(active_action_set), 1);
	}

	for (auto& [key, value] : boolean_actions) {
		boolean_values[key] = GetDigitalActionState(value);
		/*if (boolean_values[key]) {
			printf("Pressed: %s\n", key.c_str());
		}*/
	}

	vr::InputAnalogActionData_t analog_data;
	for (auto& [key, value] : vector_actions) {
		if (vr::VRInput()->GetAnalogActionData(value, &analog_data, sizeof(analog_data), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analog_data.bActive) {
			vector_values[key].x = analog_data.x;
			vector_values[key].y = analog_data.y;

			/*if (glm::length(vector_values[key]) > 0.01f) {
				printf("Vector input active: %s\n", key.c_str());
			}*/

		}
	}

	vr::InputPoseActionData_t pose;
	for (auto& [key, value] : pose_actions) {
		if (vr::VRInput()->GetPoseActionDataRelativeToNow(value, vr::TrackingUniverseStanding, 0.0f, &pose, sizeof(pose), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None
			&& pose.bActive && pose.pose.bPoseIsValid) {
			pose_values[key] = ConvertSteamVRMatrixToMat4(pose.pose.mDeviceToAbsoluteTracking);
		}
	}

	vr::InputSkeletalActionData_t skeleton;
	for (auto& [key, value] : skeleton_actions) {
		//printf("%s skeleton action exists\n", key.c_str());
		if (vr::VRInput()->GetSkeletalActionData(value, &skeleton, sizeof(skeleton)) == vr::VRInputError_None
			&& skeleton.bActive) {
			unsigned int num_bones = 0;
			vr::VRInput()->GetBoneCount(value, &num_bones);

			int* parent_indices = (int*)malloc(sizeof(int) * num_bones);
			vr::EVRInputError vr_error = vr::VRInput()->GetBoneHierarchy(value, parent_indices, num_bones);
			if (vr_error != vr::VRInputError_None) {
				continue;
			}
			vr::VRBoneTransform_t* raw_bones = (vr::VRBoneTransform_t*)malloc(sizeof(vr::VRBoneTransform_t) * num_bones);

			vr_error = vr::VRInput()->GetSkeletalBoneData(value, vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Parent, vr::EVRSkeletalMotionRange::VRSkeletalMotionRange_WithController, raw_bones, num_bones);
			if (vr_error != vr::VRInputError_None) {
				continue;
			}

			char* raw_name = (char*)malloc(vr::k_unMaxBoneNameLength);

			//printf("'%s' skeleton found %d bones!\n", key.c_str(), num_bones);
			bool valid = true;
			std::vector<SkeletonBone> bones;
			for (unsigned int k = 0; k < num_bones; k++) {
				vr_error = vr::VRInput()->GetBoneName(value, k, raw_name, vr::k_unMaxBoneNameLength);

				glm::vec3 p = glm::vec3(raw_bones[k].position.v[0], raw_bones[k].position.v[1], raw_bones[k].position.v[2]);
				glm::quat o = glm::quat(raw_bones[k].orientation.w, raw_bones[k].orientation.x, raw_bones[k].orientation.y, raw_bones[k].orientation.z);
				valid &= fabs((glm::dot(o, o) - 1.0f)) < 1e-4; // quaternion is a rotation (zero quaterions occur with invalid data)
				if (!valid) {
					break;
				}
				int parent = parent_indices[k];
				std::string name = std::string(raw_name);
				bones.emplace_back(name, parent, p, o);
				skeleton_bone_id[key][name] = k;
				//printf("Bones %d (%s): parent = %d  pos: %f,%f,%f, rot %f,%f,%f,%f\n", k, raw_name, parent, p.x, p.y, p.z, o.x, o.y, o.z, o.w);
			}

			if (!valid) {
				break;
			}
			skeleton_values[key] = bones;
			//Get the base pose of the skeleton
			vr::VRInput()->GetPoseActionDataRelativeToNow(value, vr::TrackingUniverseStanding, 0.0f, &pose, sizeof(pose), vr::k_ulInvalidInputValueHandle);
			pose_values[key] = ConvertSteamVRMatrixToMat4(pose.pose.mDeviceToAbsoluteTracking);

			free(raw_bones);
			free(parent_indices);
			free(raw_name);
		}
		if (!skeleton.bActive) {
			//printf("Skeleton not active\n");
		}
	}

}

bool OpenXRPlugin::BInit() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		printf("%s - SDL could not initialize! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	// Loading the SteamVR Runtime
	vr::EVRInitError eError = vr::VRInitError_None;
	m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

	if (eError != vr::VRInitError_None) {
		m_pHMD = NULL;
		char buf[1024];
		sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);
		return false;
	}
	
	
	SetupCameras();

	if (!BInitCompositor()) {
		printf("%s - Failed to initialize VR Compositor!\n", __FUNCTION__);
		return false;
	}

	//printf("Running bindings!\n");
	

	//vr::VRInput()->SetActionManifestPath(Path_MakeAbsolute("./hellovr_actions.json", Path_StripFilename(Path_GetExecutablePath())).c_str());
	//vr::VRInput()->GetActionSetHandle("/actions/demo", &action_set);
	
	//vr::VRInput()->SetActionManifestPath(actions_path.c_str());

	std::string full_path = std::filesystem::absolute(action_file).string();
	//printf("%s - > %s\n", action_file.c_str(), full_path.c_str());
	vr::EVRInputError binding_error = vr::VRInput()->SetActionManifestPath(full_path.c_str()); // load the actions into openXR

	std::string file_text = loadTextFile(full_path);// also load the file here, so we can parse it to bind the actions automatically
	Variant action_json = Variant::parseJSON(file_text); 
	//action_json.printFormatted();
	std::vector<Variant> actions = action_json["actions"].getVariantArray();
	
	std::unordered_set<std::string> dedupe_action_sets;
	for (Variant& action : actions) {
		std::string name = action["name"].getString();
		std::string type = action["type"].getString();
		int pos = 0;
		int num_slash = 0;
		while (num_slash < 3 && pos < name.length()) {
			if (name.at(pos) == '/') {
				num_slash++;
			}
			pos++;
		}
		// make sure the action set grouping is known so it can be turned on
		std::string set_name = name.substr(0, pos-1);
		dedupe_action_sets.insert(set_name);
		if (type == "boolean") {
			printf("Set: %s   Name: %s  Type: bool\n", set_name.c_str(), name.c_str());
			vr::VRInput()->GetActionHandle(name.c_str(), &boolean_actions[name]);
		}
		else if (type == "vector1" || type == "vector2" || type == "vector3") {
			printf("Set: %s   Name: %s  Type: vector\n", set_name.c_str(), name.c_str());
			vr::VRInput()->GetActionHandle(name.c_str(), &vector_actions[name]);
		}
		else if (type == "pose") {
			printf("Set: %s   Name: %s  Type: pose\n", set_name.c_str(), name.c_str());
			vr::VRInput()->GetActionHandle(name.c_str(), &pose_actions[name]);
		}
		else if (type == "vibration") {
			printf("Set: %s   Name: %s  Type: haptic\n", set_name.c_str(), name.c_str());
			vr::VRInput()->GetActionHandle(name.c_str(), &haptic_actions[name]);
		}
		else if (type == "skeleton") {
			printf("Set: %s   Name: %s  Type: skeleton\n", set_name.c_str(), name.c_str());
			vr::VRInput()->GetActionHandle(name.c_str(), &skeleton_actions[name]);
		}else{
			printf("Error parsing openXR action file! Unknown type: %s\n", type.c_str());
		}

	}
	for (const std::string& action_set_name : dedupe_action_sets) {
		binding_error = vr::VRInput()->GetActionSetHandle(action_set_name.c_str(), &action_sets[action_set_name]);
		printf("Action Set enabled: %s\n", action_set_name.c_str());
	}

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Initialize Compositor. Returns true if the compositor was
//          successfully initialized, false otherwise.
//-----------------------------------------------------------------------------
bool OpenXRPlugin::BInitCompositor() {
	vr::EVRInitError peError = vr::VRInitError_None;

	if (!vr::VRCompositor()) {
		printf("Compositor initialization failed.\n");
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void OpenXRPlugin::SetupCameras() {
	glm::mat4 y_flip = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)); // Vulkan takes flipped y but we don't want that
	m_mat4ProjectionLeft = y_flip * GetHMDMatrixProjectionEye(vr::Eye_Left) ;
	m_mat4ProjectionRight = y_flip * GetHMDMatrixProjectionEye(vr::Eye_Right);
	m_mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left) ;
	m_mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right) ;
	m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);
}
/*
bool OpenXRPlugin::SetupStereoRenderTargets() {
	if (!m_pHMD)
		return false;

	left_eye.reset(new RendererPlugin::RenderTarget(Taggable::getTag(VR_VIEW)));
	right_eye.reset(new RendererPlugin::RenderTarget(Taggable::getTag(VR_VIEW)));

	m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);
	left_eye->viewport_width = m_nRenderWidth;
	left_eye->viewport_height = m_nRenderHeight;
	right_eye->viewport_width = m_nRenderWidth;
	right_eye->viewport_height = m_nRenderHeight;
	left_eye->multi_sample = ENABLE_MULTISAMPLING;
	right_eye->multi_sample = ENABLE_MULTISAMPLING;
	RendererPlugin::createFrameBuffer(left_eye.get());
	RendererPlugin::createFrameBuffer(right_eye.get());

	return true;
}
*/

std::pair<int,int> OpenXRPlugin::getStereoTargetResolution(){
	m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);
	return { m_nRenderWidth, m_nRenderHeight} ;
}

void OpenXRPlugin::setStereoTargets(std::shared_ptr<RenderTarget> left, std::shared_ptr<RenderTarget> right){
	left_eye_target = left ;
	right_eye_target = right ;

	left_eye_target->near = near_clip ;
	left_eye_target->far = far_clip;
	right_eye_target->near = near_clip;
	right_eye_target->far = far_clip;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a Matrix Projection Eye with respect to nEye.
//-----------------------------------------------------------------------------
glm::mat4 OpenXRPlugin::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye) {
	if (!m_pHMD)
		return glm::mat4(1.0f);

	vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, near_clip, far_clip);

	return glm::mat4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

//-----------------------------------------------------------------------------
// Purpose: Gets an HMDMatrixPoseEye with respect to nEye.
//-----------------------------------------------------------------------------
glm::mat4 OpenXRPlugin::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye) {
	if (!m_pHMD)
		return glm::mat4();

	vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform(nEye);
	glm::mat4 matrixObj(
		matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
	);

	return glm::inverse(matrixObj);
}

//-----------------------------------------------------------------------------
// Purpose: Gets a Current View Projection Matrix with respect to nEye,
//          which may be an Eye_Left or an Eye_Right.
//-----------------------------------------------------------------------------
glm::mat4 OpenXRPlugin::GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye) {
	glm::mat4 matMVP(1.0f);
	if (nEye == vr::Eye_Left) {
		matMVP = m_mat4ProjectionLeft * m_mat4eyePosLeft * inverse_head_pose;
	}
	else if (nEye == vr::Eye_Right) {
		matMVP = m_mat4ProjectionRight * m_mat4eyePosRight * inverse_head_pose;
	}

	return matMVP;
}

void OpenXRPlugin::UpdateHMDMatrixPose() {
	if (!m_pHMD)
		return;

	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);
	//vr::VRCompositor()->GetLastPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	m_iValidPoseCount = 0;
	m_strPoseClasses = "";
	for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice) {
		if (m_rTrackedDevicePose[nDevice].bPoseIsValid) {
			m_iValidPoseCount++;
			m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToMat4(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
			if (m_rDevClassChar[nDevice] == 0) {
				switch (m_pHMD->GetTrackedDeviceClass(nDevice)) {
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {

		glm::mat4 raw_pose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];// TODO why is this coming back with flipped y movement?
		
/*
		raw_pose[1][0] *=-1;
		raw_pose[1][1] *= -1;
		raw_pose[1][2] *= -1;
		raw_pose[3][1] *= -1;
*/
		//glm::mat4 y_flip = glm::scale(glm::mat4(1), glm::vec3(1,-1,1)); 
		//head_pose = y_flip*raw_pose*y_flip ;
		head_pose = raw_pose ;
		inverse_head_pose = glm::inverse(head_pose);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Converts a SteamVR matrix to our local matrix class
//-----------------------------------------------------------------------------
glm::mat4 OpenXRPlugin::ConvertSteamVRMatrixToMat4(const vr::HmdMatrix34_t& matPose) {
	
	glm::mat4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
	);

	return matrixObj;
}


// Returns the current value a single valued action(like a trigger)
float OpenXRPlugin::getValue(std::string action_name) {
	return vector_values[action_name].x;
}

// Returns the current value a vector action (like a joystick)
glm::vec2 OpenXRPlugin::getVector(std::string action_name) {
	return glm::vec2(vector_values[action_name].x, vector_values[action_name].y);
}

// Returns the value of a boolean action (like a button press)
bool OpenXRPlugin::getBoolean(std::string action_name) {
	return boolean_values[action_name];
}

// Returns the value of a pose action
glm::mat4 OpenXRPlugin::getPose(std::string action_name) {
	return pose_values[action_name];
}

// Returns the total transformation of a skeleton bone from the skeleton base
glm::mat4 OpenXRPlugin::getSkeletonBoneTransform(const std::string& skeleton_name, const std::string& bone_name) {

	if (skeleton_bone_id.find(skeleton_name) == skeleton_bone_id.end()) {
		/*printf("skeleton '%s' not found in skeleton bones!\n", skeleton_name.c_str());
		for (auto& [key, value] : skeleton_bone_id) {
			printf("option: '%s'\n", key.c_str());
		}
		*/
		return glm::mat4(1.0f);
	}

	if (skeleton_bone_id[skeleton_name].find(bone_name) == skeleton_bone_id[skeleton_name].end()) {
		/*printf("skeleton '%s' doesn't have bone '%s'!\n", skeleton_name.c_str(), bone_name.c_str());
		for (auto& [key, value] : skeleton_bone_id[skeleton_name]) {
			printf("option: '%s'\n", key.c_str());
		}
		*/
		return glm::mat4(1.0f);
	}

	int bone_index = skeleton_bone_id[skeleton_name][bone_name];

	return getSkeletonBoneTransform(skeleton_name, bone_index) ;
}

glm::mat4 OpenXRPlugin::getSkeletonBoneTransform(const std::string& skeleton_name, int bone_index) {

	//printf("bone index: %d\n", bone_index);

	glm::mat4 transform = glm::mat4(1);
	
	while (bone_index >= 0) {
		
		SkeletonBone& bone = skeleton_values[skeleton_name][bone_index];

		//printf("got bone data: %f,%f,%f\n", bone.position.x, bone.position.y, bone.position.z);
		
		transform =  glm::mat4_cast(bone.orientation) * transform ; // TODO check order of these multiplications
		transform = glm::translate(glm::mat4(1.0f), bone.position) * transform; //TODO check order of these two lines
		
		bone_index = bone.parent;
	}
	transform = pose_values[skeleton_name] * transform ;
	return transform;
}


int OpenXRPlugin::getSkeletonBoneIndex(const std::string& skeleton_name, const std::string& bone_name){
	return skeleton_bone_id[skeleton_name][bone_name];
}

int OpenXRPlugin::getSkeletonBoneParentIndex(const std::string& skeleton_name, const std::string& bone_name){

	int bone_index = skeleton_bone_id[skeleton_name][bone_name];
	SkeletonBone& bone = skeleton_values[skeleton_name][bone_index];
	return bone.parent ;
}

// Returns the local orientation of a skeleton bone from the skeleton base
glm::quat OpenXRPlugin::getSkeletonLocalOrientation(const std::string& skeleton_name, const std::string& bone_name) {
	if (skeleton_bone_id.find(skeleton_name) == skeleton_bone_id.end()) {
		/*printf("skeleton '%s' not found in skeleton bones!\n", skeleton_name.c_str());
		for (auto& [key, value] : skeleton_bone_id) {
			printf("option: '%s'\n", key.c_str());
		}
		*/
		return glm::mat4(1.0f);
	}

	if (skeleton_bone_id[skeleton_name].find(bone_name) == skeleton_bone_id[skeleton_name].end()) {
		/*printf("skeleton '%s' doesn't have bone '%s'!\n", skeleton_name.c_str(), bone_name.c_str());
		for (auto& [key, value] : skeleton_bone_id[skeleton_name]) {
			printf("option: '%s'\n", key.c_str());
		}
		*/
		return glm::mat4(1.0f);
	}

	int bone_index = skeleton_bone_id[skeleton_name][bone_name];
	SkeletonBone& bone = skeleton_values[skeleton_name][bone_index];
	return bone.orientation;
}

// Returns the current pose of the HMD
glm::mat4 OpenXRPlugin::getHeadPose() {
	return head_pose;
}

// Returns the current pose of the HMD
glm::mat4 OpenXRPlugin::getInverseHeadPose() {
	return inverse_head_pose;
}

void OpenXRPlugin::setBackgroundColor(glm::vec3 color) {
	if (ENABLED && left_eye_target) {
		left_eye_target->clear_values[0] = {color.r,color.g,color.b, 1.0f};
		right_eye_target->clear_values[0] = { color.r,color.g,color.b, 1.0f };
	}
}

void OpenXRPlugin::setHaptic(std::string action_name, float duration, float frequency, float amplitude) {
	vr::VRInput()->TriggerHapticVibrationAction(haptic_actions[action_name], 0, duration, frequency, amplitude, vr::k_ulInvalidInputValueHandle);
}																				