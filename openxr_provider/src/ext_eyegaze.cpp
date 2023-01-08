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

#include <provider/ext_eyegaze.hpp>

namespace oxr
{
	ExtEyeGaze::ExtEyeGaze( XrInstance xrInstance )
		: ExtBase( XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		m_xrInstance = xrInstance;
	}

	XrResult ExtEyeGaze::Init( XrSystemId systemId )
	{
		assert( systemId != XR_NULL_SYSTEM_ID );

		m_xrSystemId = systemId;

		return XR_SUCCESS;
	}

	bool ExtEyeGaze::IsEyeGazeSupported()
	{
		// eye gaze properties
		XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProps { XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT };
		eyeGazeProps.next = nullptr;
		eyeGazeProps.supportsEyeGazeInteraction = XR_FALSE;

		// system properties
		XrSystemProperties systemProps { XR_TYPE_SYSTEM_PROPERTIES };
		systemProps.next = &eyeGazeProps;

		// get system properties with eye gaze support status (hardware and security context dependent)
		XrResult xrResult = xrGetSystemProperties( m_xrInstance, m_xrSystemId, &systemProps );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			return static_cast< bool >( eyeGazeProps.supportsEyeGazeInteraction );
		}

		LogWarning( LOG_CATEGORY_EXTEYEGAZE, "Unable to get eye gaze properties from runtime: %s", XrEnumToString( xrResult ) );
		return false;
	}

	XrResult ExtEyeGaze::AddPoseActionBinding( XrAction action )
	{
		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( m_xrInstance, sEyeGazePath, &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTEYEGAZE, "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sEyeGazePath, sInteractionProfilePath );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		m_vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( LOG_CATEGORY_EXTEYEGAZE, "Added binding path: (%s) for: (%s)", sEyeGazePath, sInteractionProfilePath );
		return XR_SUCCESS;
	}

	XrResult ExtEyeGaze::SuggestActionBindings( void *pOtherInfo )
	{
		// Convert interaction profile path to an xrpath
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( m_xrInstance, sInteractionProfilePath, &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTEYEGAZE, "Error converting interaction profile to an xrpath (%s): %s", XrEnumToString( xrResult ), sInteractionProfilePath );
			return xrResult;
		}

		XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		xrInteractionProfileSuggestedBinding.next = pOtherInfo;
		xrInteractionProfileSuggestedBinding.interactionProfile = xrPath;
		xrInteractionProfileSuggestedBinding.suggestedBindings = m_vecSuggestedBindings.data();
		xrInteractionProfileSuggestedBinding.countSuggestedBindings = static_cast< uint32_t >( m_vecSuggestedBindings.size() );

		xrResult = xrSuggestInteractionProfileBindings( m_xrInstance, &xrInteractionProfileSuggestedBinding );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTEYEGAZE, "Error suggesting bindings (%s) for %s", XrEnumToString( xrResult ), sInteractionProfilePath );
		}

		LogInfo( LOG_CATEGORY_EXTEYEGAZE, "All action bindings sent to runtime for: (%s)", sInteractionProfilePath );
		return xrResult;
	}

	XrTime ExtEyeGaze::GetEyeGazeSampleTime( XrSession session, XrSpace baseSpace, XrSpace eyeSpace, XrTime predictedTime )
	{
		XrEyeGazeSampleTimeEXT sampleTime { XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT };

		XrSpaceLocation eyeSpaceLocation { XR_TYPE_SPACE_LOCATION };
		eyeSpaceLocation.next = &sampleTime;

		XrResult xrResult = xrLocateSpace( eyeSpace, baseSpace, predictedTime, &eyeSpaceLocation );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			return sampleTime.time;
		}

		return 0;
	}

} // namespace oxr
