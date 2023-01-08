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

#define LOG_CATEGORY_EXTEYEGAZE "ExtEyeGaze"

namespace oxr
{
	class ExtEyeGaze : public ExtBase
	{
	  public:
		inline static const char *sInteractionProfilePath = "/interaction_profiles/ext/eye_gaze_interaction";
		inline static const char *sUserPath = "/user/eyes_ext";
		inline static const char *sEyeGazePath = "/user/eyes_ext/input/gaze_ext/pose";

		/// <summary>
		/// Constructor - a valid active openxr instance and session is required.
		/// </summary>
		/// <param name="xrInstance">Handle of the active openxr instance</param>
		ExtEyeGaze( XrInstance xrInstance );

		/// <summary>
		/// Registers a system id for the eye gaze tracker. This must be called prior to IsEyeGazeSupported()
		/// </summary>
		/// <param name="systemId">Active system id from the openxr runtime</param>
		/// <returns>Success</returns>
		XrResult Init( XrSystemId systemId );

		/// <summary>
		/// Checks if eye tracking hardware is currently active (and permitted)
		/// </summary>
		/// <returns>Whether or not eye gtrackign can be used in this session</returns>
		bool IsEyeGazeSupported();

		/// <summary>
		/// Adds a pose action binding to the /input/eyes_ext interaction profile provided by this extension
		/// </summary>
		/// <param name="action">Pose action to bind</param>
		/// <returns>Result of attempting to create a pose action binding for the eye pose</returns>
		XrResult AddPoseActionBinding( XrAction action );

		/// <summary>
		/// Suggests the action binding (defined in AddActionBinding() to the openxr runtime
		/// </summary>
		/// <param name="pOtherInfo">Any other info, usually provided via new extensions</param>
		/// <returns>Result of the bindings suggestion from the openxr runtime</returns>
		XrResult SuggestActionBindings( void *pOtherInfo = nullptr );

		/// <summary>
		/// Retrieves the eye gaze sample time. This will call xrLocateSpace for the eye space provided and return the
		/// XrTime for the eye gaze. Comparing with predicted frame time, the app can use this to determine precision
		/// </summary>
		/// <param name="session">Active and running openxr session</param>
		/// <param name="baseSpace">The base space to use, typically the app space</param>
		/// <param name="eyeSpace">The eye space defined for the eye pose</param>
		/// <param name="predictedTime">Current predictedframe time</param>
		/// <returns>Result of retrieving the eye gaze sample time from the openxr runtime</returns>
		XrTime GetEyeGazeSampleTime( XrSession session, XrSpace baseSpace, XrSpace eyeSpace, XrTime predictedTime );

	  private:
		// The active openxr instance handle
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// The active and running openxr session
		XrSystemId m_xrSystemId = XR_NULL_SYSTEM_ID;

		// Suggested bindings for the eye interaction profile introduced by this extension (/interaction_profiles/ext/eye_gaze_interaction)
		std::vector< XrActionSuggestedBinding > m_vecSuggestedBindings;
	};

} // namespace oxr
