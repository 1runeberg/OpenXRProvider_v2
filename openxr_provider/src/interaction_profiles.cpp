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

#include <provider/interaction_profiles.hpp>

namespace oxr
{
	XrResult Controller::AddBinding( XrInstance xrInstance, XrAction action, std::string sFullBindingPath )
	{
		// Convert binding to path
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, sFullBindingPath.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Error adding binding path [%s]: (%s) for: (%s)", XrEnumToString( xrResult ), sFullBindingPath.c_str(), Path() );
			return xrResult;
		}

		XrActionSuggestedBinding suggestedBinding {};
		suggestedBinding.action = action;
		suggestedBinding.binding = xrPath;

		vecSuggestedBindings.push_back( suggestedBinding );

		LogInfo( LOG_CATEGORY_INPUT, "Added binding path: (%s) for: (%s)", sFullBindingPath.c_str(), Path() );
		return XR_SUCCESS;
	}

	XrResult Controller::SuggestControllerBindings( XrInstance xrInstance, void *pOtherInfo )
	{
		// Convert interaction profile path to an xrpath
		XrPath xrPath = XR_NULL_PATH;
		XrResult xrResult = xrStringToPath( xrInstance, Path(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Error converting interaction profile to an xrpath (%s): %s", XrEnumToString( xrResult ), Path() );
			return xrResult;
		}

		XrInteractionProfileSuggestedBinding xrInteractionProfileSuggestedBinding { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		xrInteractionProfileSuggestedBinding.next = pOtherInfo;
		xrInteractionProfileSuggestedBinding.interactionProfile = xrPath;
		xrInteractionProfileSuggestedBinding.suggestedBindings = vecSuggestedBindings.data();
		xrInteractionProfileSuggestedBinding.countSuggestedBindings = static_cast< uint32_t >( vecSuggestedBindings.size() );

		xrResult = xrSuggestInteractionProfileBindings( xrInstance, &xrInteractionProfileSuggestedBinding );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Error suggesting bindings (%s) for %s", XrEnumToString( xrResult ), Path() );
		}

		LogInfo( LOG_CATEGORY_INPUT, "All action bindings sent to runtime for: (%s)", Path() );
		return xrResult;
	}

	XrResult ValveIndex::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::PrimaryButton:
			{
				sBinding += k_pccA;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::SecondaryButton:
			{
				sBinding += k_pccB;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
			}
			break;
			case Controller::Component::Squeeze:
			{
				sBinding += k_pccSqueeze;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
				else
					sBinding += k_pccForce;
			}
			break;
			case Controller::Component::Menu:
			case Controller::Component::System:
			{
				sBinding += k_pccSystem;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
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

	XrResult OculusTouch::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::PrimaryButton:
			{
				sBinding += ( hand == XR_HAND_LEFT_EXT ) ? k_pccX : k_pccA;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::SecondaryButton:
			{
				sBinding += ( hand == XR_HAND_LEFT_EXT ) ? k_pccY : k_pccB;
				if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
			}
			break;
			case Controller::Component::Squeeze:
			{
				sBinding += k_pccSqueeze;
				if ( qualifier == Controller::Qualifier::Value )
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::Menu:
				if ( hand == XR_HAND_LEFT_EXT )
				{
					sBinding += k_pccMenu;
					sBinding += k_pccClick;
				}
				else
				{
					sBinding.clear();
				}
				break;
			case Controller::Component::System:
				if ( hand == XR_HAND_RIGHT_EXT )
				{
					sBinding += k_pccSystem;
					sBinding += k_pccClick;
				}
				else
				{
					sBinding.clear();
				}
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
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

	XrResult HTCVive::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
			{
				sBinding += k_pccTrigger;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else
					sBinding += k_pccValue;
			}
			break;
			case Controller::Component::PrimaryButton:
			case Controller::Component::SecondaryButton:
				sBinding.clear();
				break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccTrackpad;
				if ( qualifier == Controller::Qualifier::Click )
					sBinding += k_pccClick;
				else if ( qualifier == Controller::Qualifier::Touch )
					sBinding += k_pccTouch;
				else if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
			}
			break;
			case Controller::Component::Squeeze:
				sBinding += k_pccSqueeze;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Menu:
				sBinding += k_pccMenu;
				sBinding += k_pccClick;
				break;
			case Controller::Component::System:
				sBinding += k_pccSystem;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
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

	XrResult MicrosoftMixedReality::AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier )
	{
		std::string sBinding = ( hand == XR_HAND_LEFT_EXT ) ? k_pccLeftHand : k_pccRightHand;
		sBinding += ( component == Controller::Component::Haptic ) ? k_pccOutput : k_pccInput;

		// Map requested binding configuration for this controller
		switch ( component )
		{
			case Controller::Component::GripPose:
				sBinding += k_pccGripPose;
				break;
			case Controller::Component::AimPose:
				sBinding += k_pccAimPose;
				break;
			case Controller::Component::Trigger:
				sBinding += k_pccTrigger;
				sBinding += k_pccValue;
				break;
			case Controller::Component::PrimaryButton:
			case Controller::Component::SecondaryButton:
				sBinding.clear();
				break;
			case Controller::Component::AxisControl:
			{
				sBinding += k_pccThumbstick;
				if ( qualifier == Controller::Qualifier::X )
					sBinding += k_pccX;
				else if ( qualifier == Controller::Qualifier::Y )
					sBinding += k_pccY;
				else if ( qualifier == Controller::Qualifier::None )
					break;
				else
					sBinding += k_pccClick;
			}
			break;
			case Controller::Component::Squeeze:
				sBinding += k_pccSqueeze;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Menu:
				sBinding += k_pccMenu;
				sBinding += k_pccClick;
				break;
			case Controller::Component::System:
				sBinding += k_pccSystem;
				sBinding += k_pccClick;
				break;
			case Controller::Component::Haptic:
				sBinding += k_pccHaptic;
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
