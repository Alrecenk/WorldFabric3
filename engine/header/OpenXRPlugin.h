#ifndef _OPEN_XR_PLUGIN_H_
#define _OPEN_XR_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "VulkanPlugin.h"

#include "SDL3/SDL.h"
#include "glew.h"
#include "SDL3/SDL_opengl.h"
#include "openvr.h"
#include "glm/glm.hpp"
#include "Utilities.h"

#include <GL/glu.h>
#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_set>




//---------------------------------------------------------------------------------------------------------------------
// Purpose: Returns true if the action is active and had a rising edge
//---------------------------------------------------------------------------------------------------------------------
inline bool GetDigitalActionRisingEdge(vr::VRActionHandle_t action, vr::VRInputValueHandle_t* pDevicePath = nullptr)
{
	vr::InputDigitalActionData_t actionData;
	vr::VRInput()->GetDigitalActionData(action, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	if (pDevicePath)
	{
		*pDevicePath = vr::k_ulInvalidInputValueHandle;
		if (actionData.bActive)
		{
			vr::InputOriginInfo_t originInfo;
			if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(actionData.activeOrigin, &originInfo, sizeof(originInfo)))
			{
				*pDevicePath = originInfo.devicePath;
			}
		}
	}
	return actionData.bActive && actionData.bChanged && actionData.bState;
}


//---------------------------------------------------------------------------------------------------------------------
// Purpose: Returns true if the action is active and had a falling edge
//---------------------------------------------------------------------------------------------------------------------
inline bool GetDigitalActionFallingEdge(vr::VRActionHandle_t action, vr::VRInputValueHandle_t* pDevicePath = nullptr)
{
	vr::InputDigitalActionData_t actionData;
	vr::VRInput()->GetDigitalActionData(action, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	if (pDevicePath)
	{
		*pDevicePath = vr::k_ulInvalidInputValueHandle;
		if (actionData.bActive)
		{
			vr::InputOriginInfo_t originInfo;
			if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(actionData.activeOrigin, &originInfo, sizeof(originInfo)))
			{
				*pDevicePath = originInfo.devicePath;
			}
		}
	}
	return actionData.bActive && actionData.bChanged && !actionData.bState;
}


//---------------------------------------------------------------------------------------------------------------------
// Purpose: Returns true if the action is active and its state is true
//---------------------------------------------------------------------------------------------------------------------
inline bool GetDigitalActionState(vr::VRActionHandle_t action, vr::VRInputValueHandle_t* pDevicePath = nullptr)
{
	vr::InputDigitalActionData_t actionData;
	vr::VRInput()->GetDigitalActionData(action, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	if (pDevicePath)
	{
		*pDevicePath = vr::k_ulInvalidInputValueHandle;
		if (actionData.bActive)
		{
			vr::InputOriginInfo_t originInfo;
			if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(actionData.activeOrigin, &originInfo, sizeof(originInfo)))
			{
				*pDevicePath = originInfo.devicePath;
			}
		}
	}
	return actionData.bActive && actionData.bState;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to get a string from a tracked device property and turn it
//			into a string
//-----------------------------------------------------------------------------
inline std::string GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError = NULL) {
	uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
	if (unRequiredBufferLen == 0)
		return "";

	char* pchBuffer = new char[unRequiredBufferLen];
	unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
	std::string sResult = pchBuffer;
	delete[] pchBuffer;
	return sResult;
}


class OpenXRPlugin : public AsyncPlugin{

public:

	static inline std::string tag = "openXRLink";
	static inline std::string VR_VIEW = "vr_view";
	static inline bool ENABLE_MULTISAMPLING = true; 
	static inline bool ENABLED = false; // Needs to be set before the first openXR is initialized

	std::shared_ptr<RenderTarget> left_eye_target;
	std::shared_ptr<RenderTarget> right_eye_target;

	class SkeletonBone {
	public:
		std::string name;
		int parent;
		glm::vec3 position;
		glm::quat orientation;
	};

	// Boots SteamVR and sets up openGL and links to controllers and other hardware
	OpenXRPlugin(std::string action_file_path);


	// Called on every plug-in before any plug-ins are run
	// Adds an XRStatus object with the tag "xr_status" contain data other plugins can use to interact with the headset
	void initialize() override;

	void run() override;


	// Returns the current value a single valued action(like a trigger)
	float getValue(std::string action_name);

	// Returns the current value a vector action (like a joystick)
	glm::vec2 getVector(std::string action_name);

	// Returns the value of a boolean action (like a button press)
	bool getBoolean(std::string action_name);

	// Returns the total transformation of a skeleton bone from the skeleton base
	glm::mat4 getSkeletonBoneTransform(const std::string& skeleton_name, const std::string& bone_name);

	glm::mat4 getSkeletonBoneTransform(const std::string& skeleton_name, int bone_index) ;

	int getSkeletonBoneIndex(const std::string& skeleton_name, const std::string& bone_name) ;

	int getSkeletonBoneParentIndex(const std::string& skeleton_name, const std::string& bone_name);

	// Returns the local orientation of a skeleton bone from the skeleton base
	glm::quat getSkeletonLocalOrientation(const std::string& skeleton_name, const std::string& bone_name);

	// Returns the value of a pose action
	glm::mat4 getPose(std::string action_name);

	// Returns the current pose of the HMD
	glm::mat4 getHeadPose();

	// Returns the inverse of the current pose of the HMD
	glm::mat4 getInverseHeadPose();

	//Blocking function that updates VR poses
	void UpdateHMDMatrixPose();

	void setBackgroundColor(glm::vec3 color);

	void setHaptic(std::string action_name, float duration, float frequency, float amplitude);


	std::pair<int, int> getStereoTargetResolution();

	void setStereoTargets(std::shared_ptr<RenderTarget> left, std::shared_ptr<RenderTarget> right);

private:

	std::string action_file;

	vr::IVRSystem* m_pHMD;
	std::string m_strDriver;
	std::string m_strDisplay;
	vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	glm::mat4 m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];

	
	vr::VRActionSetHandle_t action_set = vr::k_ulInvalidActionSetHandle;

	vr::VRActionHandle_t action_press_a = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_press_b = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_press_x = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_press_y = vr::k_ulInvalidActionHandle;
	

	vr::VRActionHandle_t action_left_stick_move = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_right_stick_move = vr::k_ulInvalidActionHandle;

	vr::VRActionHandle_t action_left_pose = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_right_pose = vr::k_ulInvalidActionHandle;

	vr::VRActionHandle_t action_left_haptic = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_right_haptic = vr::k_ulInvalidActionHandle;

	vr::VRActionHandle_t action_left_trigger_pull = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_right_trigger_pull = vr::k_ulInvalidActionHandle;

	vr::VRActionHandle_t action_left_grip_pull = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t action_right_grip_pull = vr::k_ulInvalidActionHandle;


	std::map<std::string, vr::VRActionSetHandle_t> action_sets;
	std::map<std::string, vr::VRActionHandle_t> boolean_actions;
	std::map<std::string, vr::VRActionHandle_t> haptic_actions;
	std::map<std::string, vr::VRActionHandle_t> pose_actions;
	std::map<std::string, vr::VRActionHandle_t> vector_actions;
	std::map<std::string, vr::VRActionHandle_t> skeleton_actions;

	std::map<std::string, bool> boolean_values;
	std::map<std::string, glm::vec3> vector_values;
	std::map<std::string, glm::mat4> pose_values;
	//TODO haptic setting values

	std::map<std::string, std::vector<SkeletonBone>> skeleton_values;

	std::map<std::string, std::map < std::string, int>> skeleton_bone_id; // maps a skeleton name and bone name to and index in skeleton_bone

	struct ControllerInfo_t
	{
		vr::VRInputValueHandle_t m_source = vr::k_ulInvalidInputValueHandle;
		vr::VRActionHandle_t m_actionPose = vr::k_ulInvalidActionHandle;
		vr::VRActionHandle_t m_actionHaptic = vr::k_ulInvalidActionHandle;
		glm::mat4 m_rmat4Pose;
		bool m_bShowController = false;
	};

	enum EHand
	{
		Left = 0,
		Right = 1,
	};
	ControllerInfo_t m_rHand[2];


	// SDL bookkeeping
	
	//SDL_Window* m_pCompanionWindow = nullptr;
	//SDL_GLContext m_pContext = NULL;
	

	// OpenGL bookkeeping
	int m_iTrackedControllerCount;
	int m_iTrackedControllerCount_Last;
	int m_iValidPoseCount;
	int m_iValidPoseCount_Last;
	bool m_bShowCubes;
	glm::vec2 m_vAnalogValue;

	std::string m_strPoseClasses;                            // what classes we saw poses for this frame
	char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];   // for each device, a character representing its class

	float near_clip = 0.15f;
	float far_clip = 1000.0f;

	glm::mat4 m_mat4ProjectionLeft;

	glm::mat4 head_pose = glm::mat4(1.0f);
	glm::mat4 inverse_head_pose;
	glm::mat4 m_mat4eyePosLeft;
	glm::mat4 m_mat4eyePosRight;

	glm::mat4 m_mat4ProjectionCenter;
	glm::mat4 m_mat4ProjectionRight;


	uint32_t m_nRenderWidth;
	uint32_t m_nRenderHeight;

		

	bool BInit();

	bool BInitCompositor();

	//TODO void Shutdown();

	//bool SetupStereoRenderTargets();



	void SetupCameras();

	glm::mat4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
	glm::mat4 GetHMDMatrixPoseEye(vr::Hmd_Eye nEye);
	glm::mat4 GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye);

	

	glm::mat4 ConvertSteamVRMatrixToMat4(const vr::HmdMatrix34_t& matPose);

};
#endif // #ifndef _OPEN_XR_PLUGIN_H_
