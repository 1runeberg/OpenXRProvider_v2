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

#include "ext_base.hpp"
#define LOG_CATEGORY_HANDTRACKING "HandTracking"

namespace oxr
{
	class ExtHandTracking : public ExtBase
	{
	  public:
		/// <summary>
		/// Hand tracking extension implementation - requires a valid openxr instance and session (may or may not be running)
		/// </summary>
		/// <param name="xrInstance">Active openxr instance</param>
		/// <param name="xrSession">Active openxr session (may or may not be running)</param>
		ExtHandTracking( XrInstance xrInstance, XrSession xrSession );

		~ExtHandTracking();

		/// <summary>
		/// Get the last retrieved hand joint locations. LocateHandJoints must be called separately - ideally once per frame.
		/// </summary>
		/// <param name="eHand">Hand (left/right) for which hand joint locations are to be retrieved for</param>
		/// <returns>Hand joint locations for the requested hand</returns>
		XrHandJointLocationsEXT *GetHandJointLocations( XrHandEXT eHand )
		{
			if ( eHand == XR_HAND_LEFT_EXT )
				return &m_xrLocations_Left;

			return &m_xrLocations_Right;
		}

		/// <summary>
		/// Get the last retrieved hand joint velocities. LocateHandJoints must be called separately - ideally once per frame.
		/// </summary>
		/// <param name="eHand">Hand (left/right) for which hand joint velocities are to be tretrieved for</param>
		/// <returns></returns>
		XrHandJointVelocitiesEXT *GetHandJointVelocities( XrHandEXT eHand )
		{
			if ( eHand == XR_HAND_LEFT_EXT )
				return &m_xrVelocities_Left;

			return &m_xrVelocities_Right;
		}

		/// <summary>
		/// Creates the hand trackers for both left and right hands and caches the main LocateHandJoints() function call from the runtime
		/// </summary>
		/// <returns>Result from the runtime</returns>
		XrResult Init();

		/// <summary>
		/// Retrieve the latest hand joint locations from the active openxr runtime
		/// </summary>
		/// <param name="eHand">Hand (left/right) to retrieve hand joint locations for</param>
		/// <param name="xrSpace">Reference space to use</param>
		/// <param name="xrTime">Predicted display time</param>
		/// <param name="eMotionrange">Optional motion range if the Hand Motion Range extension is also active</param>
		/// <returns>True if the hand joint locations were succesfully retrieved from the runtime, false otherwise</returns>
		bool LocateHandJoints( XrHandEXT eHand, XrSpace xrSpace, XrTime xrTime, XrHandJointsMotionRangeEXT eMotionrange = XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT );

		/// <summary>
		/// If hand tracking is active for the left hand
		/// </summary>
		/// <returns>True if hand tracking is active for the left hand, false otherwise</returns>
		bool IsActive_Left() const { return bIsHandTrackingActive_Left; }

		/// <summary>
		/// Set whether hand tracking will be active for the left hand
		/// </summary>
		/// <param name="val">new value</param>
		void IsActive_Left( bool val ) { bIsHandTrackingActive_Left = val; }

		/// <summary>
		/// If hand tracking is active for the right hand
		/// </summary>
		/// <returns>True if hand tracking is active for the right hand, false otherwise</returns>
		bool IsActive_Right() const { return bIsHandTrackingActive_Right; }

		/// <summary>
		/// Set whether hand tracking will be active for the right hand
		/// </summary>
		/// <param name="val">new value</param>
		void IsActive_Right( bool val ) { bIsHandTrackingActive_Right = val; }

		/// <summary>
		/// If velocities for the left hand will be retrieved when LocateHandJointLocations() is called 
		/// </summary>
		/// <returns>True if velocities for the left hand will be retrieved when LocateHandJointLocations() is called, false otherwise</returns>
		bool IncludeVelocities_Left() const { return bGetHandJointVelocities_Left; }

		/// <summary>
		/// Set whether velocities for the left hand will be retrieved when LocateHandJointLocations() is called 
		/// </summary>
		/// <param name="val">new value</param>
		void IncludeVelocities_Left( bool val ) { bGetHandJointVelocities_Left = val; }

		/// <summary>
		/// If velocities for the right hand will be retrieved when LocateHandJointLocations() is called 
		/// </summary>
		/// <returns>True if velocities for the right hand will be retrieved when LocateHandJointLocations() is called, false otherwise</returns>
		bool IncludeVelocities_Right() const { return bGetHandJointVelocities_Right; }

		/// <summary>
		/// Set whether velocities for the right hand will be retrieved when LocateHandJointLocations() is called 
		/// </summary>
		/// <param name="val">new value</param>
		void IncludeVelocities_Right( bool val ) { bGetHandJointVelocities_Right = val; }


	  private:

		// Active openxr instance
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// Active openxr session
		XrSession m_xrSession = XR_NULL_HANDLE;

		// Current state of hand tracking for the left hand
		bool bIsHandTrackingActive_Left = true;

		// Current state of hand tracking for the right hand
		bool bIsHandTrackingActive_Right = true;

		// Current state of hand velocities for the left hand
		bool bGetHandJointVelocities_Left = false;

		// Current state of hand tracking for the right hand
		bool bGetHandJointVelocities_Right = false;

		// Hand joint locations of the left hand (per joint)
		XrHandJointLocationEXT m_XRHandJointsData_Left[ XR_HAND_JOINT_COUNT_EXT ];

		// Hand joint locations of the right hand (per joint)
		XrHandJointLocationEXT m_XRHandJointsData_Right[ XR_HAND_JOINT_COUNT_EXT ];

		// Hand joint velocities of the left hand (per joint)
		XrHandJointVelocityEXT m_XRHandJointVelocities_Left[ XR_HAND_JOINT_COUNT_EXT ];

		// Hand joint velocities of the right hand (per joint)
		XrHandJointVelocityEXT m_XRHandJointVelocities_Right[ XR_HAND_JOINT_COUNT_EXT ];

		// Hand joint velocities of the left hand 
		XrHandJointVelocitiesEXT m_xrVelocities_Left { XR_TYPE_HAND_JOINT_VELOCITIES_EXT };

		// Hand joint velocities of the right hand
		XrHandJointVelocitiesEXT m_xrVelocities_Right { XR_TYPE_HAND_JOINT_VELOCITIES_EXT };

		// Hand joint locations of the left hand
		XrHandJointLocationsEXT m_xrLocations_Left { XR_TYPE_HAND_JOINT_LOCATIONS_EXT };

		// Hand joint locations of the right hand
		XrHandJointLocationsEXT m_xrLocations_Right { XR_TYPE_HAND_JOINT_LOCATIONS_EXT };

		// The openxr hand tracker handle for the left hand
		XrHandTrackerEXT m_HandTracker_Left = XR_NULL_HANDLE;

		// The openxr hand tracker handle for the right hand
		XrHandTrackerEXT m_HandTracker_Right = XR_NULL_HANDLE;

		// Cached function pointer to the main LocateHandJoints call from the active openxr runtime
		PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT = nullptr;
	};

} // namespace oxr
