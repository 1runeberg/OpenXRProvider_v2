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

#pragma once

#include <provider/common.hpp>
#include <provider/ext_base.hpp>
#include <provider/interaction_profiles.hpp>

#define LOG_CATEGORY_EXTVIVETRACKER "HTCXViveTrackerInteraction"

namespace oxr
{
	struct ActionSet;
	struct Action;
	class Input;
	class ExtHTCXViveTrackerInteraction : public oxr::ExtBase
	{
	  public:
		inline static const char *sInteractionProfilePath = "/interaction_profiles/htc/vive_tracker_htcx";
		inline static const char *sUserPath = "/user/vive_tracker_htcx";

		inline static const char *sRoleHandheld = "/role/handheld_object";
		inline static const char *sRoleFootLeft = "/role/left_foot";
		inline static const char *sRoleFootRight = "/role/right_foot";
		inline static const char *sRoleShoulderLeft = "/role/left_shoulder";
		inline static const char *sRoleShoulderRight = "/role/right_shoulder";
		inline static const char *sRoleElbowLeft = "/role/left_elbow";
		inline static const char *sRoleElbowRight = "/role/right_elbow";
		inline static const char *sRoleKneeLeft = "/role/left_knee";
		inline static const char *sRoleKneeRight = "/role/right_knee";
		inline static const char *sRoleWaist = "/role/waist";
		inline static const char *sRoleChest = "/role/chest";
		inline static const char *sRoleCamera = "/role/camera";
		inline static const char *sRoleKeyboard = "/role/keyboard";

		enum ETrackerRole
		{
			Handheld = 0,
			FootLeft = 1,
			FootRight = 2,
			ShoulderLeft = 3,
			ShoulderRight = 4,
			ElbowLeft = 5,
			ElbowRight = 6,
			KneeLeft = 7,
			KneeRight = 8,
			Waist = 9,
			Chest = 10,
			Camera = 11,
			Keyboard = 12,
			TrackerRoleMax
		};

		/// <summary>
		/// HTC Vive Tracker interaction extension - requires a valid openxr instance and session (may or may not be running)
		/// </summary>
		/// <param name="xrInstance">Active openxr instance</param>
		/// <param name="xrSession">Active openxr session (may or may not be running)</param>
		ExtHTCXViveTrackerInteraction( XrInstance xrInstance, XrSession xrSession );

		~ExtHTCXViveTrackerInteraction();

		/// <summary>
		/// Initializes this extension and optionally create default action and corresponding action spaces for all possible tracker roles
		/// </summary>
		/// <param name="pInput">optional oxr::Input provider object to create the default action and corresponding actionspaces for all possible tracker roles</param>
		/// <param name="pActionset">optional actionset to use for the default action</param>
		/// <param name="sLocalizedActionName">optional localized action name for the default action</param>
		/// <returns>Result from the openxr runtime</returns>
		XrResult Init( oxr::Input* pInput = nullptr, oxr::ActionSet *pActionset = nullptr, std::string sLocalizedActionName = "Tracker Poses" );

		/// <summary>
		/// Create a default action and corresponding action spaces for all possible tracker roles
		/// An active session is REQUIRED
		/// </summary>
		/// <param name="pInput">oxr::Input provider object to create the default action and corresponding actionspaces for all possible tracker roles</param>
		/// <param name="pActionset">Actionset to use for the default action</param>
		/// <param name="sLocalizedActionName">optional localized action name for the default action</param>
		void SetupAllTrackerRoles( oxr::Input* pInput, oxr::ActionSet *pActionset, std::string sLocalizedActionName = "Tracker Poses" );

		/// <summary>
		/// Get the full openxr role path from provide role (e.g. waist)
		/// </summary>
		/// <param name="sRole"></param>
		/// <returns></returns>
		const char *GetRolePath( const char *sRole );

		/// <summary>
		/// Place in the output vector all possible tracker role paths (e.g. "/user/vive_tracker_htcx/role/handheld_object" )
		/// </summary>
		/// <param name="vecRolePaths">Output vector that will contain all possible tracker role paths as per openxr spec</param>
		void GetAllRolePaths( std::vector< std::string > &vecRolePaths );

		/// <summary>
		/// Retrieves all connected trackers as reported by the currently active openxr runtime
		/// </summary>
		/// <param name="vecConnectedTrackers">Out vector of XrViveTrackerPathsHTCX that will hold the active trackers as reported by the runtime</param>
		/// <returns></returns>
		XrResult GetAllConnectedTrackers( std::vector< XrViveTrackerPathsHTCX > &vecConnectedTrackers );

		/// <summary>
		/// Cleanup all internal objects
		/// </summary>
		void Cleanup();

		// Tracker pose action that will have all the role subpaths - this can be auto-populated during Init() and is intentionally public for advanced uses
		oxr::Action *pTrackerAction = XR_NULL_HANDLE;

		// Action spaces for all possible tracker poses - this can be auto-populated during Init() and is intentionally public for advanced uses. 
		// use ETrackerRole as index
		std::vector< XrSpace > vecActionSpaces;

	  private:
		// Active openxr instance
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// Active openxr session
		XrSession m_xrSession = XR_NULL_HANDLE;

		// Placeholder for role generation
		std::string m_sRole;

		// Cached function pointer to the main EnumberViveTrackerPaths call from the active openxr runtime
		PFN_xrEnumerateViveTrackerPathsHTCX xrEnumerateViveTrackerPathsHTCX = nullptr;
	};

} // namespace oxr
