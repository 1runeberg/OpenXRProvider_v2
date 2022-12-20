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

		XR_EXT_HAND_TRACKING_EXTENSION_NAME, // Multi-vendor extension but may have different behaviors/implementations across runtimes
		XR_FB_PASSTHROUGH_EXTENSION_NAME	 // Vendor specific extension - not supported on all runtimes
	};

	oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

	// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
	//     including the supported extensions we found earlier
	oxr::AppInstanceInfo oxrAppInstanceInfo {};
	oxrAppInstanceInfo.sAppName = APP_NAME;
	oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( SAMPLE7_VERSION_MAJOR, SAMPLE7_VERSION_MINOR, SAMPLE7_VERSION_PATCH );
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
		xrResult = g_extHandTracking->Init();

		if ( !XR_UNQUALIFIED_SUCCESS( g_extHandTracking->Init() ) )
			delete g_extHandTracking;
	}

	g_extFBPassthrough = static_cast< oxr::ExtFBPassthrough * >( oxrProvider->Instance()->extHandler.GetExtension( XR_FB_PASSTHROUGH_EXTENSION_NAME ) );
	if ( g_extFBPassthrough && g_pSession->GetAppSpace() != XR_NULL_HANDLE )
	{
		if ( !XR_UNQUALIFIED_SUCCESS( g_extFBPassthrough->Init() ) )
		{
			delete g_extFBPassthrough;
		}
		else
		{
			g_extFBPassthrough->StartPassThrough();
			g_extFBPassthrough->SetModeToMono();
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
	std::vector< int64_t > vecRequestedDepthFormats = { VK_FORMAT_D24_UNORM_S8_UINT }; // we'll send an empty one and let the provider choose from the runtime's supported depth formats

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
		Shapes::Shape_Cube cubePalmLeft {};
		g_pRender->PrepareShapesPipeline( &cubePalmLeft, "shaders/shape.vert.spv", "shaders/shape.frag.spv" );

		PopulateHandShapes( &cubePalmLeft );

		// For painting cubes
		g_pReferencePaint = new Shapes::Shape_Cube;
		g_pReferencePaint->vecVertices = &g_vecPaintCubeVertices;
		g_pReferencePaint->scale = { 0.01f, 0.01f, 0.01f };

		g_pRender->PrepareShapesPipeline( g_pReferencePaint, "shaders/shape.vert.spv", "shaders/shape.frag.spv" );
	}

	// (8.3) Add Render Scenes to render (will spawn in world origin)
	// g_pRender->AddRenderScene( "models/Box.glb", { 1.0f, 1.0f, 0.1f } );
	g_unLeftHandIndex = g_pRender->AddRenderSector( "models/RiggedLowpolyHand.glb", { -0.04f, 0.04f, 0.04f } );
	g_unRightHandIndex = g_pRender->AddRenderSector( "models/RiggedLowpolyHand.glb", { 0.04f, 0.04f, 0.04f } );

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
	g_pRender->skybox->currentScale = { 5.0f, 5.0f, 5.0f };
	g_pRender->skybox->bApplyOffset = true;
	g_pRender->skybox->offsetRotation = { 0.0f, 0.0f, 1.0f, 0.0f }; // rotate 180 degrees in z (roll)

	// (9) Start loading gltf assets from disk
	g_pRender->LoadAssets();
	g_pRender->PrepareAllPipelines();

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

	oxr::Action actionPose( XR_ACTION_TYPE_POSE_INPUT, &SetActionPaintIsActive );
	g_pInput->CreateAction( &actionPose, &actionsetMain, "pose", "controller pose", { "/user/hand/left", "/user/hand/right" } );

	// We'll need a reference to this action for painting later on in the render callbacks
	g_ControllerPoseAction = &actionPose;

	oxr::Action actionPaint( XR_ACTION_TYPE_BOOLEAN_INPUT, &SetActionPaintCurrentState );
	g_pInput->CreateAction( &actionPaint, &actionsetMain, "paint", "Draw in 3D space", { "/user/hand/left", "/user/hand/right" } );

	oxr::Action actionCycleFX( XR_ACTION_TYPE_BOOLEAN_INPUT, &ActionCycleFX );
	g_pInput->CreateAction( &actionCycleFX, &actionsetMain, "cycle_fx", "Cycle through colormap fx", { "/user/hand/left", "/user/hand/right" } );

	// todo: note on no filter
	oxr::Action actionScaleSkybox( XR_ACTION_TYPE_FLOAT_INPUT, &ActionScaleSkybox );
	g_pInput->CreateAction( &actionScaleSkybox, &actionsetMain, "scale_skybox", "Scale the skybox" );

	oxr::Action actionAdjustSaturation( XR_ACTION_TYPE_FLOAT_INPUT, &ActionAdjustSaturation );
	g_pInput->CreateAction( &actionAdjustSaturation, &actionsetMain, "adjust_saturation", "Adjust passthrough saturation values" );


	// (12.4) Create supported controllers
	oxr::ValveIndex controllerIndex {};
	oxr::OculusTouch controllerTouch {};

	// (12.5) Create action to controller bindings
	//		  The use of the "BaseController" here is optional. It's a convenience controller handle that will auto-map
	//		  basic controller components to every supported controller it knows of.
	//
	//		  Alternatively (or in addition), you can directly add bindings per controller
	//		  e.g. controllerVive.AddBinding(...)
	oxr::BaseController baseController {};
	baseController.vecSupportedControllers.push_back( &controllerIndex );
	baseController.vecSupportedControllers.push_back( &controllerTouch );

	// poses
	g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
	g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

	// paint
	g_pInput->AddBinding( &baseController, actionPaint.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Click );
	g_pInput->AddBinding( &baseController, actionPaint.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Click );

	// scale skybox
	g_pInput->AddBinding( &baseController, actionScaleSkybox.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Y );

	// adjust saturation
	g_pInput->AddBinding( &baseController, actionAdjustSaturation.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Y );

	// cycle through colormap fx
	g_pInput->AddBinding( &baseController, actionCycleFX.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::PrimaryButton, oxr::Controller::Qualifier::Click );
	g_pInput->AddBinding( &baseController, actionCycleFX.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::PrimaryButton, oxr::Controller::Qualifier::Click );

	//... todo: note on adding more specific controls ....

	// (12.6) Suggest bindings to the active openxr runtime
	//        As with adding bindings, you can also suggest bindings manually per controller
	//        e.g. controllerIndex.SuggestBindings(...)
	g_pInput->SuggestBindings( &baseController, nullptr );

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
	XrPosef poseInSpace;
	XrPosef_Identity( &poseInSpace );

	// g_pInput->CreateActionSpace( &actionPose, &poseInSpace, "user/hand/left");  // one for each subpath defined during action creation
	// g_pInput->CreateActionSpace( &actionPose, &poseInSpace, "user/hand/right"); // one for each subpath defined during action creation
	g_pInput->CreateActionSpaces( &actionPose, &poseInSpace );

	g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->xrSpace = actionPose.vecActionSpaces[ 0 ];
	g_pRender->vecRenderSectors[ g_unRightHandIndex ]->xrSpace = actionPose.vecActionSpaces[ 1 ];

	// Main game loop
	bool bProcessRenderFrame = false;
	bool bProcessInputFrame = false;
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
		// (13) Poll runtime for openxr events
		g_xrEventDataBaseheader = oxrProvider->PollXrEvents();

		// (14) Handle events
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
					// (14.1) Start input
					bProcessInputFrame = true;
				}
				else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
				{
					// (14.2) End session - end input
					bProcessRenderFrame = false;

					// (14.3) End session - end the app's frame loop here
					oxr::LogInfo( LOG_CATEGORY_DEMO, "App frame loop ends here." );
					if ( oxrProvider->Session()->End() == XR_SUCCESS )
						bProcessRenderFrame = false;
				}
			}
		}

		// (15) Input loop
		if ( bProcessInputFrame && g_pInput )
		{
			g_pInput->ProcessInput();
		}

		// (16) Render loop
		if ( bProcessRenderFrame )
		{
			// (15.1) Call render frame - this will call our registered callback at the appropriate times

			//  (15.2) Define projection layers to render
			XrCompositionLayerFlags xrCompositionLayerFlags = 0;
			g_vecFrameLayers.clear();
			if ( g_extFBPassthrough )
			{
				xrCompositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

				g_vecFrameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( g_extFBPassthrough->GetCompositionLayer() ) );
			}

			g_vecFrameLayerProjectionViews.resize( oxrProvider->Session()->GetSwapchains().size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } );
			oxrProvider->Session()->RenderFrame( g_vecFrameLayerProjectionViews, g_vecFrameLayers, &m_xrFrameState, xrCompositionLayerFlags );
		}
	}

	// (14) Cleanup
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
