/* Copyright 2023 Charlotte Gore (GitHub: https://github.com/charlottegore)
 * and Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
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

#include <assert.h>
#include <provider/ext_htcxtracker.hpp>
#include <provider/input.hpp>

inline void Callback_Void( oxr::Action *pAction, uint32_t unActionStateIndex ) { return; }

namespace oxr
{
	ExtHTCXViveTrackerInteraction::ExtHTCXViveTrackerInteraction( XrInstance xrInstance, XrSession xrSession )
		: ExtBase( XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		assert( xrSession != XR_NULL_HANDLE );

		m_xrInstance = xrInstance;
		m_xrSession = xrSession;
	}

	XrResult ExtHTCXViveTrackerInteraction::Init( oxr::Input *pInput, oxr::ActionSet *pActionset, std::string sLocalizedActionName /* "Tracker Poses" */ )
	{
		XrResult xrResult = XR_SUCCESS;

		// Initialize all function pointers for this extension
		xrResult = INIT_PFN( m_xrInstance, xrEnumerateViveTrackerPathsHTCX );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Initialize default action and spaces if app provided an input pointer and actionset during init
		// this allows the app the opportunity to opt-out of default actions and spaces if it wants to do all these manually
		if ( pInput && pActionset )
			SetupAllTrackerRoles( pInput, pActionset, sLocalizedActionName );

		return XR_SUCCESS;
	}

	void ExtHTCXViveTrackerInteraction::SetupAllTrackerRoles( oxr::Input *pInput, oxr::ActionSet *pActionset, std::string sLocalizedActionName /*= "Tracker Poses" */ )
	{
		// Get all roles paths - we will use these as subpaths for the pose action
		// so this single action can track all possible tracker poses
		std::vector< std::string > vecRolePaths;
		GetAllRolePaths( vecRolePaths );

		// Create pose action
		pTrackerAction = new oxr::Action( XR_ACTION_TYPE_POSE_INPUT, Callback_Void );
		pInput->CreateAction( pTrackerAction, pActionset, "tracker_pose", sLocalizedActionName, vecRolePaths );

		// Create action spaces
		vecActionSpaces.resize( ETrackerRole::TrackerRoleMax, XR_NULL_PATH );
		XrPosef originPose {};
		originPose.orientation.w = 1.0f;

		for ( uint32_t i = 0; i < ETrackerRole::TrackerRoleMax; i++ )
		{
			pInput->CreateActionSpace( pTrackerAction, &originPose, vecRolePaths[ i ] );
		}
	}

	const char *ExtHTCXViveTrackerInteraction::GetRolePath( const char *sRole )
	{
		m_sRole = sUserPath;
		m_sRole += sRole;

		return m_sRole.c_str();
	}

	void ExtHTCXViveTrackerInteraction::GetAllRolePaths( std::vector< std::string > &vecRolePaths )
	{
		vecRolePaths.resize( ETrackerRole::TrackerRoleMax );

		vecRolePaths[ ETrackerRole::Handheld ] = GetRolePath( sRoleHandheld );
		vecRolePaths[ ETrackerRole::FootLeft ] = GetRolePath( sRoleFootLeft );
		vecRolePaths[ ETrackerRole::FootRight ] = GetRolePath( sRoleFootRight );
		vecRolePaths[ ETrackerRole::ShoulderLeft ] = GetRolePath( sRoleShoulderLeft );
		vecRolePaths[ ETrackerRole::ShoulderRight ] = GetRolePath( sRoleShoulderRight );
		vecRolePaths[ ETrackerRole::ElbowLeft ] = GetRolePath( sRoleElbowLeft );
		vecRolePaths[ ETrackerRole::ElbowRight ] = GetRolePath( sRoleElbowRight );
		vecRolePaths[ ETrackerRole::KneeLeft ] = GetRolePath( sRoleKneeLeft );
		vecRolePaths[ ETrackerRole::KneeRight ] = GetRolePath( sRoleKneeRight );
		vecRolePaths[ ETrackerRole::Waist ] = GetRolePath( sRoleWaist );
		vecRolePaths[ ETrackerRole::Chest ] = GetRolePath( sRoleChest );
		vecRolePaths[ ETrackerRole::Camera ] = GetRolePath( sRoleCamera );
		vecRolePaths[ ETrackerRole::Keyboard ] = GetRolePath( sRoleKeyboard );
	}

	XrResult ExtHTCXViveTrackerInteraction::GetAllConnectedTrackers( std::vector< XrViveTrackerPathsHTCX > &vecConnectedTrackers )
	{
		assert( m_xrInstance != XR_NULL_HANDLE );

		// Clear output vector
		vecConnectedTrackers.clear();

		// Retrieve capacity from openxr runtime
		uint32_t unCapacityOut = 0;
		XrResult xrResult = xrEnumerateViveTrackerPathsHTCX( m_xrInstance, 0, &unCapacityOut, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogDebug( LOG_CATEGORY_EXTVIVETRACKER, "Unable to retrieve capacity for xrEnumerateViveTrackerPathsHTCX: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		if ( unCapacityOut == 0 )
			return XR_SUCCESS;

		// Retrieve active trackers
		uint32_t unCapacityIn = unCapacityOut;
		unCapacityOut = 0;
		xrResult = xrEnumerateViveTrackerPathsHTCX( m_xrInstance, unCapacityIn, &unCapacityOut, vecConnectedTrackers.data() );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogDebug( LOG_CATEGORY_EXTVIVETRACKER, "Unable to retrieve active trackers: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	void ExtHTCXViveTrackerInteraction::Cleanup()
	{
		// Destroy action spaces
		for ( auto &space : vecActionSpaces )
		{
			if ( space != XR_NULL_HANDLE )
				xrDestroySpace( space );
		}

		// Destroy action
		if ( pTrackerAction )
		{
			delete pTrackerAction;
			pTrackerAction = nullptr;
		}
	}

	ExtHTCXViveTrackerInteraction::~ExtHTCXViveTrackerInteraction() { Cleanup(); }
} // namespace oxr
