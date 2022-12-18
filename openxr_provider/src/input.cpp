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

#include <provider/input.hpp>
#include <provider/session.hpp>

namespace oxr
{
	Action::~Action()
	{
		if ( xrActionHandle != XR_NULL_HANDLE )
		{
			xrDestroyAction( xrActionHandle );
		}
	}

	bool Action::IsActive()
	{
		switch ( xrActionType )
		{
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
				return actionState.stateBoolean.isActive;
				break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
				return actionState.stateFloat.isActive;
				break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
				return actionState.stateVector2f.isActive;
				break;
			case XR_ACTION_TYPE_POSE_INPUT:
				return actionState.statePose.isActive;
				break;
			case XR_ACTION_TYPE_MAX_ENUM:
			default:
				break;
		}

		return false;
	}

	XrResult Action::Init(
		XrInstance xrInstance,
		ActionSet *pActionSet,
		XrActionType actionType,
		std::string sName,
		std::string sLocalizedName,
		std::vector< std::string > vecSubpaths,
		void *pOtherInfo )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		if ( xrActionHandle != XR_NULL_HANDLE )
			return XR_SUCCESS;

		// Get subaction paths if available
		std::vector< XrPath > vecSubactionpaths;

		XrResult xrResult = XR_SUCCESS;
		for ( auto &path : vecSubpaths )
		{
			xrResult = AddSubActionPath( vecSubactionpaths, xrInstance, path );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Create action
		XrActionCreateInfo xrActionCreateInfo { XR_TYPE_ACTION_CREATE_INFO };
		xrActionCreateInfo.next = pOtherInfo;
		strcpy_s( xrActionCreateInfo.actionName, XR_MAX_ACTION_SET_NAME_SIZE, sName.c_str() );
		strcpy_s( xrActionCreateInfo.localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE, sLocalizedName.c_str() );
		xrActionCreateInfo.actionType = xrActionType;

		uint32_t unSubpathsCount = static_cast< uint32_t >( vecSubactionpaths.size() );
		if ( unSubpathsCount > 0 )
		{
			xrActionCreateInfo.countSubactionPaths = unSubpathsCount;
			xrActionCreateInfo.subactionPaths = vecSubactionpaths.data();
		}
		else
		{
			xrActionCreateInfo.countSubactionPaths = 0;
			xrActionCreateInfo.subactionPaths = nullptr;
		}

		xrResult = xrCreateAction( pActionSet->xrActionSetHandle, &xrActionCreateInfo, &xrActionHandle );

		// Add to action set
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			pActionSet->vecActions.push_back( this );
		}

		return xrResult;
	}

	XrResult Action::AddSubActionPath( std::vector< XrPath > &outSubactionPaths, XrInstance xrInstance, std::string sPath )
	{
		assert( xrInstance != XR_NULL_HANDLE );

		XrPath xrPath;
		XrResult xrResult = xrStringToPath( xrInstance, sPath.c_str(), &xrPath );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Error creating an openxr subpath - make sure only allowed charaters are used in the path: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		outSubactionPaths.push_back( xrPath );
		return XR_SUCCESS;
	}

	Input::Input( oxr::Instance *pInstance ) {}

	Input::~Input() {}

	XrResult Input::StringToXrPath( const char *string, XrPath *xrPath )
	{
		assert( xrPath );

		XrResult xrResult = xrStringToPath( m_pInstance->xrInstance, string, xrPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogError( LOG_CATEGORY_INPUT, "Unable to convert %s to an XrPath: %s", string, XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult Input::XrPathToString( std::string &outString, XrPath *xrPath )
	{
		outString.clear();

		uint32_t unCount = 0;
		char sPath[ XR_MAX_PATH_LENGTH ];
		XrResult xrResult = xrPathToString( m_pInstance->xrInstance, *xrPath, sizeof( sPath ), &unCount, sPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_INPUT, "Unable to convert XrPath: %" PRIu64 " to a readable string", *xrPath, XrEnumToString( xrResult ) );
			return xrResult;
		}

		outString = sPath;
		return XR_SUCCESS;
	}

	XrResult Input::AddActionsetForSync( ActionSet *pActionSet, std::string subpath ) 
	{
		XrPath xrPath;
		XrResult xrResult = StringToXrPath(subpath.c_str(), &xrPath);

		if (!XR_UNQUALIFIED_SUCCESS(xrResult))
			return xrResult;

		m_vecActiveActionSets.push_back(pActionSet);
		m_vecXrActiveActionSets.push_back({ pActionSet->xrActionSetHandle, xrPath });

		return XR_SUCCESS;
	}

	XrResult Input::RemoveActionsetForSync( ActionSet *pActionSet, std::string subpath ) 
	{
		XrPath xrPath;
		XrResult xrResult = StringToXrPath( subpath.c_str(), &xrPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Remove from Active ActiveActionsets
		auto end_actionset = 
			std::remove_if( m_vecActiveActionSets.begin(), m_vecActiveActionSets.end(), 
				[pActionSet]( const ActionSet* actionset ) { return actionset == pActionSet; } );

		m_vecActiveActionSets.erase( end_actionset, m_vecActiveActionSets.end() );

		// Remove from Active XrActiveActionsets
		auto end_xractionset = std::remove_if(
			m_vecXrActiveActionSets.begin(),
			m_vecXrActiveActionSets.end(),
			[ pActionSet, xrPath ]( const XrActiveActionSet &activeActionSet ) { return activeActionSet.actionSet == pActionSet->xrActionSetHandle && activeActionSet.subactionPath == xrPath; } );

		m_vecXrActiveActionSets.erase( end_xractionset, m_vecXrActiveActionSets.end() );

		return XR_SUCCESS;
	}

	XrResult Input::AttachActionSetsToSession( XrActionSet *arrActionSets, uint32_t unActionSetCount )
	{
		XrSessionActionSetsAttachInfo xrSessionActionSetsAttachInfo { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
		xrSessionActionSetsAttachInfo.countActionSets = unActionSetCount;
		xrSessionActionSetsAttachInfo.actionSets = arrActionSets;
		
		XrResult xrResult = xrAttachSessionActionSets( m_pSession->GetXrSession(), &xrSessionActionSetsAttachInfo );

		if ( XR_UNQUALIFIED_SUCCESS(xrResult) )
			LogInfo( LOG_CATEGORY_INPUT, "%i action sets attached to this session", unActionSetCount );
		else
			LogError( LOG_CATEGORY_INPUT, "Error attaching action sets to this session: %s", XrEnumToString(xrResult) );

		return xrResult;
	}

	XrResult Input::SyncActionsets()
	{
		if ( m_vecXrActiveActionSets.size() < 1 )
			return XR_SUCCESS;

		// Sync active action sets with openxr runtime
		XrActionsSyncInfo xrActionSyncInfo { XR_TYPE_ACTIONS_SYNC_INFO };
		xrActionSyncInfo.countActiveActionSets = static_cast< uint32_t >( m_vecXrActiveActionSets.size() );
		xrActionSyncInfo.activeActionSets = m_vecXrActiveActionSets.data();

		XrResult xrResult = xrSyncActions( m_pSession->GetXrSession(), &xrActionSyncInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Update action states
		for ( auto &actionset : m_vecActiveActionSets )
		{
			for ( auto &action : actionset->vecActions )
			{
				m_arrFutures[ m_unFutureIndex ] = std::async( std::launch::async, &Input::GetActionState, this, action );

				// if we've hit max threads, wait for first thread to finish
				if ( m_unFutureIndex >= k_unMaxInputThreads )
					m_arrFutures[ 0 ].wait();

				// todo: run action callbacks here

			}
		}

		return xrResult;
	}

	XrResult Input::GetActionPose( XrSpaceLocation *outSpaceLocation, Action *pAction, XrTime xrTime )
	{
		return xrLocateSpace( pAction->xrSpace, m_pSession->GetAppSpace(), xrTime, outSpaceLocation );
	}

	XrResult Input::GetActionState( Action *pAction )
	{
		m_mutexFutureIndex.lock();
		m_unFutureIndex++;
		m_mutexFutureIndex.unlock();

		XrActionStateGetInfo xrActionStateGetInfo { XR_TYPE_ACTION_STATE_GET_INFO };
		xrActionStateGetInfo.action = pAction->xrActionHandle;

		XrResult xrResult = XR_SUCCESS;

		// get the action state from the runtime, block other threads while doing so
		const std::lock_guard< std::mutex > lock( pAction->mutexActionState );
		switch ( pAction->xrActionType )
		{
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
				xrResult = xrGetActionStateBoolean( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->actionState.stateBoolean );
				break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
				xrResult = xrGetActionStateFloat( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->actionState.stateFloat );
				break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
				xrResult = xrGetActionStateVector2f( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->actionState.stateVector2f );
				break;
			case XR_ACTION_TYPE_POSE_INPUT:
				xrResult = xrGetActionStatePose( m_pSession->GetXrSession(), &xrActionStateGetInfo, &pAction->actionState.statePose );
			case XR_ACTION_TYPE_MAX_ENUM:
			default:
				xrResult = XR_ERROR_ACTION_TYPE_MISMATCH;
				break;
		}

		m_mutexFutureIndex.lock();
		m_unFutureIndex--;
		m_mutexFutureIndex.unlock();

		return xrResult;
	}

	const char *Input::GetCurrentInteractionProfile( const char *sUserPath ) 
	{
		XrPath xrPath;
		XrResult xrResult = StringToXrPath( sUserPath, &xrPath );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return "";

		XrInteractionProfileState xrInteractionProfileState { XR_TYPE_INTERACTION_PROFILE_STATE };
		xrResult = xrGetCurrentInteractionProfile( m_pSession->GetXrSession(), xrPath, &xrInteractionProfileState );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return "";

		std::string sInteractionProfile;
		xrResult = XrPathToString(sInteractionProfile, &xrInteractionProfileState.interactionProfile);

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return "";

		LogInfo(LOG_CATEGORY_INPUT, "Current interaction profile (%s) : %s", sUserPath, sInteractionProfile.c_str());
		return sInteractionProfile.c_str();
	}

	XrResult Input::GenerateHaptic( XrAction xrAction, uint64_t nDuration /*= XR_MIN_HAPTIC_DURATION*/, float fAmplitude /*= 0.5f*/, float fFrequency /*= XR_FREQUENCY_UNSPECIFIED */ ) 
	{
		XrHapticVibration xrHapticVibration { XR_TYPE_HAPTIC_VIBRATION };
		xrHapticVibration.duration = nDuration;
		xrHapticVibration.amplitude = fAmplitude;
		xrHapticVibration.frequency = fFrequency;

		XrHapticActionInfo xrHapticActionInfo { XR_TYPE_HAPTIC_ACTION_INFO };
		xrHapticActionInfo.action = xrAction;

		return xrApplyHapticFeedback( m_pSession->GetXrSession(), &xrHapticActionInfo, ( const XrHapticBaseHeader * )&xrHapticVibration );
	}

} // namespace oxr
