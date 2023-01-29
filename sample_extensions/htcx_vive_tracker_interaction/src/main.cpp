/* Copyright 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
 *
 * Based on Pull Request #2 by Charlotte Gore (GitHub: https://github.com/charlottegore)
 * SPDX-License-Identifier: MIT
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
#include <openxr/xr_linear.h>

inline void Callback_ControllerPose( oxr::Action *pAction, uint32_t unActionStateIndex ) { return; }

namespace
{
	using namespace std::literals::chrono_literals;
	XrResult demo_openxr_init()
	{
		// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
		//     You can optionally set a default log level during creation.
		std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

		std::vector< const char * > vecRequestedExtensions {
			// These extensions, being KHR, are very likely to be supported by most major runtimes
			XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,   // this is our preferred graphics extension (only vulkan is supported)
			XR_KHR_VISIBILITY_MASK_EXTENSION_NAME, // this gives us a stencil mask area that the end user will never see, so we don't need to render to it

			XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME };

		oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

		// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
		oxr::AppInstanceInfo oxrAppInstanceInfo {};
		oxrAppInstanceInfo.sAppName = APP_NAME;
		oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( HTCX_TRACKER_VERSION_MAJOR, HTCX_TRACKER_VERSION_MINOR, HTCX_TRACKER_VERSION_PATCH );
		oxrAppInstanceInfo.sEngineName = ENGINE_NAME;
		oxrAppInstanceInfo.unEngineVersion = OXR_MAKE_VERSION32( PROVIDER_VERSION_MAJOR, PROVIDER_VERSION_MINOR, PROVIDER_VERSION_PATCH );
		oxrAppInstanceInfo.vecInstanceExtensions = vecRequestedExtensions;

		// (4) Initialize the provider - this will create an openxr instance with the current default openxr runtime.
		//     If there's no runtime or something has gone wrong, the provider will return an error code
		//			from https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#return-codes
		//     Notice that you can also use native openxr data types (e.g. XrResult) directly

		XrResult xrResult = oxrProvider->Init( &oxrAppInstanceInfo );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "OpenXr instance created with handle (%" PRIu64 ")", ( uint64_t )oxrProvider->GetOpenXrInstance() );
		}
		else
		{
			// The provider will add a log entry for the error, or optionally handle it,
			//	here we're just logging it again and using a nifty stringify function (only for openxr enums).
			oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Error encountered while creating an openxr instance  (%s)", oxr::XrEnumToString( xrResult ) );
			return xrResult;
		}

		// (5) Setup the vulkan renderer and create an openxr graphics binding
#ifdef XR_USE_PLATFORM_ANDROID
		g_pRender = std::make_unique< xrvk::Render >( oxrProvider->Instance()->androidActivity->assetManager, xrvk::ELogLevel::LogVerbose );
#else
		g_pRender = std::make_unique< xrvk::Render >( xrvk::ELogLevel::LogVerbose );
#endif

		xrResult =
			g_pRender->Init( oxrProvider.get(), oxrAppInstanceInfo.sAppName.c_str(), oxrAppInstanceInfo.unAppVersion, oxrAppInstanceInfo.sEngineName.c_str(), oxrAppInstanceInfo.unEngineVersion );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (5) Create openxr session
		xrResult = oxrProvider->CreateSession( g_pRender->GetVulkanGraphicsBinding() );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		g_pSession = oxrProvider->Session();

		// (6) Get any extensions that requires an active openxr instance and session
		g_extViveTracker = static_cast< oxr::ExtHTCXViveTrackerInteraction * >( oxrProvider->Instance()->extHandler.GetExtension( XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME ) );
		if ( g_extViveTracker )
		{
			xrResult = g_extViveTracker->Init();

			if ( !XR_UNQUALIFIED_SUCCESS( g_extViveTracker->Init() ) )
			{
				delete g_extViveTracker;
				g_extViveTracker = nullptr;
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

		// (8.3) Add Render Scenes to render (will spawn in world origin)
		g_pRender->AddRenderScene( "models/Box.glb", { 1.0f, 1.0f, 0.1f } );

		g_renderModels.leftController = g_pRender->AddRenderSector( "models/openxr_glove_left/openxr_glove_left.gltf", { 1.0f, 1.0f, 1.0f } );
		g_renderModels.rightController = g_pRender->AddRenderSector( "models/openxr_glove_right/openxr_glove_right.gltf", { 1.0f, 1.0f, 1.0f } );

		if ( g_extViveTracker )
		{
			g_renderModels.vecTrackers.resize( oxr::ExtHTCXViveTrackerInteraction::ETrackerRole::TrackerRoleMax, 0 );

			for ( uint32_t i = 0; i < oxr::ExtHTCXViveTrackerInteraction::ETrackerRole::TrackerRoleMax; i++ )
			{
				g_renderModels.vecTrackers[ i ] = g_pRender->AddRenderSector( "models/vive_tracker_2018.glb" );
			}
		}

		// (8.4) Optional - Set vismask if present
		oxr::ExtVisMask *pVisMask = static_cast< oxr::ExtVisMask * >( oxrProvider->Instance()->extHandler.GetExtension( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) );

		if ( pVisMask )
		{
			g_pRender->CreateVisMasks( 2 );

			pVisMask->GetVisMask(
				g_pRender->GetVisMasks()[ 0 ].vertices, g_pRender->GetVisMasks()[ 0 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );

			pVisMask->GetVisMask(
				g_pRender->GetVisMasks()[ 1 ].vertices, g_pRender->GetVisMasks()[ 1 ].indices, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
		}

		// (8.5) Optional - Set skybox
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

		// (11) Setup input

		// (11.1) Retrieve input object from provider - this is created during provider init()
		g_pInput = oxrProvider->Input();
		if ( g_pInput == nullptr || oxrProvider->Session() == nullptr )
		{
			oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Error getting input object from the provider library or init failed." );
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		// (11.2) Create actionset(s) - these are collections of actions that can be activated based on game state
		//								(e.g. one actionset for locomotion and another for ui handling)
		//								you can optionally provide a priority and other info (e.g. via extensions)
		oxr::ActionSet actionsetMain {};
		g_pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

		// (11.3) Create action(s) - these represent actions that will be triggered based on hardware state from the openxr runtime

		oxr::Action actionPose( XR_ACTION_TYPE_POSE_INPUT, &Callback_ControllerPose );
		g_pInput->CreateAction( &actionPose, &actionsetMain, "pose", "controller pose", { "/user/hand/left", "/user/hand/right" } );
		g_ControllerPoseAction = &actionPose; // We'll need a reference to this action for painting later on in the render callbacks

		// (11.4) Create supported controllers
		oxr::ValveIndex controllerIndex {};
		oxr::OculusTouch controllerTouch {};
		oxr::HTCVive controllerVive {};
		oxr::MicrosoftMixedReality controllerMR {};

		// (11.5) Create action to controller bindings
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

		// poses
		g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
		g_pInput->AddBinding( &baseController, actionPose.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

		// (11.6) Suggest bindings to the active openxr runtime
		//        As with adding bindings, you can also suggest bindings manually per controller
		//        e.g. controllerIndex.SuggestBindings(...)
		g_pInput->SuggestBindings( &baseController, nullptr );

		// (11.7) Initialize input with session - required for succeeding calls
		g_pInput->Init( oxrProvider->Session() );

		// (11.8) Create a default action with all possible tracker roles as subpaths
		//      this will also auto-populate ExtHTCXViveTrackerInteraction::vecActionSpaces
		//		use ExtHTCXViveTrackerInteraction::ETrackerRole as index.
		//
		//		NOTE: This call requires an active (initialized) session and MUST be called
		//			  BEFORE AttachActionSetsToSession() and AFTER oxr::Provider::Input::Init(Session)
		if ( g_extViveTracker )
		{
			g_extViveTracker->SetupAllTrackerRoles( g_pInput, &actionsetMain ); // Setup all possible tracker roles incl action spaces to the runtime
			g_extViveTracker->SuggestDefaultBindings();							// Send all possible tracker pose bindings to the runtime
		}

		// (11.9) Attach actionset(s) to session
		std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
		g_pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

		// (11.10) Add actionset(s) that will be active during input sync with the openxr runtime
		//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
		//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
		g_pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

		// (11.11) Create action spaces for pose actions
		XrPosef poseInSpace;
		XrPosef_Identity( &poseInSpace );

		// g_pInput->CreateActionSpace( &actionPose, &poseInSpace, "user/hand/left");  // one for each subpath defined during action creation
		// g_pInput->CreateActionSpace( &actionPose, &poseInSpace, "user/hand/right"); // one for each subpath defined during action creation
		g_pInput->CreateActionSpaces( &actionPose, &poseInSpace );

		// (11.12) assign the action space to models
		g_pRender->vecRenderSectors[ g_renderModels.leftController ]->xrSpace = actionPose.vecActionSpaces[ 0 ];
		g_pRender->vecRenderSectors[ g_renderModels.rightController ]->xrSpace = actionPose.vecActionSpaces[ 1 ];

		// (12) additional prep from extensions
		if ( g_extViveTracker )
		{
			// (12.1) For this demo, we're going to use the default action spaces and assign that to our render models
			//		  this allows the renderer to automatically update poses based on XrSpace for all possible roles.
			//		  An app can also manually check for poses by calling oxr::Input::xrLocateSpace(...)
			for ( uint32_t i = 0; i < oxr::ExtHTCXViveTrackerInteraction::ETrackerRole::TrackerRoleMax; i++ )
				g_pRender->vecRenderSectors[ g_renderModels.vecTrackers[ i ] ]->xrSpace = g_extViveTracker->vecActionSpaces[ i ];

			// (12.2) Log active trackers during start
			std::vector< XrViveTrackerPathsHTCX > vecConnectedTrackers;
			xrResult = g_extViveTracker->GetAllConnectedTrackers( vecConnectedTrackers );
			
			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "The following trackers are active during startup: " );

				for ( auto &tracker : vecConnectedTrackers )
				{
					std::string sTrackerId, sTrackerRole;
					g_pInput->XrPathToString( sTrackerId, &tracker.persistentPath );
					g_pInput->XrPathToString( sTrackerRole, &tracker.rolePath );

					oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "Persistent Path/Id: (%s) Role: (%s)", &tracker.persistentPath, &tracker.rolePath );
				}
			}
		}

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

			// Handle events
			if ( g_xrEventDataBaseheader )
			{
				// (13.1) Handle session state changes
				if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
				{
					g_sessionState = oxrProvider->Session()->GetState();

					if ( g_sessionState == XR_SESSION_STATE_READY )
					{
						// (13.1.1) Start session - begin the app's frame loop here
						oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "App frame loop starts here." );
						xrResult = oxrProvider->Session()->Begin();
						if ( xrResult == XR_SUCCESS ) // defaults to stereo (vr)
						{
							// Start processing render frames
							bProcessRenderFrame = true;
						}
						else
						{
							bProcessRenderFrame = false;
							oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
						}
					}
					else if ( g_sessionState == XR_SESSION_STATE_FOCUSED )
					{
						// (13.1.2) Start input
						bProcessInputFrame = true;
					}
					else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
					{
						// (13.1.3) End session - end the app's frame loop here
						bProcessRenderFrame = bProcessInputFrame = false;

						// (13.1.4) End session - end the app's frame loop here
						oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "App frame loop ends here." );
						if ( oxrProvider->Session()->End() == XR_SUCCESS )
							bProcessRenderFrame = false;
					}
				}

				// (13.2) Handle events from extensions
				if ( g_extViveTracker )
				{
					// (13.2.1) If a new tracker is connected, log info about it
					if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_VIVE_TRACKER_CONNECTED_HTCX )
					{
						auto xrEventDataViveTrackerConnectedHTCX = *reinterpret_cast< const XrEventDataViveTrackerConnectedHTCX * >( g_xrEventDataBaseheader );

						std::string sTrackerId, sTrackerRole;
						g_pInput->XrPathToString( sTrackerId, &xrEventDataViveTrackerConnectedHTCX.paths->persistentPath );
						g_pInput->XrPathToString( sTrackerRole, &xrEventDataViveTrackerConnectedHTCX.paths->rolePath );

						oxr::LogDebug( LOG_CATEGORY_DEMO_EXT, "Vive tracker connected: Id[%s] Role[%s]", sTrackerId.c_str(), sTrackerRole.c_str() );
					}
				}
			}

			// (14) Input
			if ( bProcessInputFrame && g_pInput )
			{
				g_inputThread = std::async( std::launch::async, &oxr::Input::ProcessInput, g_pInput );
			}

			// (15) Render loop
			if ( bProcessRenderFrame )
			{
				// Call render frame - this will call our registered callback at the appropriate times

				//  (15.1) Define projection layers to render
				XrCompositionLayerFlags xrCompositionLayerFlags = 0;
				g_vecFrameLayers.clear();

				g_vecFrameLayerProjectionViews.resize( oxrProvider->Session()->GetSwapchains().size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } );
				oxrProvider->Session()->RenderFrameWithLayers( g_vecFrameLayerProjectionViews, g_vecFrameLayers, &g_xrFrameState, xrCompositionLayerFlags );
			}
		}

		// (16) Cleanup - this will cleanly destroy all openxr objects
		oxrProvider.release();

		return xrResult;
	}

} // namespace

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
	demo_openxr_init();

	// Manual for debug sessions
	std::cout << "\n\nPress enter to end.";
	std::cin.get();

	return EXIT_SUCCESS;
}
