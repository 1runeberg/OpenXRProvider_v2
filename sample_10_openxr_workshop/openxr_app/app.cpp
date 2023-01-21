/* Copyright 2021, 2022, 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
 *
 *  SPDX-License-Identifier: MIT
 *
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#include "app.hpp"

// Callback handling
oxr::XrApp *g_pApp = nullptr;
inline void Callback_GripPose( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	if ( pAction->vecActionStates[ unActionStateIndex ].statePose.isActive )
	{
		if ( unActionStateIndex == 0 )
			g_pApp->bGripLeftIsActive = true;
		else
			g_pApp->bGripRightIsActive = true;
	}
	else
	{
		if ( unActionStateIndex == 0 )
			g_pApp->bGripLeftIsActive = false;
		else
			g_pApp->bGripRightIsActive = false;
	}
}

inline void Callback_AimPose( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	if ( pAction->vecActionStates[ unActionStateIndex ].statePose.isActive )
	{
		if ( unActionStateIndex == 0 )
			g_pApp->bAimLeftIsActive = true;
		else
			g_pApp->bAimRightIsActive = true;
	}
	else
	{
		if ( unActionStateIndex == 0 )
			g_pApp->bAimLeftIsActive = false;
		else
			g_pApp->bAimRightIsActive = false;
	}
}

inline void Callback_Haptic( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	// Check if the action has subpaths
	if ( !pAction->vecSubactionpaths.empty() )
	{
		g_pApp->GetInput()->GenerateHaptic( pAction->xrActionHandle, pAction->vecSubactionpaths[ unActionStateIndex ], g_pApp->unHapticDuration, g_pApp->fHapticAmplitude, g_pApp->fHapticFrequency );
	}
	else
	{
		g_pApp->GetInput()->GenerateHaptic( pAction->xrActionHandle, XR_NULL_PATH, g_pApp->unHapticDuration, g_pApp->fHapticAmplitude, g_pApp->fHapticFrequency );
	}
}

inline void Callback_Empty( oxr::Action *pAction, uint32_t unActionStateIndex ) { return; }

namespace oxr
{
	XrApp::XrApp()
	{
		// Add minimal set of extensions
		vecRequestedExtensions.push_back( XR_KHR_VULKAN_ENABLE_EXTENSION_NAME );
		vecRequestedExtensions.push_back( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME );
		vecRequestedExtensions.push_back( XR_EXT_HAND_TRACKING_EXTENSION_NAME );

		// Add minimal texture formats
#ifdef XR_USE_PLATFORM_ANDROID
		vecRequestedTextureFormats.push_back( VK_FORMAT_R8G8B8A8_SRGB );
#endif

		vecRequestedDepthFormats.push_back( VK_FORMAT_D24_UNORM_S8_UINT );

		// Add minimal set of controllers to the base controller
		baseController.vecSupportedControllers.push_back( &controllerIndex );
		baseController.vecSupportedControllers.push_back( &controllerTouch );
		baseController.vecSupportedControllers.push_back( &controllerVive );
		baseController.vecSupportedControllers.push_back( &controllerMR );
	}

	XrApp::~XrApp() { Cleanup(); }

#ifdef XR_USE_PLATFORM_ANDROID
	XrResult XrApp::Init( struct android_app *app, std::string sAppName, OxrVersion32 unVersion, ELogLevel eLogLevel, bool bCreateRenderer, bool bCreateSession, bool bCreateInput )
#else
	XrResult XrApp::Init( std::string sAppName, OxrVersion32 unVersion, ELogLevel eLogLevel, bool bCreateRenderer, bool bCreateSession, bool bCreateInput )
#endif
	{
		if ( sAppName.empty() || unVersion == 0 )
			return XR_ERROR_INITIALIZATION_FAILED;

		// Set this object's reference for our callbacks
		g_pApp = this;

		// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
		//     You can optionally set a default log level during creation.
		m_pProvider = std::make_unique< oxr::Provider >( eLogLevel );

		// For Android, we need to first initialize the android openxr loader
#ifdef XR_USE_PLATFORM_ANDROID
		if ( !XR_UNQUALIFIED_SUCCESS( m_pProvider->InitAndroidLoader( app ) ) )
		{
			oxr::LogError( LOG_CATEGORY_DEFAULT, "FATAL! Unable to load Android OpenXR Loader in this device!" );
			return XR_ERROR_RUNTIME_UNAVAILABLE;
		}
#endif

		// (2) Filter out unsupported extensions
		XrResult xrResult = m_pProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
		//     including the supported extensions we found earlier
		oxr::AppInstanceInfo oxrAppInstanceInfo {};
		oxrAppInstanceInfo.sAppName = sAppName;
		oxrAppInstanceInfo.unAppVersion = unVersion;
		oxrAppInstanceInfo.sEngineName = "openxr_provider";
		oxrAppInstanceInfo.unEngineVersion = OXR_MAKE_VERSION32( PROVIDER_VERSION_MAJOR, PROVIDER_VERSION_MINOR, PROVIDER_VERSION_PATCH );
		oxrAppInstanceInfo.vecInstanceExtensions = vecRequestedExtensions;

		// (4) Initialize the provider - this will create an openxr instance with the current default openxr runtime.
		//     If there's no runtime or something has gone wrong, the provider will return an error code
		//			from https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#return-codes
		//     Notice that you can also use native openxr data types (e.g. XrResult) directly

		xrResult = m_pProvider->Init( &oxrAppInstanceInfo );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (5) Setup the vulkan renderer and create an openxr graphics binding
		if ( !bCreateRenderer )
			return XR_SUCCESS;

#ifdef XR_USE_PLATFORM_ANDROID
		m_pRender = new xrvk::Render( m_pProvider->Instance()->androidActivity->assetManager, xrvk::ELogLevel::LogVerbose );
#else
		m_pRender = new xrvk::Render( xrvk::ELogLevel::LogVerbose );
#endif

		xrResult =
			m_pRender->Init( m_pProvider.get(), oxrAppInstanceInfo.sAppName.c_str(), oxrAppInstanceInfo.unAppVersion, oxrAppInstanceInfo.sEngineName.c_str(), oxrAppInstanceInfo.unEngineVersion );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (6) Create an openxr session
		if ( !bCreateSession )
			return XR_SUCCESS;

		xrResult = m_pProvider->CreateSession( m_pRender->GetVulkanGraphicsBinding() );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		m_pSession = m_pProvider->Session();

		// (6.1) Get any extensions that requires an active openxr instance and session
		m_extHandTracking = static_cast< oxr::ExtHandTracking * >( m_pProvider->Instance()->extHandler.GetExtension( XR_EXT_HAND_TRACKING_EXTENSION_NAME ) );
		if ( m_extHandTracking )
		{
			if ( !XR_UNQUALIFIED_SUCCESS( m_extHandTracking->Init() ) )
			{
				delete m_extHandTracking;
				m_extHandTracking = nullptr;
			}
		}

		// (7) Create swapchains for rendering
		oxr::TextureFormats selectedTextureFormats { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
		xrResult = m_pProvider->Session()->CreateSwapchains( &selectedTextureFormats, vecRequestedTextureFormats, vecRequestedDepthFormats );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (8) Prepare vulkan resources for rendering - this creates the command buffers and render passes

		// (8.1) Set texture extent
		VkExtent2D vkExtent;
		vkExtent.width = m_pProvider->Session()->GetSwapchains()[ 0 ].unWidth;
		vkExtent.height = m_pProvider->Session()->GetSwapchains()[ 0 ].unHeight;

		// (8.2) Initialize render resources
		m_pRender->CreateRenderResources( m_pSession, selectedTextureFormats.vkColorTextureFormat, selectedTextureFormats.vkDepthTextureFormat, vkExtent );

		// (8.3) Set vismask if present
		oxr::ExtVisMask *pVisMask = static_cast< oxr::ExtVisMask * >( m_pProvider->Instance()->extHandler.GetExtension( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) );

		if ( pVisMask )
		{
			m_pRender->CreateVisMasks( 2 );

			pVisMask->GetVisMask(
				m_pRender->GetVisMasks()[ 0 ].vertices, m_pRender->GetVisMasks()[ 0 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );

			pVisMask->GetVisMask(
				m_pRender->GetVisMasks()[ 1 ].vertices, m_pRender->GetVisMasks()[ 1 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
		}

		// (8.6) Optional - Set skybox (set m_pRender->skybox->bIsVisible = false if not needed)
		m_pRender->skybox->currentScale = { 5.0f, 5.0f, 5.0f };
		m_pRender->skybox->bApplyOffset = true;
		m_pRender->skybox->offsetRotation = { 0.0f, 0.0f, 1.0f, 0.0f }; // rotate 180 degrees in z (roll)

		// (9) Set other platform callbacks
#ifdef XR_USE_PLATFORM_ANDROID
		m_pProvider->Instance()->androidApp->onAppCmd = app_handle_cmd;
#endif

		// (10) Setup input
		if ( !bCreateInput )
			return XR_SUCCESS;

		// (10.1) Retrieve input object from provider - this is created during provider init()
		m_pInput = m_pProvider->Input();
		if ( m_pInput == nullptr || m_pProvider->Session() == nullptr )
		{
			oxr::LogError( LOG_CATEGORY_DEFAULT, "Error getting input object from the provider library or init failed." );
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		// (10.2) Create actionset(s) - these are collections of actions that can be activated based on game state
		//								(e.g. one actionset for locomotion and another for ui handling)
		//								you can optionally provide a priority and other info (e.g. via extensions)
		m_pActionsetMain = new oxr::ActionSet();
		m_pInput->CreateActionSet( m_pActionsetMain, "main", "main actions" );

		// (10.3) Create action(s) - these represent actions that will be triggered based on hardware state from the openxr runtime
		m_pActionGripPose = new Action( XR_ACTION_TYPE_POSE_INPUT, &Callback_GripPose );
		m_pInput->CreateAction( m_pActionGripPose, m_pActionsetMain, "pose_grip", "controller grip pose", { "/user/hand/left", "/user/hand/right" } );

		m_pActionAimPose = new Action( XR_ACTION_TYPE_POSE_INPUT, &Callback_AimPose );
		m_pInput->CreateAction( m_pActionAimPose, m_pActionsetMain, "pose_aim", "controller aim pose", { "/user/hand/left", "/user/hand/right" } );

		m_pActionHaptic = new Action( XR_ACTION_TYPE_VIBRATION_OUTPUT, &Callback_Haptic );
		m_pInput->CreateAction( m_pActionHaptic, m_pActionsetMain, "haptic", "play controller haptics", { "/user/hand/left", "/user/hand/right" } );

		// (10.4) Create action to controller bindings
		//		  The use of the "BaseController" here is optional. It's a convenience controller handle that will auto-map
		//		  basic controller components to every supported controller it knows of.
		//
		//		  Alternatively (or in addition), you can directly add bindings per controller
		//		  e.g. controllerVive.AddBinding(...)

		// pose - hmd

		// poses - controllers (grip)
		m_pInput->AddBinding( &baseController, m_pActionGripPose->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::GripPose, Controller::Qualifier::None );
		m_pInput->AddBinding( &baseController, m_pActionGripPose->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::GripPose, Controller::Qualifier::None );

		// poses - controllers (aim)
		m_pInput->AddBinding( &baseController, m_pActionAimPose->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::AimPose, Controller::Qualifier::None );
		m_pInput->AddBinding( &baseController, m_pActionAimPose->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::AimPose, Controller::Qualifier::None );

		// haptics
		m_pInput->AddBinding( &baseController, m_pActionHaptic->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::Haptic, Controller::Qualifier::None );
		m_pInput->AddBinding( &baseController, m_pActionHaptic->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::Haptic, Controller::Qualifier::None );

		// (11) Initialize input with session - required for succeeding calls
		m_pInput->Init( m_pSession );

		// (12) Create actionspaces for default pose actions (if present)
		XrPosef poseInSpace;
		XrPosef_Identity( &poseInSpace );

		if ( m_pActionGripPose )
			m_pInput->CreateActionSpaces( m_pActionGripPose, &poseInSpace );

		if ( m_pActionAimPose )
			m_pInput->CreateActionSpaces( m_pActionAimPose, &poseInSpace );

		return XR_SUCCESS;
	}

	void XrApp::Cleanup()
	{
		if ( m_pActionGripPose )
			delete m_pActionGripPose;

		if ( m_pActionAimPose )
			delete m_pActionAimPose;

		if ( m_pActionHaptic )
			delete m_pActionHaptic;

		if ( m_pActionsetMain )
			delete m_pActionsetMain;
	}

	XrVector3f XrApp::GetBackVector( XrQuaternionf *quat )
	{
		XrVector3f v {};
		v.x = 2 * ( quat->x * quat->z + quat->w * quat->y );
		v.y = 2 * ( quat->y * quat->z - quat->w * quat->x );
		v.z = 1 - 2 * ( quat->x * quat->x + quat->y * quat->y );

		return v;
	}

	XrVector3f XrApp::GetUpVector( XrQuaternionf *quat )
	{
		XrVector3f v {};
		v.x = 2 * ( quat->x * quat->y - quat->w * quat->z );
		v.y = 1 - 2 * ( quat->x * quat->x + quat->z * quat->z );
		v.z = 2 * ( quat->y * quat->z + quat->w * quat->x );

		return v;
	}

	XrVector3f XrApp::GetRightVector( XrQuaternionf *quat )
	{
		XrVector3f v {};
		v.x = 1 - 2 * ( quat->y * quat->y + quat->z * quat->z );
		v.y = 2 * ( quat->x * quat->y + quat->w * quat->z );
		v.z = 2 * ( quat->x * quat->z - quat->w * quat->y );

		return v;
	}

	XrVector3f XrApp::FlipVector( XrVector3f *vec )
	{
		XrVector3f v { -1.f, -1.f, -1.f };
		v.x *= vec->x;
		v.y *= vec->y;
		v.z *= vec->z;

		return v;
	}

	XrVector3f XrApp::GetForwardVector( XrQuaternionf *quat )
	{
		XrVector3f v = GetBackVector( quat );
		return FlipVector( &v );
	}

	XrVector3f XrApp::GetDownVector( XrQuaternionf *quat ) 
	{
		XrVector3f v = GetUpVector( quat );
		return FlipVector( &v );
	}

	XrVector3f XrApp::GetLeftVector( XrQuaternionf *quat ) 
	{
		XrVector3f v = GetRightVector( quat );
		return FlipVector( &v );
	}

	void XrApp::AddDebugMeshes()
	{
		m_bDrawDebug = true;

		// (1) Define debug shape
		m_debugShape.vecIndices = &g_vecPyramidIndices;
		m_debugShape.vecVertices = &g_vecPyramidVertices;

		// (2) Create graphics pipeline for the debug shapes
		m_pRender->PrepareShapesPipeline( &m_debugShape, "shaders/shape.vert.spv", "shaders/shape.frag.spv" );

		// (3) Hand tracking
		if ( m_extHandTracking )
		{
			// (3.1) a cube per joint per hand - we'll match the indices with the hand tracking extension's
			// so we can easily refer to them later on to update the current tracked poses
			uint32_t unTotalHandJoints = XR_HAND_JOINT_COUNT_EXT * 2;
			m_pRender->vecShapes.resize( unTotalHandJoints );

			// (3.2) zero out the scale so cubes won't immediately appear until after the first frame of poses come in
			Shapes::Shape debugShapeJoint = m_debugShape;
			debugShapeJoint.scale = { 0.0f, 0.0f, 0.0f };

			// (3.3) left hand will use the first XR_HAND_JOINT_COUNT_EXT indices
			// right hand will use specHandJointIndex + XR_HAND_JOINT_COUNT_EXT indices
			for ( uint32_t i = 0; i < unTotalHandJoints; i++ )
			{
				m_pRender->vecShapes[ i ] = debugShapeJoint.Duplicate();
			}
		}

		// (4) Controllers: Add debug shapes and define action spaces for them
		XrVector3f controllerScale { 0.05f, 0.05f, 0.08f };
		m_pRender->vecShapes[ m_pRender->AddShape( m_debugShape.Duplicate(), controllerScale ) ]->space = m_pActionGripPose->vecActionSpaces[ 0 ];
		m_pRender->vecShapes[ m_pRender->AddShape( m_debugShape.Duplicate(), controllerScale ) ]->space = m_pActionGripPose->vecActionSpaces[ 1 ];

		XrVector3f laserScale { 0.0075f, 0.0075f, 0.1f };
		m_pRender->vecShapes[ m_pRender->AddShape( m_debugShape.Duplicate(), laserScale ) ]->space = m_pActionAimPose->vecActionSpaces[ 0 ];
		m_pRender->vecShapes[ m_pRender->AddShape( m_debugShape.Duplicate(), laserScale ) ]->space = m_pActionAimPose->vecActionSpaces[ 1 ];
	}

} // namespace oxr
