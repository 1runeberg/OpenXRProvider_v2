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
#include <openxr/xr_linear.h>

namespace
{
	using namespace std::literals::chrono_literals;
	XrResult demo_openxr_init()
	{
		// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
		//     You can optionally set a default log level during creation.
		std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

		std::vector< const char * > vecRequestedExtensions { XR_MND_HEADLESS_EXTENSION_NAME, XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME };

		oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

		// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
		oxr::AppInstanceInfo oxrAppInstanceInfo {};
		oxrAppInstanceInfo.sAppName = APP_NAME;
		oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( SAMPLEXX_VERSION_MAJOR, SAMPLEXX_VERSION_MINOR, SAMPLEXX_VERSION_PATCH );
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
			oxr::LogInfo( LOG_CATEGORY_HEADLESS, "OpenXr instance created with handle (%" PRIu64 ")", ( uint64_t )oxrProvider->GetOpenXrInstance() );
		}
		else
		{
			// The provider will add a log entry for the error, or optionally handle it,
			//	here we're just logging it again and using a nifty stringify function (only for openxr enums).
			oxr::LogError( LOG_CATEGORY_HEADLESS, "Error encountered while creating an openxr instance  (%s)", oxr::XrEnumToString( xrResult ) );
			return xrResult;
		}

		// (5) Create openxr session

		XrSessionCreateInfo xrSessionCreateInfo { XR_TYPE_SESSION_CREATE_INFO };
		xrSessionCreateInfo.systemId = oxrProvider->Instance()->xrSystemId;

		xrResult = oxrProvider->CreateSession( &xrSessionCreateInfo );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogInfo( LOG_CATEGORY_HEADLESS, "Headless OpenXr session created with handle (%" PRIu64 ")", ( uint64_t )oxrProvider->Session()->GetXrSession() );
		}
		else
		{
			oxr::LogError( LOG_CATEGORY_HEADLESS, "Error creating openxr session with result (%s) ", oxr::XrEnumToString( xrResult ) );
			return xrResult;
		}

		g_pSession = oxrProvider->Session();

		// (6.1) Get any extensions that requires an active openxr instance and session
		g_extHTCXViveTrackerInteraction = static_cast< oxr::ExtHTCXViveTrackerInteraction * >( oxrProvider->Instance()->extHandler.GetExtension( XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME ) );
		if ( g_extHTCXViveTrackerInteraction )
		{
			xrResult = g_extHTCXViveTrackerInteraction->Init();

			if ( !XR_UNQUALIFIED_SUCCESS( g_extHTCXViveTrackerInteraction->Init() ) )
				delete g_extHTCXViveTrackerInteraction;
		}

		g_pInput = oxrProvider->Input();
		if ( g_pInput == nullptr || oxrProvider->Session() == nullptr )
		{
			oxr::LogError( LOG_CATEGORY_HEADLESS, "Error getting input object from the provider library or init failed." );
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		oxr::ActionSet actionsetMain {};
		g_pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

		oxr::Action actionLeftElbowPose( XR_ACTION_TYPE_POSE_INPUT, UpdateLeftElbowPose );
		g_pInput->CreateAction( &actionLeftElbowPose, &actionsetMain, "left_elbow_pose", "left elbow pose", { "/user/vive_tracker_htcx/role/left_elbow" } );
		g_leftElbowPoseAction = &actionLeftElbowPose;

		oxr::Action actionLeftFootPose( XR_ACTION_TYPE_POSE_INPUT, UpdateLeftFootPose );
		g_pInput->CreateAction( &actionLeftFootPose, &actionsetMain, "left_foot_pose", "left foot pose", { "/user/vive_tracker_htcx/role/left_foot" } );
		g_leftFootPoseAction = &actionLeftFootPose; // We'll need a reference to this action for getting the pose later

		oxr::Action actionControllerPose( XR_ACTION_TYPE_POSE_INPUT, UpdateControllerPose );
		g_pInput->CreateAction( &actionControllerPose, &actionsetMain, "post", "controller pose", { "/user/hand/left", "/user/hand/right" } );
		g_controllerPoseAction = &actionControllerPose; // We'll need a reference to this action for getting the pose later

		oxr::Action actionAim( XR_ACTION_TYPE_VECTOR2F_INPUT, UpdateAim );
		g_pInput->CreateAction( &actionAim, &actionsetMain, "aim", "aim", { "/user/hand/left", "/user/hand/right" } );

		oxr::Action actionAimTouch( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateAimTouch );
		g_pInput->CreateAction( &actionAimTouch, &actionsetMain, "aim_touch", "aim touch", { "/user/hand/left", "/user/hand/right" } );
		oxr::Action actionAimClick( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateAimClick );
		g_pInput->CreateAction( &actionAimClick, &actionsetMain, "aim_click", "aim click", { "/user/hand/left", "/user/hand/right" } );
		oxr::Action actionSelect( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateSelect );
		g_pInput->CreateAction( &actionSelect, &actionsetMain, "menu", "menu", { "/user/hand/left", "/user/hand/right" } );
		oxr::Action actionActivate( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateActivate );
		g_pInput->CreateAction( &actionActivate, &actionsetMain, "activate", "activate", { "/user/hand/left", "/user/hand/right" } );

		oxr::Action actionSpecial( XR_ACTION_TYPE_FLOAT_INPUT, UpdateSpecial );
		g_pInput->CreateAction( &actionSpecial, &actionsetMain, "special", "special", { "/user/hand/left", "/user/hand/right" } );

		oxr::Action actionFullSpecial( XR_ACTION_TYPE_BOOLEAN_INPUT, UpdateFullSpecial );
		g_pInput->CreateAction( &actionFullSpecial, &actionsetMain, "full_special", "full special", { "/user/hand/left", "/user/hand/right" } );

		oxr::HTCVive controllerVive {};
		oxr::ViveTracker trackerVive {};

		trackerVive.AddBinding(
			oxrProvider->Instance()->xrInstance, actionLeftElbowPose.xrActionHandle, oxr::ViveTracker::RolePath::LeftElbow, oxr::Controller::Component::GripPose, oxr::Controller::Qualifier::None );
		trackerVive.AddBinding(
			oxrProvider->Instance()->xrInstance, actionLeftFootPose.xrActionHandle, oxr::ViveTracker::RolePath::LeftFoot, oxr::Controller::Component::GripPose, oxr::Controller::Qualifier::None );

		trackerVive.SuggestBindings( oxrProvider->Instance()->xrInstance, nullptr );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionControllerPose.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionControllerPose.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AimPose, oxr::Controller::Qualifier::None );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAim.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::None );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAim.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::None );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimTouch.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Touch );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimTouch.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Touch );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimClick.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Click );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionAimClick.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::AxisControl, oxr::Controller::Qualifier::Click );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionSelect.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Menu, oxr::Controller::Qualifier::Click );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionSelect.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Menu, oxr::Controller::Qualifier::Click );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionActivate.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Squeeze, oxr::Controller::Qualifier::Click );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionActivate.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Squeeze, oxr::Controller::Qualifier::Click );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionSpecial.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Value );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionSpecial.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Value );

		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionFullSpecial.xrActionHandle, XR_HAND_LEFT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Click );
		controllerVive.AddBinding( oxrProvider->Instance()->xrInstance, actionFullSpecial.xrActionHandle, XR_HAND_RIGHT_EXT, oxr::Controller::Component::Trigger, oxr::Controller::Qualifier::Click );

	
		controllerVive.SuggestBindings( oxrProvider->Instance()->xrInstance, nullptr );

		// (12.7) Initialize input with session - required for succeeding calls
		g_pInput->Init( oxrProvider->Session() );

		// (12.8) Attach actionset(s) to session
		std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
		g_pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

		// (12.9) Add actionset(s) that will be active during input sync with the openxr runtime
		//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
		//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
		g_pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

		XrPosef leftElbowPoseInSpace;
		XrPosef leftFootPoseInSpace;
		XrPosef controllerPoseInSpace;
		XrPosef_Identity( &leftElbowPoseInSpace );
		XrPosef_Identity( &leftFootPoseInSpace );
		XrPosef_Identity( &controllerPoseInSpace );

		g_pInput->CreateActionSpace( &actionControllerPose, &controllerPoseInSpace, "/user/hand/left" );
		g_pInput->CreateActionSpace( &actionControllerPose, &controllerPoseInSpace, "/user/hand/right" );
		g_pInput->CreateActionSpace( &actionLeftElbowPose, &leftElbowPoseInSpace, "/user/vive_tracker_htcx/role/left_elbow" );
		g_pInput->CreateActionSpace( &actionLeftFootPose, &leftFootPoseInSpace, "/user/vive_tracker_htcx/role/left_foot" );

		bool bProcessInput = false;
		bool bFakeRender = false;

		std::vector< oxr::Action > vecChangedActions = {};
		ControllerActionData *activeController = nullptr;

		// Main game loop
		while ( oxrProvider->Session()->GetState() != XR_SESSION_STATE_EXITING )
		{
			// (6) Poll runtime for openxr events
			g_xrEventDataBaseheader = oxrProvider->PollXrEvents();

			if ( g_xrEventDataBaseheader )
			{
				// Handle session state changes
				if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
				{
					g_sessionState = oxrProvider->Session()->GetState();

					if ( g_sessionState == XR_SESSION_STATE_READY )
					{
						// (7.1) Start session - begin the app's frame loop here.
						oxr::LogInfo( LOG_CATEGORY_HEADLESS, "App frame loop starts here." );

						// With the headless extension, runtimes must ignore viewconfiguration
						// and must accept a 0 value.
						xrResult = oxrProvider->Session()->Begin( ( XrViewConfigurationType )0 );
						if ( xrResult != XR_SUCCESS )
						{
							oxr::LogError( LOG_CATEGORY_HEADLESS, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
						}
					}
					else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
					{
						// (7.2) End session - end the app's frame loop here
						oxr::LogInfo( LOG_CATEGORY_HEADLESS, "App frame loop ends here." );
						oxrProvider->Session()->End();
					}
					else if ( g_sessionState == XR_SESSION_STATE_FOCUSED )
					{
						bProcessInput = true;
					}
				}
			}

			if ( bProcessInput )
			{

				g_inputThread = std::async( std::launch::async, &oxr::Input::ProcessInput, g_pInput );
				bProcessInput = false;
				bFakeRender = true;

				std::this_thread::sleep_for( 100ms );
			}
			if ( bFakeRender )
			{

				oxr::LogInfo( LOG_CATEGORY_HEADLESS, "Triggering input thread" );
				g_pSession->RenderHeadlessFrame( &g_xrFrameState );

				bProcessInput = true;
				bFakeRender = false;

				int activeSpaceIndex = -1;

				auto finish = std::chrono::high_resolution_clock::now();

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
					XrSpaceLocation handLocation { XR_TYPE_SPACE_LOCATION };
					xrResult = g_pInput->GetActionPose( &handLocation, g_controllerPoseAction, activeSpaceIndex, g_xrFrameState.predictedDisplayTime );
					

					oxr::LogInfo(
						LOG_CATEGORY_HEADLESS,
						"Hand: (x%2.2fy%2.2fz%2.2f x%2.2fy%2.2fz%2.2fw%2.2f) %s %s %s %s %s ",
						handLocation.pose.position.x,
						handLocation.pose.position.y,
						handLocation.pose.position.z,
						handLocation.pose.orientation.x,
						handLocation.pose.orientation.y,
						handLocation.pose.orientation.z,
						handLocation.pose.orientation.w,
						activeController->bAimTouch ? "1" : "0",
						activeController->bAimClick ? "1" : "0",
						activeController->bSelect ? "1" : "0",
						activeController->bActivate ? "1" : "0",
						activeController->bFullSpecial ? "1" : "0");


					if ( g_ActionLeftElbow.bIsActive )
					{
						XrSpaceLocation bodyLocation { XR_TYPE_SPACE_LOCATION };
						xrResult = g_pInput->GetActionPose( &bodyLocation, g_leftElbowPoseAction, 0, g_xrFrameState.predictedDisplayTime );

						oxr::LogInfo(
							LOG_CATEGORY_HEADLESS,
							"Body: (x%2.2fy%2.2fz%2.2f x%2.2fy%2.2fz%2.2fw%2.2f)",
							bodyLocation.pose.position.x,
							bodyLocation.pose.position.y,
							bodyLocation.pose.position.z,
							bodyLocation.pose.orientation.x,
							bodyLocation.pose.orientation.y,
							bodyLocation.pose.orientation.z,
							bodyLocation.pose.orientation.w );
					}
					if ( g_ActionLeftFoot.bIsActive )
					{
						XrSpaceLocation headLocation { XR_TYPE_SPACE_LOCATION };
						xrResult = g_pInput->GetActionPose( &headLocation, g_leftFootPoseAction, 0, g_xrFrameState.predictedDisplayTime );

						oxr::LogInfo(
							LOG_CATEGORY_HEADLESS,
							"Head: (x%2.2fy%2.2fz%2.2f x%2.2fy%2.2fz%2.2fw%2.2f)",
							headLocation.pose.position.x,
							headLocation.pose.position.y,
							headLocation.pose.position.z,
							headLocation.pose.orientation.x,
							headLocation.pose.orientation.y,
							headLocation.pose.orientation.z,
							headLocation.pose.orientation.w );
					}
				}
			}
		}

		// (8) Cleanup - this will cleanly destroy all openxr objects
		oxrProvider.release();

		return xrResult;
	}

}



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
