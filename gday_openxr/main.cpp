/* Copyright 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
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

#include<openxr_provider.h>

#define APPNAME "GDAY_OPENXR"

int main(int argc, char* argv[])
{
	// Create the openxr provider helper object
	std::unique_ptr< oxr::Provider > oxrProvider = std::make_unique< oxr::Provider >( oxr::ELogLevel::LogDebug );

	// Define a list of extensions our app wants to use
	std::vector< const char * > vecRequestedExtensions 
	{
		XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME, 
		XR_KHR_VISIBILITY_MASK_EXTENSION_NAME, 
		XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME, 
		XR_VALVE_ANALOG_THRESHOLD_EXTENSION_NAME 
	};

	oxr::LogInfo( APPNAME, "*** These are the extensions we want ***" );
	for ( auto &extName : vecRequestedExtensions )
	{
		oxr::LogInfo( APPNAME, "\t%s", extName );
	}

	// Filter out unsupported extensions (from currently active runtime)
	oxrProvider->FilterOutUnsupportedExtensions( vecRequestedExtensions );

	oxr::LogInfo( APPNAME, "*** These are the extensions that will be enabled (sans extensions that the current runtime doesn't support ***" );
	for ( auto &extName : vecRequestedExtensions )
	{
		oxr::LogInfo( APPNAME, "\t%s", extName );
	}

	// Define application instance
	oxr::AppInstanceInfo oxrAppInstanceInfo {};
	oxrAppInstanceInfo.sAppName = APPNAME;
	oxrAppInstanceInfo.unAppVersion = OXR_MAKE_VERSION32( 0, 1, 0 );
	oxrAppInstanceInfo.sEngineName = "openxr_provider";
	oxrAppInstanceInfo.unEngineVersion = OXR_MAKE_VERSION32( PROVIDER_VERSION_MAJOR, PROVIDER_VERSION_MINOR, PROVIDER_VERSION_PATCH );
	oxrAppInstanceInfo.vecInstanceExtensions = vecRequestedExtensions;
	
	// Create openxr instance
	XrResult xrResult = oxrProvider->Init( &oxrAppInstanceInfo );

	if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		oxr::LogInfo( APPNAME, "OpenXr instance created with handle (%" PRIu64 ")", ( uint64_t )oxrProvider->GetOpenXrInstance() );

		// List enabled extensions
		oxr::LogInfo( APPNAME, "*** These are the enabled openxr extensions for this instance ***" );
		for (auto& extName : oxrProvider->GetEnabledExtensions())
		{
			oxr::LogInfo( APPNAME, "\t%s", extName.c_str() );
		}
	}
}
