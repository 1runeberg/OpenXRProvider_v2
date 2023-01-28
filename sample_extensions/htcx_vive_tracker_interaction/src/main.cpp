/* Copyright 2023 Charlotte Gore (GitHub: https://github.com/charlottegore)
 * and Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
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
#include <openxr/xr_linear.h>

namespace
{
	using namespace std::literals::chrono_literals;
	XrResult demo_openxr_init()
	{
		// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
		//     You can optionally set a default log level during creation.
		std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

		std::vector< const char * > vecRequestedExtensions { XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME };

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

		// (5) Create openxr session
		XrSessionCreateInfo xrSessionCreateInfo { XR_TYPE_SESSION_CREATE_INFO };
		xrSessionCreateInfo.systemId = oxrProvider->Instance()->xrSystemId;

		xrResult = oxrProvider->CreateSession( &xrSessionCreateInfo );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "Headless OpenXr session created with handle (%" PRIu64 ")", ( uint64_t )oxrProvider->Session()->GetXrSession() );
		}
		else
		{
			oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Error creating openxr session with result (%s) ", oxr::XrEnumToString( xrResult ) );
			return xrResult;
		}

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

		// (7) Configuring input
		g_pInput = oxrProvider->Input();
		if ( g_pInput == nullptr || oxrProvider->Session() == nullptr )
		{
			oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Error getting input object from the provider library or init failed." );
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		// (7.1) Create an actionset
		oxr::ActionSet actionsetMain {};
		g_pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

		// (7.2) Create actions
		oxr::Action actionControllerPose( XR_ACTION_TYPE_POSE_INPUT, UpdateControllerPose );
		g_pInput->CreateAction( &actionControllerPose, &actionsetMain, "pose", "controller pose", { "/user/hand/left", "/user/hand/right" } );
		g_controllerPoseAction = &actionControllerPose; // We'll need a reference to this action for getting the pose later

		oxr::Action actionAim( XR_ACTION_TYPE_VECTOR2F_INPUT, UpdateAim );
		g_pInput->CreateAction( &actionAim, &actionsetMain, "aim", "aim", { "/user/hand/left", "/user/hand/right" } );

		oxr::Action actionAimClick( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateAimClick );
		g_pInput->CreateAction( &actionAimClick, &actionsetMain, "aim_click", "aim click", { "/user/hand/left", "/user/hand/right" } );

		// (7.3) Create controller instances as a utility for setting bindings
		oxr::HTCVive controllerVive {};

		// (7.4) Add bindings
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionControllerPose.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionControllerPose.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAim.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::None );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAim.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::None );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimClick.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Click );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimClick.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Click );

		// (7.5) Suggest those bindings to the runtime
		controllerVive.SuggestBindings( oxrProvider->Instance()->xrInstance, nullptr );

		// (7.6) Initialize input with session - required for succeeding calls
		g_pInput->Init( oxrProvider->Session() );

		// (7.6.1) Create a default action with all possible tracker roles as subpaths
		//      this will also auto-populate ExtHTCXViveTrackerInteraction::vecActionSpaces
		//		use ExtHTCXViveTrackerInteraction::ETrackerRole as index.
		//		NOTE: This call requires an active (initialized) session
		if ( g_extViveTracker )
			g_extViveTracker->SetupAllTrackerRoles( g_pInput, &actionsetMain );

		// (7.7) Attach actionset(s) to session
		std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
		g_pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

		// (7.8) Add actionset(s) that will be active during input sync with the openxr runtime
		//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
		//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
		g_pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

		// (7.9) Create identity poses for use as action spaces for pose actions
		XrPosef controllerPoseInSpace;
		XrPosef_Identity( &controllerPoseInSpace );

		// (7.10) Create the action spaces. Note that you can add multiple action spaces for the same action.
		g_pInput->CreateActionSpace( &actionControllerPose, &controllerPoseInSpace, "/user/hand/left" );
		g_pInput->CreateActionSpace( &actionControllerPose, &controllerPoseInSpace, "/user/hand/right" );

		// Main game loop
		bool bProcessInput = false;
		bool bFakeRender = false;

		std::vector< oxr::Action > vecChangedActions = {};
		ControllerActionData *activeController = nullptr;

		while ( oxrProvider->Session()->GetState() != XR_SESSION_STATE_EXITING )
		{
			// (8) Poll runtime for openxr events
			g_xrEventDataBaseheader = oxrProvider->PollXrEvents();

			if ( g_xrEventDataBaseheader )
			{
				// Handle session state changes
				if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
				{
					g_sessionState = oxrProvider->Session()->GetState();

					if ( g_sessionState == XR_SESSION_STATE_READY )
					{
						// (8.1) Start session - begin the app's frame loop here.
						oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "App frame loop starts here." );

						// With the headless extension, runtimes must ignore viewconfiguration
						// and must accept a 0 value.
						xrResult = oxrProvider->Session()->Begin( ( XrViewConfigurationType )0 );
						if ( xrResult != XR_SUCCESS )
						{
							oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
						}
					}
					else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
					{
						// (8.2) End session - end the app's frame loop here
						oxr::LogInfo( LOG_CATEGORY_DEMO_EXT, "App frame loop ends here." );
						oxrProvider->Session()->End();
					}
					else if ( g_sessionState == XR_SESSION_STATE_FOCUSED )
					{
						bProcessInput = true;
					}
				}

				// Handle events from extensions
				if ( g_extViveTracker )
				{
					if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_VIVE_TRACKER_CONNECTED_HTCX )
					{
						auto xrEventDataViveTrackerConnectedHTCX = *reinterpret_cast< const XrEventDataViveTrackerConnectedHTCX * >( g_xrEventDataBaseheader );

						std::string sTrackerId, sTrackerRole;
						g_pInput->XrPathToString( sTrackerId, &xrEventDataViveTrackerConnectedHTCX.paths->persistentPath );
						g_pInput->XrPathToString( sTrackerId, &xrEventDataViveTrackerConnectedHTCX.paths->rolePath );

						oxr::LogDebug(
							LOG_CATEGORY_DEMO_EXT,
							"Vive tracker connected: Id[%s] Role[%s]",
							sTrackerId.c_str(),
							sTrackerRole.c_str() );
					}
				}
			}

			// In a headless session, the state will reamin XR_SESSION_STATE_FOCUSED indefinitely.
			if ( bProcessInput )
			{
				// Because there is no render in a headless session, we must manually wait for the
				// input future to complete before proceeding.
				g_inputThread = std::async( std::launch::async, &oxr::Input::ProcessInput, g_pInput );
				g_inputThread.wait();
				xrResult = g_inputThread.get();
				if ( xrResult == XR_SUCCESS )
				{
					// We've successfully synced and updated all actions, and are ready to "render"
					bProcessInput = false;
					bFakeRender = true;
				}
				else
				{
					// If there's been a problem we can log and error and just skip the rendering phase and try again.
					oxr::LogError( LOG_CATEGORY_DEMO_EXT, "Unable to process inputs (%s)", oxr::XrEnumToString( xrResult ) );
					std::this_thread::sleep_for( 100ms );
				}
			}
			// Headless Render
			if ( bFakeRender )
			{
				// We need to tell the session to render a headless frame in order
				// to get valid predictedRenderTime, required for resolving pose data.
				g_pSession->RenderHeadlessFrame( &g_xrFrameState );

				bProcessInput = true;
				bFakeRender = false;

				// Ensure we're pointing at the active hand controller.
				int activeSpaceIndex = -1;
				activeController = nullptr;

				if ( g_ActionLeftController.bIsActive )
				{
					activeController = &g_ActionLeftController;
					activeSpaceIndex = 0;
				}
				else if ( g_ActionRightController.bIsActive )
				{
					activeController = &g_ActionRightController;
					activeSpaceIndex = 1;
				}

				if ( activeController != nullptr )
				{
					// Resolve the controller pose...
					XrSpaceLocation handLocation { XR_TYPE_SPACE_LOCATION };
					xrResult = g_pInput->GetActionPose( &handLocation, g_controllerPoseAction, activeSpaceIndex, g_xrFrameState.predictedDisplayTime );

					oxr::LogInfo(
						LOG_CATEGORY_DEMO_EXT,
						"Controller:\npos: %2.2f,%2.2f,%2.2f\nrot: %2.2f,%2.2f,%2.2f,%2.2f\nAim: %1.2f,%1.2f\nAimClick: %s\n",
						handLocation.pose.position.x,
						handLocation.pose.position.y,
						handLocation.pose.position.z,
						handLocation.pose.orientation.x,
						handLocation.pose.orientation.y,
						handLocation.pose.orientation.z,
						handLocation.pose.orientation.w,
						activeController->xrvec2Aim.x,
						activeController->xrvec2Aim.y,
						activeController->bAimClick ? "On" : "Off" );

					if ( g_ActionLeftFoot.bIsActive )
					{
						XrSpaceLocation bodyLocation { XR_TYPE_SPACE_LOCATION };
						xrResult = g_pInput->GetActionPose( &bodyLocation, g_leftFootPoseAction, 0, g_xrFrameState.predictedDisplayTime );

						oxr::LogInfo(
							LOG_CATEGORY_DEMO_EXT,
							"Left Foot:\npos: %2.2f,%2.2f,%2.2f\nrot: %2.2f,%2.2f,%2.2f,%2.2f",
							bodyLocation.pose.position.x,
							bodyLocation.pose.position.y,
							bodyLocation.pose.position.z,
							bodyLocation.pose.orientation.x,
							bodyLocation.pose.orientation.y,
							bodyLocation.pose.orientation.z,
							bodyLocation.pose.orientation.w );
					}
				}

				std::this_thread::sleep_for( 100ms );
			}
		}

		// (8) Cleanup - this will cleanly destroy all openxr objects
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
