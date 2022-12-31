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

#include <assert.h>
#include <string>
#include <vector>

#include <provider/common.hpp>

#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_platform_defines.h"
#include "openxr/openxr_reflection.h"

#define LOG_CATEGORY_EXTFBREFRESHRATE "ExtFBRefreshRate"

namespace oxr
{
	class ExtFBRefreshRate : public ExtBase
	{
	  public:
		/// <summary>
		/// Constructor - a valid active openxr instance and session is required. Session may or may not be running
		/// </summary>
		/// <param name="xrInstance">Handle of the active openxr instance</param>
		/// <param name="xrSession">Handle of the active openxr session - may or may not be running</param>
		ExtFBRefreshRate( XrInstance xrInstance, XrSession xrSession );

		/// <summary>
		/// Initializes this extension. Call this before any other function in this class.
		/// </summary>
		/// <returns>Result from the openxr runtime</returns>
		XrResult Init();

		/// <summary>
		/// Retrieve all supported display refresh rates from the openxr runtime that is valid for the running session and hardware
		/// </summary>
		/// <param name="outSupportedRefreshRates">vector fo floats to put the supported refresh rates in</param>
		/// <returns>Result from the openxr runtime of retrieving all supported refresh rates</returns>
		XrResult GetSupportedRefreshRates( std::vector< float > &outSupportedRefreshRates );

		/// <summary>
		/// Retrieves the currently active display refresh rate
		/// </summary>
		/// <returns>The active display refresh rate, 0.0f if an error is encountered (check logs) - you can also check for event XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB</returns>
		float GetCurrentRefreshRate();

		/// <summary>
		/// Request a refresh rate from the openxr runtime. Provided refresh rate must be a supported refresh rate via GetSupportedRefreshRates()
		/// </summary>
		/// <param name="fRequestedRefreshRate">The requested refresh rate. This must be a valid refresh rate from GetSupportedRefreshRates(). Use 0.0f to indicate no prefrence and let the runtime
		/// choose the most appropriate one for the session.</param>
		/// <returns>Result from the openxr runtime of requesting a refresh rate</returns>
		XrResult RequestRefreshRate( float fRequestedRefreshRate );

	  private:
		// The active openxr instance handle
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// The active openxr session handle
		XrSession m_xrSession = XR_NULL_HANDLE;

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_display_refresh_rate
		PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
		PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB = nullptr;
		PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;
	};

} // namespace oxr
