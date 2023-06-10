/* Copyright 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
 * 
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

#include <assert.h>
#include <provider/ext_htcwrist.hpp>

namespace oxr
{
	XrResult HTCViveWrist::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) 
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccHTCLeftWrist : k_pccHTCRightWrist;
		sBinding += k_pccInput; // no haptics for this controller

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
			case Controller::Component::AimPose:
				sBinding += k_pccHTCEntityPose;
				break;
			case Controller::Component::Trigger:
			case Controller::Component::PrimaryButton:
			{
				sBinding += ( hand == XR_HAND_LEFT_EXT ) ? k_pccX : k_pccA;
				sBinding += k_pccClick;
			}
			break;
			case Controller::Component::Menu:
			case Controller::Component::System:
				sBinding +=  ( hand == XR_HAND_LEFT_EXT ) ? k_pccMenu : k_pccSystem;
				sBinding += k_pccClick;
				break;
			default:
				sBinding.clear();
				break;
		}

		// If binding can't be mapped, don't add
		// We'll then let the dev do "additive" bindings instead using AddBinding( XrInstance xrInstance, XrAction action, FULL INPUT PAHT )
		if ( sBinding.empty() )
		{
			LogInfo( LOG_CATEGORY_INPUT, "Skipping (%s) as there's no equivalent controller component for this binding", Path() );
			return XR_SUCCESS;
		}

		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sBinding.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sBinding.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( LOG_CATEGORY_INPUT, "Added binding path: (%s) for: (%s)", sBinding.c_str(), Path() );
		return XR_SUCCESS;
	}
} // namespace oxr
