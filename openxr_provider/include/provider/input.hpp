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
#include "interaction_profiles.hpp"
#include <map>

namespace oxr
{
	class Session;
	struct ActionSet;
	struct Action
	{
		XrActionType xrActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

		XrAction xrActionHandle = XR_NULL_HANDLE;

		ActionSet *pActionSet = nullptr;

		std::mutex mutexActionState;

		union ActionState
		{
			XrActionStateBoolean stateBoolean;
			XrActionStateFloat stateFloat;
			XrActionStateVector2f stateVector2f;
			XrActionStatePose statePose;
		};

		std::vector< ActionState > vecActionStates;
		std::vector< XrPath > vecSubactionpaths;
		std::vector< XrSpace > vecActionSpaces;

		Action( XrActionType actionType )
			: xrActionType( actionType )
		{
		}
		~Action();

		bool IsActive( uint32_t unActionStateIndex = 0 );
		void SetActionStateType( uint32_t unActionStateIndex = 0 );

		XrResult Init( XrInstance xrInstance, ActionSet *pActionSet, std::string sName, std::string sLocalizedName, std::vector< std::string > vecSubpaths = {}, void *pOtherInfo = nullptr );

		XrResult AddSubActionPath( std::vector< XrPath > &outSubactionPaths, XrInstance xrInstance, std::string sPath );
	};

	struct ActionSet
	{
		XrActionSet xrActionSetHandle = XR_NULL_HANDLE;
		uint32_t unPriority = 0;

		std::vector< Action * > vecActions;

		ActionSet() {}

		~ActionSet()
		{
			if ( xrActionSetHandle != XR_NULL_HANDLE )
			{
				xrDestroyActionSet( xrActionSetHandle );
			}
		}

		XrResult Init( XrInstance xrInstance, std::string sName, std::string sLocalizedName, uint32_t unPriority = 0, void *pOtherInfo = nullptr )
		{
			assert( xrInstance != XR_NULL_HANDLE );

			if ( xrActionSetHandle != XR_NULL_HANDLE )
				return XR_SUCCESS;

			XrActionSetCreateInfo xrActionSetCreateInfo { XR_TYPE_ACTION_SET_CREATE_INFO };
			xrActionSetCreateInfo.next = pOtherInfo;
			xrActionSetCreateInfo.priority = unPriority;
			std::strncpy( xrActionSetCreateInfo.actionSetName, sName.c_str(), XR_MAX_ACTION_SET_NAME_SIZE );
			std::strncpy( xrActionSetCreateInfo.localizedActionSetName, sLocalizedName.c_str(), XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE );

			XrResult xrResult = xrCreateActionSet( xrInstance, &xrActionSetCreateInfo, &xrActionSetHandle );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				LogError( LOG_CATEGORY_INPUT, "Error creating actionset %s : %s", sName.c_str(), XrEnumToString( xrResult ) );
				return xrResult;
			}

			LogInfo( LOG_CATEGORY_INPUT, "Actionset created (%s) : %s", sName.c_str(), sLocalizedName.c_str() );
			return XR_SUCCESS;
		}
	};

	class Session;
	class Input
	{
	  public:
		static const uint8_t k_unMaxInputThreads = 4;

		Input( oxr::Instance *pInstance, ELogLevel eLoglevel = ELogLevel::LogInfo );
		~Input();

		void Init( oxr::Session *pSession );

		XrResult CreateActionSet( ActionSet *outActionSet, std::string sName, std::string sLocalizedName, uint32_t unPriority = 0, void *pOtherInfo = nullptr );

		XrResult CreateAction(
			Action *outAction,
			ActionSet *pActionSet,
			XrActionType actionType,
			std::string sName,
			std::string sLocalizedName,
			std::vector< std::string > vecSubpaths = {},
			void *pOtherInfo = nullptr );

		XrResult CreateActionSpace( Action *outAction, XrPosef *poseInSpace, std::string subpath = "", void *pOtherInfo = nullptr );

		XrResult CreateActionSpaces( Action *outAction, XrPosef *poseInSpace, void *pOtherInfo = nullptr );

		XrResult AddBinding( Controller *controller, XrAction action, XrHandEXT hand, Controller::Component component, Controller::Qualifier qualifier );

		XrResult AddBinding( Controller *controller, XrAction action, std::string sFullBindingPath );

		XrResult SuggestBindings( Controller *controller, void *pOtherInfo );

		XrResult StringToXrPath( const char *string, XrPath *xrPath );

		XrResult XrPathToString( std::string &outString, XrPath *xrPath );

		XrResult AddActionsetForSync( ActionSet *pActionSet, std::string subpath = "" );

		XrResult RemoveActionsetForSync( ActionSet *pActionSet, std::string subpath );

		XrResult AttachActionSetsToSession( XrActionSet *arrActionSets, uint32_t unActionSetCount );

		XrResult ProcessInput();

		XrResult GetActionPose( XrSpaceLocation *outSpaceLocation, Action *pAction, uint32_t unSpaceIndex, XrTime xrTime );

		XrResult GetActionState( Action *pAction );

		const char *GetCurrentInteractionProfile( const char *sUserPath );

		XrResult GenerateHaptic( XrAction xrAction, uint64_t nDuration = XR_MIN_HAPTIC_DURATION, float fAmplitude = 0.5f, float fFrequency = XR_FREQUENCY_UNSPECIFIED );

	  private:
		// Current severity for logging
		ELogLevel m_eMinLogLevel = ELogLevel::LogInfo;

		// Category to use for logging
		std::string m_sLogCategory = LOG_CATEGORY_INPUT;

		// Pointer to instance state from the Provider class
		Instance *m_pInstance = nullptr;

		Session *m_pSession = nullptr;

		std::mutex m_mutexFutureIndex;
		uint8_t m_unFutureIndex = 0;
		std::array< std::future< XrResult >, k_unMaxInputThreads > m_arrFutures;

		// Active action sets
		std::vector< XrActiveActionSet > m_vecXrActiveActionSets;
		std::vector< ActionSet * > m_vecActiveActionSets;
	};

} // namespace oxr
