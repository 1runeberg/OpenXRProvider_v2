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

#include "YOUR_MAIN_CLASS_HERE.hpp"

using namespace YOUR_NAMESPACE_HERE;

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

	// Create main app class
	std::unique_ptr< YOUR_MAIN_CLASS_HERE > pApp = std::make_unique< YOUR_MAIN_CLASS_HERE >();


	// Start openxr app
	XrResult xrResult = pApp->Start( app );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		LogError( LOG_CATEGORY_APP, "FATAL! Unable to start YOUR_APP_NAME_HERE" );
	}
}
#else
int main( int argc, char *argv[] )
{
	// Debugging
	if (argc > 1)
	{
		std::cout << "\n\nDebug pause - attach debugger and press enter to start.";
		std::cin.get();
	}

	// Create main app class
	std::unique_ptr< YOUR_MAIN_CLASS_HERE > pApp = std::make_unique< YOUR_MAIN_CLASS_HERE >();

	// Start openxr app
	XrResult xrResult = pApp->Start();
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		LogError( LOG_CATEGORY_APP, "FATAL! Unable to start YOUR_APP_NAME_HERE" );
		return EXIT_FAILURE;
	}

	// For debug sessions
	if ( argc > 1 )
	{
		std::cout << "\n\nPress enter to end.";
		std::cin.get();
	}

	return EXIT_SUCCESS;
}
#endif
