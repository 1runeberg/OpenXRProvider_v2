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

#include "common.hpp"
#define LOG_CATEGORY_INPUT "OpenXRProvider-Input"

namespace oxr
{
	struct Controller
	{
	  public:
		const char *k_pccLeftHand = "/user/hand/left";
		const char *k_pccRightHand = "/user/hand/right";
		const char *k_pccInput = "/input";
		const char *k_pccOutput = "/output";

		const char *k_pccTrigger = "/trigger";
		const char *k_pccThumbstick = "/thumbstick";
		const char *k_pccTrackpad = "/trackpad";
		const char *k_pccSqueeze = "/squeeze";
		const char *k_pccMenu = "/menu";
		const char *k_pccSystem = "/system";

		const char *k_pccGripPose = "/grip/pose";
		const char *k_pccAimPose = "/aim/pose";
		const char *k_pccHaptic = "/haptic";

		const char *k_pccClick = "/click";
		const char *k_pccTouch = "/touch";
		const char *k_pccValue = "/value";
		const char *k_pccForce = "/force";

		const char *k_pccA = "/a";
		const char *k_pccB = "/b";
		const char *k_pccX = "/x";
		const char *k_pccY = "/y";

		enum class Component
		{
			GripPose = 1,
			AimPose = 2,
			Trigger = 3,
			PrimaryButton = 4,
			SecondaryButton = 5,
			AxisControl = 6,
			Squeeze = 7,
			Menu = 8,
			System = 9,
			Haptic = 10,
			ComponentEMax
		};

		enum class Qualifier
		{
			None = 0,
			Click = 1,
			Touch = 2,
			Value = 3,
			Force = 4,
			X = 5,
			Y = 6,
			Grip = 7,
			Haptic = 8,
			ComponentEMax
		};

		std::vector< XrActionSuggestedBinding > vecSuggestedBindings;

		virtual const char *Path() = 0;
		virtual XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) = 0;
		virtual XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) = 0;

		XrResult AddBinding( XrInstance xrInstance, XrAction action, std::string sFullBindingPath );
		XrResult SuggestControllerBindings( XrInstance xrInstance, void *pOtherInfo );
	};

	struct HTCVive : Controller
	{
		const char *Path() override { return "/interaction_profiles/htc/vive_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings(XrInstance xrInstance, void* pOtherInfo) override { return SuggestControllerBindings(xrInstance, pOtherInfo); };
	};

	struct MicrosoftMixedReality : Controller
	{
		const char *Path() override { return "/interaction_profiles/microsoft/motion_controller"; }
		XrResult AddBinding(XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct OculusTouch : Controller
	{
		const char *Path() override { return "/interaction_profiles/oculus/touch_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct ValveIndex : public Controller
	{
		const char *Path() override { return "/interaction_profiles/valve/index_controller"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override;
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct ViveTracker : public Controller
	{
	  public:
		const char *k_pccTracker = "/user/vive_tracker_htcx/role";

		const char *k_pccHandheldObject = "/handheld_object";
		const char *k_pccLeftFoot = "/left_foot";
		const char *k_pccRightFoot = "/right_foot";
		const char *k_pccLeftShoulder = "/left_shoulder";
		const char *k_pccRightShoulder = "/right_shoulder";
		const char *k_pccLeftElbow = "/left_elbow";
		const char *k_pccRightElbow = "/right_elbow";
		const char *k_pccLeftKnee = "/left_knee";
		const char *k_pccRightKnee = "/right_knee";
		const char *k_pccWaist = "/waist";
		const char *k_pccChest = "/chest";
		const char *k_pccCamera = "/camera";
		const char *k_pccKeyboard = "/keyboard";

		enum class RolePath
		{
			HandheldObject,
			LeftFoot,
			RightFoot,
			LeftShoulder,
			RightShoulder,
			LeftElbow,
			RightElbow,
			LeftKnee,
			RightKnee,
			Waist,
			Chest,
			Camera,
			Keyboard
		};

		const char *Path() override { return "/interaction_profiles/htc/vive_tracker_htcx"; }
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, oxr::Controller::Component component, oxr::Controller::Qualifier qualifier ) override
		{
			return XR_ERROR_FEATURE_UNSUPPORTED;
		};
		virtual XrResult AddBinding( XrInstance xrInstance, XrAction action, RolePath role, oxr::Controller::Component component, oxr::Controller::Qualifier qualifier );
		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override { return SuggestControllerBindings( xrInstance, pOtherInfo ); };
	};

	struct BaseController : Controller
	{
	  public:
		const char *Path() override { return "base"; }

		XrResult xrResult = XR_SUCCESS;
		XrResult AddBinding( XrInstance xrInstance, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier ) override
		{
			for ( auto &interactionProfile : vecSupportedControllers )
			{
				xrResult = interactionProfile->AddBinding( xrInstance, action, hand, component, qualifier );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					return xrResult;
			}

			return XR_SUCCESS;
		}

		XrResult SuggestBindings( XrInstance xrInstance, void *pOtherInfo ) override
		{
			for ( auto &interactionProfile : vecSupportedControllers )
			{
				xrResult = interactionProfile->SuggestBindings( xrInstance, pOtherInfo );

				if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
					return xrResult;
			}

			return XR_SUCCESS;
		}

		std::vector< Controller * > vecSupportedControllers;
	};
} // namespace oxr
