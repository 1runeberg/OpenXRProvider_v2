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

XrResult demo_openxr_init()
{
	// (1) Create a openxr provider object that we'll use to communicate with the openxr runtime.
	//     You can optionally set a default log level during creation.
	std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

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
		XR_MND_HEADLESS_EXTENSION_NAME,
		XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME
	};

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
	xrSessionCreateInfo.systemId = oxrProvider->Instance()->xrSystemId;;

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

	// (6) Get any extensions that requires an active openxr instance and session
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
				if ( oxrProvider->Session()->GetState() == XR_SESSION_STATE_READY )
				{
					// (7.1) Start session - begin the app's frame loop here.
					oxr::LogInfo( LOG_CATEGORY_HEADLESS, "App frame loop starts here." );

					// With the headless extension, runtimes must ignore viewconfiguration 
					// and must accept a 0 value. 
					xrResult = oxrProvider->Session()->Begin((XrViewConfigurationType)0);
				}
				else if ( oxrProvider->Session()->GetState() == XR_SESSION_STATE_STOPPING )
				{
					// (7.2) End session - end the app's frame loop here
					oxr::LogInfo( LOG_CATEGORY_HEADLESS, "App frame loop ends here." );
					oxrProvider->Session()->End();
				}
			}
			else if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_VIVE_TRACKER_CONNECTED_HTCX )
			{
				const XrEventDataViveTrackerConnectedHTCX &viveTrackerConnected = *reinterpret_cast< XrEventDataViveTrackerConnectedHTCX * >( g_xrEventDataBaseheader );
				std::string path;
				g_pInput->XrPathToString( path, &viveTrackerConnected.paths->persistentPath );
				oxr::LogInfo( LOG_CATEGORY_HEADLESS, "Vice Tracker connected: %s", path.c_str());

				if ( viveTrackerConnected.paths->rolePath != XR_NULL_PATH )
				{
					g_pInput->XrPathToString( path, &viveTrackerConnected.paths->rolePath );
					oxr::LogInfo( LOG_CATEGORY_HEADLESS, "New role is: %s", path.c_str() );
				}
				else
				{
					oxr::LogInfo( LOG_CATEGORY_HEADLESS, "No role path");
				}
			}
			else if ( g_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED )
			{

			}
		}

		#ifdef LINUX
		usleep( SLEEP * 1000 ); // usleep takes sleep time in us (1 millionth of a second)
		#endif
		#ifdef WINDOWS
		Sleep( SLEEP );
		#endif
	}

	// (8) Cleanup - this will cleanly destroy all openxr objects
	oxrProvider.release();

	return xrResult;
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
