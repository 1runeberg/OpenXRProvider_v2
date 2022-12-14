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

// App defines
#define APP_NAME "sample_05_hello_pbr"
#define ENGINE_NAME "openxr_provider"
#define LOG_CATEGORY_DEMO "OpenXRProviderDemo"

// Generated by cmake by populating project_config.h.in
#include "project_config.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <openxr_provider.h>

#include "xrvk/xrvk.hpp"

// global vars
XrEventDataBaseHeader *g_xrEventDataBaseheader = nullptr;
std::unique_ptr< xrvk::Render > g_pRender = nullptr;
oxr::Session *g_pSession = nullptr;
XrSessionState g_sessionState = XR_SESSION_STATE_UNKNOWN;
XrFrameState g_xrFrameState { XR_TYPE_FRAME_STATE };
std::vector< XrCompositionLayerProjectionView > g_vecFrameLayerProjectionViews;

/**
 * These are utility functions to check game loop conditions
 * For android, we need to be able to check the app state as well
 * so we can process android events data at the appropriate times.
 */
#ifdef XR_USE_PLATFORM_ANDROID
static void app_handle_cmd( struct android_app *app, int32_t cmd )
{
	oxr::AndroidAppState *appState = ( oxr::AndroidAppState * )app->userData;

	switch ( cmd )
	{
		case APP_CMD_RESUME:
		{
			appState->Resumed = true;
			break;
		}
		case APP_CMD_PAUSE:
		{
			appState->Resumed = false;
			break;
		}
		case APP_CMD_DESTROY:
		{
			appState->NativeWindow = NULL;
			break;
		}
		case APP_CMD_INIT_WINDOW:
		{
			appState->NativeWindow = app->window;
			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			appState->NativeWindow = NULL;
			break;
		}
	}
}

bool CheckGameLoopExit( oxr::Provider *oxrProvider ) { return oxrProvider->Instance()->androidApp->onAppCmd = app_handle_cmd; }

#else
bool CheckGameLoopExit( oxr::Provider *oxrProvider ) { return oxrProvider->Session()->GetState() != XR_SESSION_STATE_EXITING; }
#endif

/**
 * These are callback functions that would be registered with the
 * openxr provider to handle render calls at appropriate times in
 * the application.
 */
void PreRender_Callback( uint32_t unSwapchainIndex, uint32_t unImageIndex )
{
	g_pRender->BeginRender( g_pSession, g_vecFrameLayerProjectionViews, &g_xrFrameState, unSwapchainIndex, unImageIndex, 0.1f, 10000.f );
}

void PostRender_Callback( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pRender->EndRender(); }

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

	std::vector< const char * > vecRequestedExtensions {										// These extensions, being KHR, are very likely to be supported by most major runtimes
														 XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,	// this is our preferred graphics extension (only vulkan is supported)
														 XR_KHR_VISIBILITY_MASK_EXTENSION_NAME, // this gives us a stencil mask area that the end user will never see, so we don't need to render to it
																								// XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,

														 XR_EXT_HAND_TRACKING_EXTENSION_NAME };

	oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

	// (3) Set the application info that the openxr runtime will need in order to initialize an openxr instance
	//     including the supported extensions we found earlier
	oxr::AppInstanceInfo oxrAppInstanceInfo {};
	oxrAppInstanceInfo.sAppName = APP_NAME;
	oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( SAMPLE5_VERSION_MAJOR, SAMPLE5_VERSION_MINOR, SAMPLE5_VERSION_PATCH );
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
	//oxr::ExtHandTracking* g_exthandTracking= static_cast<oxr::ExtHandTracking*>(oxrProvider->Instance()->extHandler.GetExtension(XR_EXT_HAND_TRACKING_EXTENSION_NAME));
	//if (g_exthandTracking)
	//{
	//	xrResult = g_exthandTracking->Init();
	//}

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

	// (8.4) Add Render Sectors and Render Models (will spawn based on defined reference space and/or offsets from world origin)
	XrSpace spaceFront;
	oxrProvider->Session()->CreateReferenceSpace( &spaceFront, XR_REFERENCE_SPACE_TYPE_STAGE, { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -3.0f, -1.0f } } ); // 3m down, no rotation

	XrSpace spaceLeft;
	oxrProvider->Session()->CreateReferenceSpace( &spaceLeft, XR_REFERENCE_SPACE_TYPE_STAGE, { { 0.5f, 0.5f, -0.5f, 0.5f }, { -1.0f, 1.0f, 0.0f } } ); // 1m left 1m up, rotated x: 90, y: 0, z: 90

	// asset loading is done asynchronously with start times in FIFO from adding them
	// so best to add more complex models first
	g_pRender->AddRenderModel( "models/DamagedHelmet.glb", { 0.25f, 0.25f, 0.25f }, spaceLeft );
	g_pRender->AddRenderModel( "models/EnvironmentTest/EnvironmentTest.gltf", { 0.2f, 0.2f, 0.2f }, spaceFront );

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
	g_pRender->SetSkyboxVisibility( true );
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

	// Main game loop
	bool bProcessRenderFrame = false;
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
		// (12) Poll runtime for openxr events
		g_xrEventDataBaseheader = oxrProvider->PollXrEvents();

		// Handle events
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
						bProcessRenderFrame = true;
					}
					else
					{
						oxr::LogError( LOG_CATEGORY_DEMO, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
					}
				}
				else if ( g_sessionState == XR_SESSION_STATE_STOPPING )
				{
					// (12.2) End session - end the app's frame loop here
					oxr::LogInfo( LOG_CATEGORY_DEMO, "App frame loop ends here." );
					if ( oxrProvider->Session()->End() == XR_SUCCESS )
						bProcessRenderFrame = false;
				}
			}
		}

		// Render loop
		if ( bProcessRenderFrame )
		{
			// (13) Call render frame - this will call our registered callback at the appropriate times
			g_vecFrameLayerProjectionViews.resize( oxrProvider->Session()->GetSwapchains().size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } );
			oxrProvider->Session()->RenderFrame( g_vecFrameLayerProjectionViews, &g_xrFrameState );
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
