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

#include "main.hpp"

/**
 * This is the application openxr flow and is numbered
 * with distinct steps that correlates to the openxr
 * application lifecycle.
 */
#ifdef XR_USE_PLATFORM_ANDROID
XrResult demo_openxr_start( struct android_app *app )
#else
XrResult demo_openxr_start()
#endif
{
	// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
	//     You can optionally set a default log level during creation.
	std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

	// For Android, we need to first initialize the android openxr loader
#ifdef XR_USE_PLATFORM_ANDROID
	if ( !XR_UNQUALIFIED_SUCCESS( oxrProvider->InitAndroidLoader( app ) ) )
	{
		oxr::LogError( LOG_CATEGORY_DEMO, "FATAL! Unable to load Android OpenXR Loader in this device!" );
		return XR_ERROR_RUNTIME_UNAVAILABLE;
	}
#endif

	// (2) Check requested extensions against supported extensions from the current runtime
	//     We will use this later when initializing an openxr instance
	//     In this sample, we'll try to check for and enable some useful extensions if available:
	//     https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_visibility_mask
	//	   https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_composition_layer_depth
	//     https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_dpad_binding
	//
	// 	   If you do not specify a vulkan graphics api extension, then the provider will choose the best one
	//     from what's available from the runtime. In this case, you can use GetCurrentVulkanExt() check which
	//
	//	   You can also manually request available runtime extensions with GetSupportedExtensions()
	//     Similar functions are available for api layers

	std::vector< const char * > vecRequestedExtensions {
		// These extensions, being KHR, are very likely to be supported by most major runtimes
		XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,   // this is our preferred graphics extension (only vulkan is supported)
		XR_KHR_VISIBILITY_MASK_EXTENSION_NAME, // this gives us a stencil mask area that the end user will never see, so we don't need to render to it

		XR_EXT_HAND_TRACKING_EXTENSION_NAME,		// Multi-vendor extension but may have different behaviors/implementations across runtimes
		XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME, // Multi-vendor extension that have limited platform and hardware implementation/support
		"XR_FB_eye_tracking_social",
		XR_FB_PASSTHROUGH_EXTENSION_NAME,		  // Vendor specific extension - not supported on all runtimes
		XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME // Vendor specific extension but has wide runtime support
	};

	oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

	// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
	//     including the supported extensions we found earlier
	oxr::AppInstanceInfo oxrAppInstanceInfo {};
	oxrAppInstanceInfo.sAppName = APP_NAME;
	oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( SAMPLE9_VERSION_MAJOR, SAMPLE9_VERSION_MINOR, SAMPLE9_VERSION_PATCH );
	oxrAppInstanceInfo.sEngineName = ENGINE_NAME;
	oxrAppInstanceInfo.unEngineVersion = OXR_MAKE_VERSION32( PROVIDER_VERSION_MAJOR, PROVIDER_VERSION_MINOR, PROVIDER_VERSION_PATCH );
	oxrAppInstanceInfo.vecInstanceExtensions = vecRequestedExtensions;

	// (4) Initialize the provider - this will create an openxr instance with the current default openxr runtime.
	//     If there's no runtime or something has gone wrong, the provider will return an error code
	//			from https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#return-codes
	//     Notice that you can also use native openxr data types (e.g. XrResult) directly

	XrResult xrResult = oxrProvider->Init( &oxrAppInstanceInfo );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		return xrResult;

	// (4.1) Get any extension that requires an openxr instance only
	g_extEyeGaze = static_cast< oxr::ExtEyeGaze * >( oxrProvider->Instance()->extHandler.GetExtension( XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME ) );
	if ( g_extEyeGaze )
	{
		// Check if current user hardware supports or has an authorized eye gaze tracking hardware we can use
		if ( !XR_UNQUALIFIED_SUCCESS( g_extEyeGaze->Init( oxrProvider->GetOpenXrSystemId() ) ) && !g_extEyeGaze->IsEyeGazeSupported() )
		{
			delete g_extEyeGaze;
			g_extEyeGaze = nullptr;
		}
		else
		{
			oxr::LogDebug( LOG_CATEGORY_DEMO, "Eye tracking extension enabled and active." );
		}
	}

	// (5) Setup the vulkan renderer and create an openxr graphics binding
#ifdef XR_USE_PLATFORM_ANDROID
	g_pRender = std::make_unique< xrvk::Render >( oxrProvider->Instance()->androidActivity->assetManager, xrvk::ELogLevel::LogVerbose );
#else
	g_pRender = std::make_unique< xrvk::Render >( xrvk::ELogLevel::LogVerbose );
#endif

	xrResult = g_pRender->Init( oxrProvider.get(), oxrAppInstanceInfo.sAppName.c_str(), oxrAppInstanceInfo.unAppVersion, oxrAppInstanceInfo.sEngineName.c_str(), oxrAppInstanceInfo.unEngineVersion );

	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		return xrResult;

	// (6) Create an openxr session
	xrResult = oxrProvider->CreateSession( g_pRender->GetVulkanGraphicsBinding() );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		return xrResult;

	g_pSession = oxrProvider->Session();

	// (6.1) Get any extensions that requires an active openxr instance and session
	g_extHandTracking = static_cast< oxr::ExtHandTracking * >( oxrProvider->Instance()->extHandler.GetExtension( XR_EXT_HAND_TRACKING_EXTENSION_NAME ) );
	if ( g_extHandTracking )
	{
		if ( !XR_UNQUALIFIED_SUCCESS( g_extHandTracking->Init() ) )
		{
			delete g_extHandTracking;
			g_extHandTracking = nullptr;
		}
	}

	g_extFBPassthrough = static_cast< oxr::ExtFBPassthrough * >( oxrProvider->Instance()->extHandler.GetExtension( XR_FB_PASSTHROUGH_EXTENSION_NAME ) );
	if ( g_extFBPassthrough && g_pSession->GetAppSpace() != XR_NULL_HANDLE )
	{
		if ( !XR_UNQUALIFIED_SUCCESS( g_extFBPassthrough->Init() ) )
		{
			delete g_extFBPassthrough;
		}
	}

	g_extFBRefreshRate = static_cast< oxr::ExtFBRefreshRate * >( oxrProvider->Instance()->extHandler.GetExtension( XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME ) );
	if ( g_extFBRefreshRate )
	{
		xrResult = g_extFBRefreshRate->Init();

		if ( !XR_UNQUALIFIED_SUCCESS( g_extHandTracking->Init() ) )
		{
			delete g_extFBRefreshRate;
			g_extFBRefreshRate = nullptr;
		}
	}

	// (7) Create swapchains for rendering

	// (7.1) Specify color formats
#ifdef XR_USE_PLATFORM_ANDROID
	std::vector< int64_t > vecRequestedTextureFormats = { VK_FORMAT_R8G8B8A8_SRGB };
#else
	std::vector< int64_t > vecRequestedTextureFormats; // we'll send an empty one and let the provider choose from the runtime's supported color formats
#endif

	// (7.2) Specify depth formats
	std::vector< int64_t > vecRequestedDepthFormats = { VK_FORMAT_D24_UNORM_S8_UINT }; // We need a depth with stencil format for this demo

	// (7.3) Create swapchains - selected texture formats will contain the texture formats selected from the requested ones (provider will choose if no preferences were sent)
	oxr::TextureFormats selectedTextureFormats { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
	xrResult = oxrProvider->Session()->CreateSwapchains( &selectedTextureFormats, vecRequestedTextureFormats, vecRequestedDepthFormats );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		return xrResult;

	// (8) Prepare vulkan resources for rendering - this creates the command buffers and render passes

	// (8.1) Set texture extent
	VkExtent2D vkExtent;
	vkExtent.width = oxrProvider->Session()->GetSwapchains()[ 0 ].unWidth;
	vkExtent.height = oxrProvider->Session()->GetSwapchains()[ 0 ].unHeight;

	// (8.2) Initialize render resources
	g_pRender->CreateRenderResources( g_pSession, selectedTextureFormats.vkColorTextureFormat, selectedTextureFormats.vkDepthTextureFormat, vkExtent );

	// (8.3) Optional: Add any shape pipelines
	if ( g_extHandTracking )
	{
		// For hand tracking cubes
		Shapes::Shape cubePalmLeft {};
		cubePalmLeft.vecIndices = &g_vecCubeIndices;
		cubePalmLeft.vecVertices = &g_vecCubeVertices;
		g_pRender->PrepareShapesPipeline( &cubePalmLeft, "shaders/shape.vert.spv", "shaders/shape.frag.spv" );

		PopulateHandShapes( &cubePalmLeft );
	}

	// (8.4) Add stuff to render (will spawn in world origin)

	// xray area
	g_unXray = g_pRender->AddRenderSector( "models/xray_area.glb", { 1.0f, 1.0f, 1.0f } );

	// hidden worlds - spaces for offsets in the scene
	XrSpace spaceHiddenWorld = XR_NULL_HANDLE;
	XrPosef poseHiddenWorld {};
	XrPosef_Identity( &poseHiddenWorld );

	// hidden world - winter forest
	g_vecHiddenWorlds.push_back( g_pRender->AddRenderSector( "models/hidden_worlds/winter_forest.glb", { 1.0f, 1.0f, 1.0f } ) );
	poseHiddenWorld.position.x = -3.0f; // left
	g_pSession->CreateReferenceSpace( &spaceHiddenWorld, XR_REFERENCE_SPACE_TYPE_STAGE, poseHiddenWorld );
	g_pRender->vecRenderSectors.back()->xrSpace = spaceHiddenWorld;

	// hidden world - la helipad
	g_vecHiddenWorlds.push_back( g_pRender->AddRenderSector( "models/hidden_worlds/la_helipad.glb", { 1.0f, 1.0f, 1.0f } ) );
	poseHiddenWorld.position.x = 3.0f; // right
	spaceHiddenWorld = XR_NULL_HANDLE;
	g_pSession->CreateReferenceSpace( &spaceHiddenWorld, XR_REFERENCE_SPACE_TYPE_STAGE, poseHiddenWorld );
	g_pRender->vecRenderSectors.back()->xrSpace = spaceHiddenWorld;

	// hidden world - barcelona rooftop
	g_vecHiddenWorlds.push_back( g_pRender->AddRenderSector( "models/hidden_worlds/barcelona_rooftop.glb", { 1.0f, 1.0f, 1.0f } ) );
	poseHiddenWorld.position.x = 0.0f;
	poseHiddenWorld.position.z = 3.0f; // back
	spaceHiddenWorld = XR_NULL_HANDLE;
	g_pSession->CreateReferenceSpace( &spaceHiddenWorld, XR_REFERENCE_SPACE_TYPE_STAGE, poseHiddenWorld );
	g_pRender->vecRenderSectors.back()->xrSpace = spaceHiddenWorld;

#ifdef XR_USE_PLATFORM_ANDROID
	// hand proxies - we'll use a lower poly model for the android version
	g_unLeftHandIndex = g_pRender->AddRenderSector( "models/RiggedLowpolyHand.glb", { -0.04f, 0.04f, 0.04f } );
	g_unRightHandIndex = g_pRender->AddRenderSector( "models/RiggedLowpolyHand.glb", { 0.04f, 0.04f, 0.04f } );
#else
	// hand proxies - we'll use a higher poly model for the pc version
	g_unLeftHandIndex = g_pRender->AddRenderSector( "models/openxr_glove_left/openxr_glove_left.gltf", { 1.0f, 1.0f, 1.0f } );
	g_unRightHandIndex = g_pRender->AddRenderSector( "models/openxr_glove_right/openxr_glove_right.gltf", { 1.0f, 1.0f, 1.0f } );

	// let's use a galaxy background for pc since passthrough isn't guaranteed
	g_pRender->AddRenderScene( "models/milkyway.glb", { 3.0f, 3.0f, 3.0f } );
#endif

	// (8.5) Optional - Set vismask if present
	oxr::ExtVisMask *pVisMask = static_cast< oxr::ExtVisMask * >( oxrProvider->Instance()->extHandler.GetExtension( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) );

	if ( pVisMask )
	{
		g_pRender->CreateVisMasks( 2 );

		pVisMask->GetVisMask(
			g_pRender->GetVisMasks()[ 0 ].vertices, g_pRender->GetVisMasks()[ 0 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );

		pVisMask->GetVisMask(
			g_pRender->GetVisMasks()[ 1 ].vertices, g_pRender->GetVisMasks()[ 1 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
	}

	// (8.6) Optional - Set skybox
	g_pRender->skybox->bIsVisible = false;
	g_pRender->skybox->currentScale = { 5.0f, 5.0f, 5.0f };
	g_pRender->skybox->bApplyOffset = true;
	g_pRender->skybox->offsetRotation = { 0.0f, 0.0f, 1.0f, 0.0f }; // rotate 180 degrees in z (roll)

	// (9) Start loading gltf assets from disk and prepare pbr pipelines
	g_pRender->LoadAssets();
	g_pRender->PrepareAllPipelines();

	// (9.1) Optional: Create custom graphics pipelines
	uint32_t unStencilPipelineIndex = 0;
	CreateStencilPipelines( &unStencilPipelineIndex, &g_unStencilFillPipelineIndex );

	// xray area
	g_pRender->vecRenderSectors[ g_unXray ]->vkPipeline = g_pRender->GetCustomPipelines()[ unStencilPipelineIndex ];

	// hidden worlds
	for ( auto index : g_vecHiddenWorlds )
	{
		g_pRender->vecRenderSectors[ index ]->vkPipeline = g_pRender->GetCustomPipelines()[ g_unStencilFillPipelineIndex ];
	}

	// (10) Register render callbacks
	//      We'll build the command buffers after acquire and submit to the queue after wait
	//      to ensure that the runtime isn't using VkQueue at the same time (most runtimes don't)
	oxr::RenderImageCallback releaseCallback;
	releaseCallback.fnCallback = PreRender_Callback;
	oxrProvider->Session()->RegisterWaitSwapchainImageImageCallback( &releaseCallback );

	oxr::RenderImageCallback waitCallback;
	waitCallback.fnCallback = PostRender_Callback;
	oxrProvider->Session()->RegisterWaitSwapchainImageImageCallback( &waitCallback );

	// (11) Set other platform callbacks
#ifdef XR_USE_PLATFORM_ANDROID
	oxrProvider->Instance()->androidApp->onAppCmd = app_handle_cmd;
#endif

	// (12) Setup input

	// (12.1) Retrieve input object from provider - this is created during provider init()
	g_pInput = oxrProvider->Input();
	if ( g_pInput == nullptr || oxrProvider->Session() == nullptr )
	{
		oxr::LogError( LOG_CATEGORY_DEMO, "Error getting input object from the provider library or init failed." );
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	// (12.2) Create actionset(s) - these are collections of actions that can be activated based on game state
	//								(e.g. one actionset for locomotion and another for ui handling)
	//								you can optionally provide a priority and other info (e.g. via extensions)
	oxr::ActionSet actionsetMain {};
	g_pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

	// (12.3) Create action(s) - these represent actions that will be triggered based on hardware state from the openxr runtime

	oxr::Action actionPose( XR_ACTION_TYPE_POSE_INPUT, &ActionSetControllerIsActive );
	g_pInput->CreateAction( &actionPose, &actionsetMain, "pose", "controller pose", { "/user/hand/left", "/user/hand/right" } );
	g_ControllerPoseAction = &actionPose; // We'll need a reference to this action for painting later on in the render callbacks

	oxr::Action actionEyePose( XR_ACTION_TYPE_POSE_INPUT, &ActionSetEyeGazeIsActive );
	g_pInput->CreateAction( &actionEyePose, &actionsetMain, "eye_pose", "combined eye gaze pose" );
	g_pActionEyePose = &actionEyePose;

	oxr::Action actionEyePoseBackup( XR_ACTION_TYPE_POSE_INPUT, &ActionSetEyeGazeBackup );
	g_pInput->CreateAction( &actionEyePoseBackup, &actionsetMain, "eye_pose_backup", "combined eye gaze pose (backup/controller)", { "/user/hand/left", "/user/hand/right" } );
	g_pActionEyePoseBackup = &actionEyePoseBackup;

	oxr::Action actionActivateXrayMode( XR_ACTION_TYPE_BOOLEAN_INPUT, &ActionActivateXrayMode );
	g_pInput->CreateAction( &actionActivateXrayMode, &actionsetMain, "activate_xray", "activate xray mode", { "/user/hand/left", "/user/hand/right" } );

	oxr::Action actionHaptic( XR_ACTION_TYPE_VIBRATION_OUTPUT, &ActionHaptic );
	g_pInput->CreateAction( &actionHaptic, &actionsetMain, "haptic", "play haptics", { "/user/hand/left", "/user/hand/right" } );
	g_hapticAction = &actionHaptic; // We'll need this to trigger haptics after a controller gesture is detected

#ifdef XR_USE_PLATFORM_ANDROID
	// For android where passthrough is guaranteed, we'll control saturation of passthrough on left and right thumbsticks (or trackpads)
	oxr::Action actionAdjustSaturation( XR_ACTION_TYPE_FLOAT_INPUT, &ActionAdjustSaturation );
	g_pInput->CreateAction( &actionAdjustSaturation, &actionsetMain, "adjust_saturation", "Adjust passthrough saturation values", { "/user/hand/left", "/user/hand/right" } );
#else

#endif

	// (12.4) Create supported controllers
	oxr::ValveIndex controllerIndex {};
	oxr::OculusTouch controllerTouch {};
	oxr::HTCVive controllerVive {};
	oxr::MicrosoftMixedReality controllerMR {};

	// (12.5) Create action to controller bindings
	//		  The use of the "BaseController" here is optional. It's a convenience controller handle that will auto-map
	//		  basic controller components to every supported controller it knows of.
	//
	//		  Alternatively (or in addition), you can directly add bindings per controller
	//		  e.g. controllerVive.AddBinding(...)
	oxr::BaseController baseController {};
	baseController.vecSupportedControllers.push_back( &controllerIndex );
	baseController.vecSupportedControllers.push_back( &controllerTouch );
	baseController.vecSupportedControllers.push_back( &controllerVive );
	baseController.vecSupportedControllers.push_back( &controllerMR );

	// poses - controllers (aim)
	g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
	g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

	// poses - controllers (aim - as backup for eye gaze)
	g_pInput->AddBinding( &baseController, actionEyePoseBackup.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
	g_pInput->AddBinding( &baseController, actionEyePoseBackup.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

	// haptics
	g_pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Haptic, oxr::Controller::Qualifier::None );
	g_pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Haptic, oxr::Controller::Qualifier::None );

	// xray mode
	g_pInput->AddBinding( &baseController, actionActivateXrayMode.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::PrimaryButton, oxr::Controller::Qualifier::Click );
	g_pInput->AddBinding( &baseController, actionActivateXrayMode.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::PrimaryButton, oxr::Controller::Qualifier::Click );

	// we'll manually define additional controller specific bindings for controllers that don't have a primary button (e.g. a/x)
	g_pInput->AddBinding( &controllerVive, actionActivateXrayMode.xrActionHandle, "/user/hand/left/input/menu/click" );
	g_pInput->AddBinding( &controllerVive, actionActivateXrayMode.xrActionHandle, "/user/hand/right/input/menu/click" );

	g_pInput->AddBinding( &controllerMR, actionActivateXrayMode.xrActionHandle, "/user/hand/left/input/menu/click" );
	g_pInput->AddBinding( &controllerMR, actionActivateXrayMode.xrActionHandle, "/user/hand/right/input/menu/click" );

#ifdef XR_USE_PLATFORM_ANDROID
	// adjust saturation in android where passthrough is guaranteed
	g_pInput->AddBinding( &baseController, actionAdjustSaturation.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Y );
	g_pInput->AddBinding( &baseController, actionAdjustSaturation.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Y );
#endif

	// (12.6) Suggest bindings to the active openxr runtime
	//        As with adding bindings, you can also suggest bindings manually per controller
	//        e.g. controllerIndex.SuggestBindings(...)
	g_pInput->SuggestBindings( &baseController, nullptr );

	// optional: bindings from extensions
	if ( g_extEyeGaze && g_extEyeGaze->IsEyeGazeSupported() )
	{
		g_extEyeGaze->AddPoseActionBinding( actionEyePose.xrActionHandle );
		g_extEyeGaze->SuggestActionBindings();
	}

	// (12.7) Initialize input with session - required for succeeding calls
	g_pInput->Init( oxrProvider->Session() );

	// (12.8) Attach actionset(s) to session
	std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
	g_pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

	// (12.9) Add actionset(s) that will be active during input sync with the openxr runtime
	//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
	//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
	g_pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

	// (12.10) Create action spaces for pose actions

	// this is for the controller
	XrPosef poseInSpace;
	XrPosef_Identity( &poseInSpace );

	g_pInput->CreateActionSpaces( &actionPose, &poseInSpace );

	// this is for the eye gaze
	XrPosef eyePoseInSpace;
	XrPosef_Identity( &eyePoseInSpace );
	eyePoseInSpace.position.z = -1.0f; // forward by a meter
	g_pInput->CreateActionSpace( &actionEyePose, &eyePoseInSpace );

	// this is for the xray area if eye gaze isn't active
	XrPosef xrayPoseInSpace;
	XrPosef_Identity( &xrayPoseInSpace );
	xrayPoseInSpace.position.y = 0.25f; // up by the radius of the xray area

	g_pInput->CreateActionSpaces( &actionEyePoseBackup, &xrayPoseInSpace );

	// assign the action space to models
	g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->xrSpace = actionPose.vecActionSpaces[ 0 ];
	g_pRender->vecRenderSectors[ g_unRightHandIndex ]->xrSpace = actionPose.vecActionSpaces[ 1 ];

	// (13) (Optional) Information and processing from extensions

	// Passthrough
	if ( g_extFBPassthrough )
	{
		g_extFBPassthrough->StartPassThrough();
		g_extFBPassthrough->SetModeToDefault();
	}

	// Refresh rate
	if ( g_extFBRefreshRate )
	{
		// Log supported refresh rates (if extension is available and active)
		std::vector< float > vecSupportedRefreshRates;
		g_extFBRefreshRate->GetSupportedRefreshRates( vecSupportedRefreshRates );

		oxr::LogDebug( LOG_CATEGORY_DEMO, "The following %i refresh rate(s) are supported by the hardware and runtime", ( uint32_t )vecSupportedRefreshRates.size() );
		for ( auto &refreshRate : vecSupportedRefreshRates )
			oxr::LogDebug( LOG_CATEGORY_DEMO, "\tRefresh rate supported: %f", refreshRate );

		// Request best available refresh rate (runtime chooses, specific refresh rate can also be specified here from result of GetSupportedRefreshRates()
		g_extFBRefreshRate->RequestRefreshRate( 0.0f );

		// Retrieve current refresh rate
		g_fCurrentRefreshRate = g_extFBRefreshRate->GetCurrentRefreshRate();
		oxr::LogDebug( LOG_CATEGORY_DEMO, "Current display refresh rate is: %f", g_fCurrentRefreshRate );
	}

	// (14) (optional) Custom states for this app

	// (14.1) global pbr values
	g_pRender->shaderValuesPbrParams.exposure = 2.0f;
	g_pRender->shaderValuesPbrParams.gamma = 1.8f;

	// (14.2) Set xray mode to off
	g_bXrayModeOn = false;
	UpdateXrayAssetsVisibility();

	// (14.3) Hide hand models if hand tracking is available
	if ( g_extHandTracking )
	{
		g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->bIsVisible = false;
		g_pRender->vecRenderSectors[ g_unRightHandIndex ]->bIsVisible = false;
	}

	// (14.4) Create view space to get hmd location
	XrPosef hmdPose {};
	XrPosef_Identity( &hmdPose );
	g_pSession->CreateReferenceSpace( &g_spaceHmd, XR_REFERENCE_SPACE_TYPE_VIEW, hmdPose );

	// Main game loop
	bool bProcessRenderFrame = false;
	bool bProcessInputFrame = false;

	UpdateAnimSpeed();

	while ( CheckGameLoopExit( oxrProvider.get() ) )
	{
#ifdef XR_USE_PLATFORM_ANDROID
		// For android we need in addition, to poll android events to proceed to the session ready state
		while ( true )
		{
			// We'll wait indefinitely (-1ms) for android poll results until we get to the android app resumed state
			// and/or the openxr session has started.
			const int nTimeout = ( !oxrProvider->Instance()->androidAppState.Resumed && oxrProvider->Session()->IsSessionRunning() ) ? -1 : 0;

			int nEvents;
			struct android_poll_source *androidPollSource;
			if ( ALooper_pollAll( nTimeout, nullptr, &nEvents, ( void ** )&androidPollSource ) < 0 )
			{
				break;
			}

			// Process android event
			if ( androidPollSource )
			{
				androidPollSource->process( oxrProvider->Instance()->androidApp, androidPollSource );
			}
		}
#endif
		// (15) Poll runtime for openxr events
		g_xrEventDataBaseheader = oxrProvider->PollXrEvents();

		// (16) Handle events
		if ( g_xrEventDataBaseheader )
		{
			// Handle session state changes
			if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
			{
				g_sessionState = oxrProvider->Session()->GetState();

				if ( g_sessionState == XR_SESSION_STATE_READY )
				{
					// (12.1) Start session - begin the app's frame loop here
					oxr::LogInfo( LOG_CATEGORY_DEMO, "App frame loop starts here." );
					xrResult = oxrProvider->Session()->Begin();
					if ( xrResult == XR_SUCCESS ) // defaults to stereo (vr)
					{
						// Start processing render frames
						bProcessRenderFrame = true;
					}
					else
					{
						bProcessRenderFrame = false;
						oxr::LogError( LOG_CATEGORY_DEMO, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
					}
				}
				else if ( g_sessionState == XR_SESSION_STATE_FOCUSED )
				{
					// (16.1) Start input
					bProcessInputFrame = true;
				}
				else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
				{
					// (16.2) End session - end input
					bProcessRenderFrame = bProcessInputFrame = false;

					// (16.3) End session - end the app's frame loop here
					oxr::LogInfo( LOG_CATEGORY_DEMO, "App frame loop ends here." );
					if ( oxrProvider->Session()->End() == XR_SUCCESS )
						bProcessRenderFrame = false;
				}
			}

			// Handle events from extensions
			if ( g_extFBRefreshRate && g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB )
			{
				auto xrExtFBRefreshRateChangedEvent = *reinterpret_cast< const XrEventDataDisplayRefreshRateChangedFB * >( g_xrEventDataBaseheader );
				g_fCurrentRefreshRate = xrExtFBRefreshRateChangedEvent.toDisplayRefreshRate;
				UpdateAnimSpeed();

				oxr::LogDebug( LOG_CATEGORY_DEMO, "Display refresh rate changed from: %f to %f", xrExtFBRefreshRateChangedEvent.fromDisplayRefreshRate, g_fCurrentRefreshRate );
			}
		}

		// (17) Input
		if ( bProcessInputFrame && g_pInput )
		{
			g_inputThread = std::async( std::launch::async, &oxr::Input::ProcessInput, g_pInput );
		}

		// (18) Render
		if ( bProcessRenderFrame )
		{
			XrCompositionLayerFlags xrCompositionLayerFlags = 0;
			g_vecFrameLayers.clear();
			if ( g_extFBPassthrough )
			{
				xrCompositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

				g_vecFrameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( g_extFBPassthrough->GetCompositionLayer() ) );
			}

			g_vecFrameLayerProjectionViews.resize( g_pSession->GetSwapchains().size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } );
			g_pSession->RenderFrameWithLayers( g_vecFrameLayerProjectionViews, g_vecFrameLayers, &g_xrFrameState, xrCompositionLayerFlags );
		}
	}

	// (19) Cleanup
	g_pRender.release();
	oxrProvider.release();

	return xrResult;
}

/**
 * These are the main entry points for the application.
 * Note that the android entry point uses the android_native_app_glue and
 * runs on its own thread.
 */
#ifdef XR_USE_PLATFORM_ANDROID
void android_main( struct android_app *app )
{
	// Attach environment to thread
	JNIEnv *Env;
	app->activity->vm->AttachCurrentThread( &Env, nullptr );

	// Create instance
	demo_openxr_start( app );

	return;
}
#else
int main( int argc, char *argv[] )
{
	// Debugging
	std::cout << "Argument count [argc] == " << argc << '\n';
	for ( uint32_t i = 0; i != argc; ++i )
	{
		std::cout << "argv[" << i << "] == " << std::quoted( argv[ i ] ) << '\n';
	}
	std::cout << "argv[" << argc << "] == " << static_cast< void * >( argv[ argc ] ) << '\n';

	std::cout << "\n\nPress enter to start. This is also a good time to attach a debugger if you need to.";
	std::cin.get();

	// Create instance
	XrResult xrResult = demo_openxr_start();
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		std::cout << "\nError running demo program with XrResult (" << xrResult << ")";
	}

	// Manual for debug sessions
	std::cout << "\n\nPress enter to end.";
	std::cin.get();

	return EXIT_SUCCESS;
}
#endif
