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

#include <provider/common.hpp>
#include <provider/ext_handtracking.hpp>

namespace oxr
{
	ExtHandTracking::ExtHandTracking( XrInstance xrInstance, XrSession xrSession )
		: ExtBase( XR_EXT_HAND_TRACKING_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		assert( xrSession != XR_NULL_HANDLE );

		m_xrInstance = xrInstance;
		m_xrSession = xrSession;
	}

	ExtHandTracking::~ExtHandTracking()
	{
		PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT = nullptr;
		XrResult xrResult = xrGetInstanceProcAddr( m_xrInstance, "xrDestroyHandTrackerEXT", ( PFN_xrVoidFunction * )&xrDestroyHandTrackerEXT );

		if ( m_HandTracker_Left )
			xrResult = xrDestroyHandTrackerEXT( m_HandTracker_Left );

		if ( xrResult == XR_SUCCESS )
			LogInfo( LOG_CATEGORY_HANDTRACKING, "Left Hand Tracker destroyed." );

		if ( m_HandTracker_Right )
			xrResult = xrDestroyHandTrackerEXT( m_HandTracker_Right );

		if ( xrResult == XR_SUCCESS )
			LogInfo( LOG_CATEGORY_HANDTRACKING, "Right Hand Tracker destroyed." );
	}

	XrResult ExtHandTracking::Init()
	{
		// Get function pointer to create hand trackers
		PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT = nullptr;
		XrResult xrResult = xrGetInstanceProcAddr( m_xrInstance, "xrCreateHandTrackerEXT", ( PFN_xrVoidFunction * )&xrCreateHandTrackerEXT );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Create hand trackers
		XrHandTrackerCreateInfoEXT xrHandTrackerCreateInfo { XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
		xrHandTrackerCreateInfo.next = nullptr;
		xrHandTrackerCreateInfo.hand = XR_HAND_LEFT_EXT;
		xrHandTrackerCreateInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

		// Left hand
		xrResult = xrCreateHandTrackerEXT( m_xrSession, &xrHandTrackerCreateInfo, &m_HandTracker_Left );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Right hand
		xrHandTrackerCreateInfo.hand = XR_HAND_RIGHT_EXT;
		xrResult = xrCreateHandTrackerEXT( m_xrSession, &xrHandTrackerCreateInfo, &m_HandTracker_Right );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Setup hand joint velocities
		m_xrVelocities_Left.jointCount = XR_HAND_JOINT_COUNT_EXT;
		m_xrVelocities_Left.jointVelocities = &m_XRHandJointVelocities_Left[ 0 ];

		m_xrVelocities_Right.jointCount = XR_HAND_JOINT_COUNT_EXT;
		m_xrVelocities_Right.jointVelocities = &m_XRHandJointVelocities_Right[ 0 ];

		// Setup hand joint locations
		m_xrLocations_Left.jointCount = XR_HAND_JOINT_COUNT_EXT;
		m_xrLocations_Left.jointLocations = &m_XRHandJointsData_Left[ 0 ];
		m_xrLocations_Left.next = nullptr;

		m_xrLocations_Right.jointCount = XR_HAND_JOINT_COUNT_EXT;
		m_xrLocations_Right.jointLocations = &m_XRHandJointsData_Right[ 0 ];
		m_xrLocations_Right.next = nullptr;

		// Cache function pointer for locate hand joints
		return xrGetInstanceProcAddr( m_xrInstance, "xrLocateHandJointsEXT", ( PFN_xrVoidFunction * )&xrLocateHandJointsEXT );
	}

	bool ExtHandTracking::LocateHandJoints( XrHandEXT eHand, XrSpace xrSpace, XrTime xrTime, XrHandJointsMotionRangeEXT eMotionrange )
	{
		assert( xrLocateHandJointsEXT );

		bool bIsLeftHand = eHand == XR_HAND_LEFT_EXT;

		// Check if app actually wants to grab hand joints data for this hand
		if ( bIsLeftHand && !IsActive_Left() )
			return false;
		else if ( !bIsLeftHand && !IsActive_Right() )
			return false;

		// Check if app wants velocity data
		if ( bIsLeftHand )
			m_xrLocations_Left.next = IncludeVelocities_Left() ? &m_xrVelocities_Left : nullptr;
		else
			m_xrLocations_Right.next = IncludeVelocities_Right() ? &m_xrVelocities_Right : nullptr;

		// Finally, get the hand joints data
		XrHandJointsLocateInfoEXT xrHandJointsLocateInfo { XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
		xrHandJointsLocateInfo.baseSpace = xrSpace;
		xrHandJointsLocateInfo.time = xrTime;

		// Check for motion range
		// xrHandJointsLocateInfo.next = NULL;
		// if ( eMotionrange == XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT )
		//{
		//	XrHandJointsMotionRangeInfoEXT xrHandJointsMotionRangeInfo { XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT };
		//	xrHandJointsMotionRangeInfo.handJointsMotionRange = eMotionrange;
		//	xrHandJointsLocateInfo.next = &xrHandJointsMotionRangeInfo;
		//}

		XrResult xrResult = xrLocateHandJointsEXT( bIsLeftHand ? m_HandTracker_Left : m_HandTracker_Right, &xrHandJointsLocateInfo, bIsLeftHand ? &m_xrLocations_Left : &m_xrLocations_Right );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogWarning( LOG_CATEGORY_HANDTRACKING, "Unable to retrieve handtracking data in this frame: %s", XrEnumToString( xrResult ) );
			return false;
		}

		return true;
	}

} // namespace oxr
