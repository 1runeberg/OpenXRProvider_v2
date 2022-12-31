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

#include <provider/ext_fbrefreshrate.hpp>

namespace oxr
{
	ExtFBRefreshRate::ExtFBRefreshRate( XrInstance xrInstance, XrSession xrSession )
		: ExtBase( XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		assert( xrSession != XR_NULL_HANDLE );

		m_xrInstance = xrInstance;
		m_xrSession = xrSession;

		// Initialize all function pointers available for this extension
		INIT_PFN( m_xrInstance, xrEnumerateDisplayRefreshRatesFB );
		INIT_PFN( m_xrInstance, xrGetDisplayRefreshRateFB );
		INIT_PFN( m_xrInstance, xrRequestDisplayRefreshRateFB );
	}

	XrResult ExtFBRefreshRate::Init() { return XR_SUCCESS; }

	XrResult ExtFBRefreshRate::GetSupportedRefreshRates( std::vector< float > &outSupportedRefreshRates )
	{
		// Check for a valid session
		if ( m_xrSession == XR_NULL_HANDLE )
		{
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "No valid session found. Did you call init()?" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		// Get the refresh rate count capacity
		uint32_t unRefreshRateCount = 0;
		XrResult xrResult = xrEnumerateDisplayRefreshRatesFB( m_xrSession, 0, &unRefreshRateCount, nullptr );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "Error retrieving all supported refresh rates: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Get the all available refresh rates
		uint32_t unRefreshRateCapacity = unRefreshRateCount;
		outSupportedRefreshRates.resize( unRefreshRateCapacity );

		xrResult = xrEnumerateDisplayRefreshRatesFB( m_xrSession, unRefreshRateCapacity, &unRefreshRateCount, outSupportedRefreshRates.data() );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "Error retrieving all supported refresh rates: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	float ExtFBRefreshRate::GetCurrentRefreshRate()
	{
		// Check for a valid session
		if ( m_xrSession == XR_NULL_HANDLE )
		{
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "No valid session found. Did you call init()?" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		float outRefreshRate = 0.0f;
		XrResult xrResult = xrGetDisplayRefreshRateFB( m_xrSession, &outRefreshRate );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "Error retrieving current refresh rate: %s", XrEnumToString( xrResult ) );
			return 0.0f;
		}

		return outRefreshRate;
	}

	XrResult ExtFBRefreshRate::RequestRefreshRate( float fRequestedRefreshRate )
	{
		// Check for a valid session
		if ( m_xrSession == XR_NULL_HANDLE )
		{
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "No valid session found. Did you call init()?" );
			return XR_ERROR_VALIDATION_FAILURE;
		}

		XrResult xrResult = xrRequestDisplayRefreshRateFB( m_xrSession, fRequestedRefreshRate );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( LOG_CATEGORY_EXTFBREFRESHRATE, "Error requesting refresh rate (%f): %s", fRequestedRefreshRate, XrEnumToString( xrResult ) );

		return xrResult;
	}

} // namespace oxr
