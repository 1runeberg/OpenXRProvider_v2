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

#pragma once
#pragma warning( disable : 4996 ) // windows: warning C4996: 'strncpy'

#include <mutex>
#include <stdarg.h>

#include "data_types.hpp"

#define LOG_CATEGORY_DEFAULT "OpenXR"
#ifdef XR_USE_PLATFORM_ANDROID
	#include <android/log.h>
#endif

namespace oxr
{
	static const char *GetLogLevelName( ELogLevel eLogLevel )
	{
		switch ( eLogLevel )
		{
			case oxr::ELogLevel::LogVerbose:
				return "Verbose";
				break;
			case oxr::ELogLevel::LogDebug:
				return "Debug";
				break;
			case oxr::ELogLevel::LogInfo:
				return "Info";
				break;
			case oxr::ELogLevel::LogWarning:
				return "Warning";
				break;
			case oxr::ELogLevel::LogError:
				return "Error";
				break;
			case oxr::ELogLevel::LogNone:
			case oxr::ELogLevel::LogEMax:
			default:
				break;
		}

		return "None";
	}

	static bool CheckLogLevel( ELogLevel eLogLevel, ELogLevel eMinLogLevel )
	{
		if ( eLogLevel < eMinLogLevel || eLogLevel == ELogLevel::LogNone )
			return false;

		return true;
	}

	static bool CheckLogLevelDebug( ELogLevel eLogLevel ) { return CheckLogLevel( eLogLevel, ELogLevel::LogDebug ); }

	static bool CheckLogLevelVerbose( ELogLevel eLogLevel ) { return CheckLogLevel( eLogLevel, ELogLevel::LogVerbose ); }

	static void Log( ELogLevel eLoglevel, std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		switch ( eLoglevel )
		{
			case oxr::ELogLevel::LogDebug:
				__android_log_vprint( ANDROID_LOG_DEBUG, pccCategory, pccMessageFormat, pArgs );
				break;
			case oxr::ELogLevel::LogInfo:
				__android_log_vprint( ANDROID_LOG_INFO, pccCategory, pccMessageFormat, pArgs );
				break;
			case oxr::ELogLevel::LogWarning:
				__android_log_vprint( ANDROID_LOG_WARN, pccCategory, pccMessageFormat, pArgs );
				break;
			case oxr::ELogLevel::LogError:
				__android_log_vprint( ANDROID_LOG_ERROR, pccCategory, pccMessageFormat, pArgs );
				break;
			case oxr::ELogLevel::LogNone:
			case oxr::ELogLevel::LogEMax:
			default:
				break;
		}
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( eLoglevel ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogInfo( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_INFO, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogInfo ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogVerbose( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_VERBOSE, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogVerbose ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogDebug( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_DEBUG, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogDebug ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogWarning( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_WARN, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogWarning ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
		va_end( pArgs );
	}

	static void LogError( std::string sCategory, const char *pccMessageFormat, ... )
	{
		va_list pArgs;
		va_start( pArgs, pccMessageFormat );

		const char *pccCategory = sCategory.empty() ? LOG_CATEGORY_DEFAULT : sCategory.c_str();

#ifdef XR_USE_PLATFORM_ANDROID
		__android_log_vprint( ANDROID_LOG_ERROR, pccCategory, pccMessageFormat, pArgs );
#else
		printf( "[%s][%s] ", pccCategory, GetLogLevelName( ELogLevel::LogError ) );
		vfprintf( stdout, pccMessageFormat, pArgs );
		printf( "\n" );
#endif
	}
} // namespace oxr
