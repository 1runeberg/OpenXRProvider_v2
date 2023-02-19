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

// App defines
#define APP_NAME "sample_07_finger_painting2"
#define ENGINE_NAME "openxr_provider"
#define LOG_CATEGORY_DEMO "OpenXRProviderDemo"

// Generated by cmake by populating project_config.h.in
#include "project_config.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <openxr_provider.h>
#include <xrvk/xrvk.hpp>

#include "xrvk/xrvk.hpp"

// Event data packet sent by the openxr runtime during polling
XrEventDataBaseHeader *g_xrEventDataBaseheader = nullptr;

// Pointer to session handling object of the openxr provider library
oxr::Session *g_pSession = nullptr;

// Pointer to input handling object of the openxr provider library
oxr::Input *g_pInput = nullptr;

// Future for input thread
std::future< XrResult > g_inputThread;

// Current openxr session state
XrSessionState g_sessionState = XR_SESSION_STATE_UNKNOWN;

// Latest openxr framestate - this is filled in on the render call
XrFrameState g_xrFrameState { XR_TYPE_FRAME_STATE };

// Projection views and layers to be rendered
std::vector< XrCompositionLayerProjectionView > g_vecFrameLayerProjectionViews;
std::vector< XrCompositionLayerBaseHeader * > g_vecFrameLayers;

// Pointer to the the xrvk renderer class
std::unique_ptr< xrvk::Render > g_pRender = nullptr;

// ====
// OpenXR Reference Cube
// SPDX-License-Identifier: Apache-2.0
std::vector< unsigned short > g_vecCubeIndices = {
	0,	1,	2,	3,	4,	5,	// -X
	6,	7,	8,	9,	10, 11, // +X
	12, 13, 14, 15, 16, 17, // -Y
	18, 19, 20, 21, 22, 23, // +Y
	24, 25, 26, 27, 28, 29, // -Z
	30, 31, 32, 33, 34, 35, // +Z
};

std::vector< Shapes::Vertex > g_vecCubeVertices = {
	CUBE_SIDE( Shapes::LTB, Shapes::LBF, Shapes::LBB, Shapes::LTB, Shapes::LTF, Shapes::LBF, Shapes::DarkRed )	 // -X
	CUBE_SIDE( Shapes::RTB, Shapes::RBB, Shapes::RBF, Shapes::RTB, Shapes::RBF, Shapes::RTF, Shapes::Red )		 // +X
	CUBE_SIDE( Shapes::LBB, Shapes::LBF, Shapes::RBF, Shapes::LBB, Shapes::RBF, Shapes::RBB, Shapes::DarkGreen ) // -Y
	CUBE_SIDE( Shapes::LTB, Shapes::RTB, Shapes::RTF, Shapes::LTB, Shapes::RTF, Shapes::LTF, Shapes::Green )	 // +Y
	CUBE_SIDE( Shapes::LBB, Shapes::RBB, Shapes::RTB, Shapes::LBB, Shapes::RTB, Shapes::LTB, Shapes::DarkBlue )	 // -Z
	CUBE_SIDE( Shapes::LBF, Shapes::LTF, Shapes::RTF, Shapes::LBF, Shapes::RTF, Shapes::RBF, Shapes::Blue )		 // +Z
};
// ==== end of OpenXR Reference Cube

// Hand tracking extension implementation, if present
oxr::ExtHandTracking *g_extHandTracking = nullptr;

// FB Passthrough extension implementation, if present
oxr::ExtFBPassthrough *g_extFBPassthrough = nullptr;

// Skybox manipulation vars
bool g_bSkyboxScalingActivated = false;
float g_fSkyboxScaleGestureDistanceOnActivate = 0.0f;

// Passthrough adjustments
bool g_bSaturationAdjustmentActivated = false;
float g_fSaturationValueOnActivation = 0.0f;
float g_fCurrentSaturationValue = 0.0f;

// Clap mechanic and passthrough effects
bool g_bClapActive = false;
uint16_t g_unPassthroughFXCycleStage = 0;

// Holds the state for painting using the aim pose of the controller
struct
{
	bool bIsActive = false;
	bool bCurrentState = false;
} g_ActionPainterLeft, g_ActionPainterRight;

// Reference to the pose action (used for aim pose painting)
oxr::Action *g_ControllerPoseAction = nullptr;

// Reference to haptic action
oxr::Action *g_hapticAction = nullptr;

// Renderable index for the hands
uint32_t g_unLeftHandIndex = 0;
uint32_t g_unRightHandIndex = 0;

// State for extra cubemaps - 360 panorama photos (for passthrough backup)
bool g_bCyclingPanoramas = false;
uint32_t g_unCurrentPanoramaIndex = 0;

#ifdef _WIN32 
	std::chrono::steady_clock::time_point g_xrLastPanoramaScaleTime;
#elif __linux__
    std::chrono::_V2::system_clock::time_point g_xrLastPanoramaScaleTime;
#endif

std::vector< uint32_t > g_vecPanoramaIndices;

// Passthrough styles
enum class EPassthroughFXMode
{
	EPassthroughFXMode_None = 0,
	EPassthroughFXMode_EdgeOn = 1,
	EPassthroughFXMode_Amber = 2,
	EPassthroughFXMode_Purple = 3,
	EPassthroughMode_Max
} g_eCurrentPassthroughFXMode;

// Gesture constants
static const float k_fGestureActivationThreshold = 0.025f;
static const float k_fPaintGestureActivationThreshold = 0.015f;
static const float k_fClapActivationThreshold = 0.07f;
static const float k_fSkyboxScalingStride = 0.05f;
static const float k_fSaturationAdjustmentStride = 0.1f;

// Pressie
bool g_bShowPressie = false;
uint32_t g_unPressieIndex = 0;

#ifdef _WIN32 
	std::chrono::steady_clock::time_point g_xrLastPressieTime;
#elif __linux__
    std::chrono::_V2::system_clock::time_point g_xrLastPressieTime;
#endif

XrQuaternionf g_quatRotation = { 0.0f, 0.0087265f, 0.0f, 1.0f }; // 1 degree in the y-axis

// Painting constants
// Cube vertices for finger painting
constexpr XrVector3f LBB { -0.5f, -0.5f, -0.5f };
constexpr XrVector3f LBF { -0.5f, -0.5f, 0.5f };
constexpr XrVector3f LTB { -0.5f, 0.5f, -0.5f };
constexpr XrVector3f LTF { -0.5f, 0.5f, 0.5f };
constexpr XrVector3f RBB { 0.5f, -0.5f, -0.5f };
constexpr XrVector3f RBF { 0.5f, -0.5f, 0.5f };
constexpr XrVector3f RTB { 0.5f, 0.5f, -0.5f };
constexpr XrVector3f RTF { 0.5f, 0.5f, 0.5f };
constexpr XrVector3f colorCyan { 0, 1, 1 };

std::vector< Shapes::Vertex > g_vecPaintCubeVertices = {
	CUBE_SIDE( LTB, LBF, LBB, LTB, LTF, LBF, colorCyan ) // -X
	CUBE_SIDE( RTB, RBB, RBF, RTB, RBF, RTF, colorCyan ) // +X
	CUBE_SIDE( LBB, LBF, RBF, LBB, RBF, RBB, colorCyan ) // -Y
	CUBE_SIDE( LTB, RTB, RTF, LTB, RTF, LTF, colorCyan ) // +Y
	CUBE_SIDE( LBB, RBB, RTB, LBB, RTB, LTB, colorCyan ) // -Z
	CUBE_SIDE( LBF, LTF, RTF, LBF, RTF, RBF, colorCyan ) // +Z
};

// Reference shape for painting
Shapes::Shape *g_pReferencePaint = nullptr;

/**
 * These are utility functions for the extensions we will be using in this demo
 */

inline void HideHandShapes()
{
	uint32_t unTotalHandJoints = XR_HAND_JOINT_COUNT_EXT * 2;

	// left hand will use the first XR_HAND_JOINT_COUNT_EXT indices
	// right hand will use specHandJointIndex + XR_HAND_JOINT_COUNT_EXT indices
	for ( uint32_t i = 0; i < unTotalHandJoints; i++ )
	{
		g_pRender->vecShapes[ i ]->scale = { 0.f, 0.f, 0.f };
	}
}

inline void PopulateHandShapes( Shapes::Shape *shapePalm )
{
	assert( g_pRender );
	assert( shapePalm );

	// a cube per joint per hand - we'll match the indices with the hand tracking extension's
	// so we can easily refer to them later on to update the current tracked poses
	uint32_t unTotalHandJoints = XR_HAND_JOINT_COUNT_EXT * 2;
	g_pRender->vecShapes.resize( unTotalHandJoints );

	// zero out the scale so cubes won't immediately appear until after the first frame of poses come in
	shapePalm->scale = { 0.0f, 0.0f, 0.0f };

	// XR_HAND_JOINT_PALM_EXT (0) - Use as a reference
	g_pRender->vecShapes[ XR_HAND_JOINT_PALM_EXT ] = shapePalm;

	// left hand will use the first XR_HAND_JOINT_COUNT_EXT indices
	// right hand will use specHandJointIndex + XR_HAND_JOINT_COUNT_EXT indices
	for ( uint32_t i = 1; i < unTotalHandJoints; i++ )
	{
		g_pRender->vecShapes[ i ] = shapePalm->Duplicate();
	}
}

inline void UpdateHandJoints( XrHandEXT hand, XrHandJointLocationEXT *handJoints )
{
	uint8_t unOffset = hand == XR_HAND_LEFT_EXT ? 0 : XR_HAND_JOINT_COUNT_EXT;

	for ( uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++ )
	{
		if ( ( handJoints[ i ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
		{
			g_pRender->vecShapes[ i + unOffset ]->pose = handJoints[ i ].pose;
			g_pRender->vecShapes[ i + unOffset ]->scale = { handJoints[ i ].radius, handJoints[ i ].radius, handJoints[ i ].radius };
		}
	}
}

inline void UpdateHandTrackingPoses( XrFrameState *frameState )
{
	if ( g_extHandTracking )
	{
		// Update the hand joints poses for this frame
		g_extHandTracking->LocateHandJoints( XR_HAND_LEFT_EXT, g_pSession->GetAppSpace(), frameState->predictedDisplayTime );
		g_extHandTracking->LocateHandJoints( XR_HAND_RIGHT_EXT, g_pSession->GetAppSpace(), frameState->predictedDisplayTime );

		// Retrieve updated hand poses
		XrHandJointLocationsEXT *leftHand = g_extHandTracking->GetHandJointLocations( XR_HAND_LEFT_EXT );
		XrHandJointLocationsEXT *rightHand = g_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT );

		// Finally, update cube poses representing the hand joints
		if ( leftHand->isActive )
			UpdateHandJoints( XR_HAND_LEFT_EXT, leftHand->jointLocations );

		if ( rightHand->isActive )
			UpdateHandJoints( XR_HAND_RIGHT_EXT, g_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT )->jointLocations );
	}
}

inline void Paint( XrPosef pose )
{
	Shapes::Shape *newPaint = g_pReferencePaint->Duplicate();
	newPaint->pose = pose;
	g_pRender->vecShapes.push_back( newPaint );
}

inline void Paint( XrHandEXT hand )
{
	// Check if hand tracking is available
	if ( g_extHandTracking )
	{
		// Get latest hand joints
		XrHandJointLocationsEXT *joints = g_extHandTracking->GetHandJointLocations( hand );

		// Check if index tip and thumb tips have valid locations
		if ( joints->isActive && ( joints->jointLocations[ XR_HAND_JOINT_INDEX_TIP_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 &&
			 ( joints->jointLocations[ XR_HAND_JOINT_THUMB_TIP_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
		{
			// Paint gesture - if index and thumb tips meet
			float fDistance = 0.0f;
			XrVector3f_Distance( &fDistance, &joints->jointLocations[ XR_HAND_JOINT_INDEX_TIP_EXT ].pose.position, &joints->jointLocations[ XR_HAND_JOINT_THUMB_TIP_EXT ].pose.position );

			if ( fDistance < k_fPaintGestureActivationThreshold )
			{
				// Paint from the index tip
				Paint( joints->jointLocations[ XR_HAND_JOINT_INDEX_TIP_EXT ].pose );
			}
		}
	}
}

inline void SetActionPaintCurrentState( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	if ( unActionStateIndex == 0 )
		g_ActionPainterLeft.bCurrentState = pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState;
	else
		g_ActionPainterRight.bCurrentState = pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState;
}

inline void SetActionPaintIsActive( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	if ( unActionStateIndex == 0 )
		g_ActionPainterLeft.bIsActive = pAction->vecActionStates[ unActionStateIndex ].statePose.isActive;
	else
		g_ActionPainterRight.bIsActive = pAction->vecActionStates[ unActionStateIndex ].statePose.isActive;
}

inline bool IsTwoHandedGestureActive(
	XrHandJointEXT leftJointA,
	XrHandJointEXT leftJointB,
	XrHandJointEXT rightJointA,
	XrHandJointEXT rightJointB,
	XrVector3f *outReferencePosition_Left,
	XrVector3f *outReferencePosition_Right,
	bool *outActivated,
	float *fCacheValue )
{
	// Get latest hand joints
	XrHandJointLocationsEXT *leftHand = g_extHandTracking->GetHandJointLocations( XR_HAND_LEFT_EXT );
	XrHandJointLocationsEXT *rightHand = g_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT );

	XrHandJointLocationEXT *leftJoints = leftHand->jointLocations;
	XrHandJointLocationEXT *rightJoints = rightHand->jointLocations;

	// Check if both left and right hands are tracking
	// and the provided joint a and joint b on both hands have valid positions
	if ( leftHand->isActive && rightHand->isActive && ( leftJoints[ leftJointA ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 &&
		 ( leftJoints[ leftJointB ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 && ( rightJoints[ rightJointA ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 &&
		 ( rightJoints[ rightJointB ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
	{
		// Check gesture
		float fDistance = 0.0f;

		*outReferencePosition_Left = leftJoints[ leftJointB ].pose.position;
		XrVector3f_Distance( &fDistance, &leftJoints[ leftJointA ].pose.position, outReferencePosition_Left );

		if ( fDistance < k_fGestureActivationThreshold )
		{
			*outReferencePosition_Right = rightJoints[ rightJointB ].pose.position;
			XrVector3f_Distance( &fDistance, &rightJoints[ rightJointA ].pose.position, outReferencePosition_Right );

			if ( fDistance < k_fGestureActivationThreshold )
			{
				*outActivated = true;
				return true;
			}
		}
	}

	*outActivated = false;
	*fCacheValue = 0.0f;
	return false;
}

inline bool IsSkyboxScalingActive( XrVector3f *outThumbPosition_Left, XrVector3f *outThumbPosition_Right )
{
	// Gesture - middle and thumb tips are touching on both hands
	return IsTwoHandedGestureActive(
		XR_HAND_JOINT_MIDDLE_TIP_EXT,
		XR_HAND_JOINT_THUMB_TIP_EXT,
		XR_HAND_JOINT_MIDDLE_TIP_EXT,
		XR_HAND_JOINT_THUMB_TIP_EXT,
		outThumbPosition_Left,
		outThumbPosition_Right,
		&g_bSkyboxScalingActivated,
		&g_fSkyboxScaleGestureDistanceOnActivate );
}

inline bool IsSaturationAdjustmentActive( XrVector3f *outThumbPosition_Left, XrVector3f *outThumbPosition_Right )
{
	// Gesture - ring and thumb tips are touching on both hands
	return IsTwoHandedGestureActive(
		XR_HAND_JOINT_RING_TIP_EXT,
		XR_HAND_JOINT_THUMB_TIP_EXT,
		XR_HAND_JOINT_RING_TIP_EXT,
		XR_HAND_JOINT_THUMB_TIP_EXT,
		outThumbPosition_Left,
		outThumbPosition_Right,
		&g_bSaturationAdjustmentActivated,
		&g_fSaturationValueOnActivation );
}

inline void ScaleSkybox( float fScaleFactor )
{
	if ( g_pRender->skybox->currentScale.x > 5.0f )
	{
		g_pRender->skybox->currentScale = { 5.0f, 5.0f, 5.0f };
		return;
	}

	if ( g_pRender->skybox->currentScale.x < 0.0f )
	{
		g_pRender->skybox->currentScale = { 0.0f, 0.0f, 0.0f };
		return;
	}

	g_pRender->skybox->currentScale.x += fScaleFactor;
	g_pRender->skybox->currentScale.y = g_pRender->skybox->currentScale.z = g_pRender->skybox->currentScale.x;
}

inline void ScaleSkybox()
{
	// Check for required extensions
	if ( g_extHandTracking == nullptr )
		return;

	// Check if gesture was activated on a previous frame
	bool bGestureActivedOnPreviousFrame = g_bSkyboxScalingActivated;

	// Check if gesture is active in this frame
	XrVector3f leftThumb, rightThumb;
	if ( IsSkyboxScalingActive( &leftThumb, &rightThumb ) )
	{
		// Gesture was activated on this frame, cache the distance
		if ( !bGestureActivedOnPreviousFrame )
		{
			XrVector3f_Distance( &g_fSkyboxScaleGestureDistanceOnActivate, &leftThumb, &rightThumb );
		}

		// Scale skybox based on distance and stride
		float currentDistance = 0.0f;
		XrVector3f_Distance( &currentDistance, &leftThumb, &rightThumb );

		float fGestureDistanceFromPreviousFrame = currentDistance - g_fSkyboxScaleGestureDistanceOnActivate;
		if ( abs( fGestureDistanceFromPreviousFrame ) < k_fSkyboxScalingStride )
			return;

		float fScaleFactor = fGestureDistanceFromPreviousFrame * k_fSkyboxScalingStride;
		ScaleSkybox( fScaleFactor );
	}
}

inline void ActionScaleSkybox( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	if ( pAction->vecActionStates[ unActionStateIndex ].stateFloat.currentState > 0.5f )
	{
		ScaleSkybox( k_fSkyboxScalingStride );
	}
	else if ( pAction->vecActionStates[ unActionStateIndex ].stateFloat.currentState < -0.5f )
	{
		ScaleSkybox( -k_fSkyboxScalingStride );
	}
}

inline void AdjustPassthroughSaturation( float fCurrentSaturationValue )
{
	// Check for required extensions
	if ( g_extFBPassthrough == nullptr )
		return;

	g_fCurrentSaturationValue = g_fCurrentSaturationValue < 0.0f ? 0.0f : fCurrentSaturationValue;
	g_extFBPassthrough->SetModeToBCS( 0.0f, 1.0f, g_fCurrentSaturationValue );
}

inline void AdjustPassthroughSaturation()
{
	if ( g_extHandTracking == nullptr )
		return;

	// Check if gesture was activated on a previous frame
	bool bGestureActivedOnPreviousFrame = g_bSaturationAdjustmentActivated;

	// Check if gesture is active in this frame
	XrVector3f leftThumb, rightThumb;
	if ( IsSaturationAdjustmentActive( &leftThumb, &rightThumb ) )
	{
		// Gesture was activated on this frame, cache the distance
		if ( !bGestureActivedOnPreviousFrame )
		{
			XrVector3f_Distance( &g_fSaturationValueOnActivation, &leftThumb, &rightThumb );
		}

		// Adjust saturation based on distance and stride
		float currentDistance = 0.0f;
		XrVector3f_Distance( &currentDistance, &leftThumb, &rightThumb );

		float fGestureDistanceFromPreviousFrame = currentDistance - g_fSaturationValueOnActivation;
		if ( abs( fGestureDistanceFromPreviousFrame ) < k_fSaturationAdjustmentStride )
			return;

		g_fCurrentSaturationValue = fGestureDistanceFromPreviousFrame * k_fSaturationAdjustmentStride * 80;
		AdjustPassthroughSaturation( g_fCurrentSaturationValue );
	}
}

inline void ActionAdjustSaturation( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	// As there's finer control with thumbsticks vs gestures, we'll lower adjustment stride
	if ( pAction->vecActionStates[ unActionStateIndex ].stateFloat.currentState > 0.5f )
	{
		AdjustPassthroughSaturation( g_fCurrentSaturationValue + ( k_fSaturationAdjustmentStride / 5 ) );
	}
	else if ( pAction->vecActionStates[ unActionStateIndex ].stateFloat.currentState < -0.5f )
	{
		AdjustPassthroughSaturation( g_fCurrentSaturationValue - ( k_fSaturationAdjustmentStride / 5 ) );
	}
}

inline void CyclePassthroughFX()
{
	// Check for required extensions
	if ( g_extFBPassthrough == nullptr || g_extHandTracking == nullptr )
		return;

	switch ( g_eCurrentPassthroughFXMode )
	{
		case EPassthroughFXMode::EPassthroughFXMode_None:
			// Enable edges
			g_extFBPassthrough->SetModeToDefault();
			g_extFBPassthrough->SetPassThroughEdgeColor( { 0.0f, 1.0f, 1.0f, 1.0f } );
			g_eCurrentPassthroughFXMode = EPassthroughFXMode::EPassthroughFXMode_EdgeOn;
			break;
		case EPassthroughFXMode::EPassthroughFXMode_EdgeOn:
			// Set to amber
			g_extFBPassthrough->SetModeToColorMap( true, true, false );
			g_eCurrentPassthroughFXMode = EPassthroughFXMode::EPassthroughFXMode_Amber;
			break;
		case EPassthroughFXMode::EPassthroughFXMode_Amber:
			// Set to purple
			g_extFBPassthrough->SetModeToColorMap( true, false, true );
			g_eCurrentPassthroughFXMode = EPassthroughFXMode::EPassthroughFXMode_Purple;
			break;
		case EPassthroughFXMode::EPassthroughFXMode_Purple:
		case EPassthroughFXMode::EPassthroughMode_Max:
		default:
			// Reset
			g_extFBPassthrough->SetModeToDefault();
			g_eCurrentPassthroughFXMode = EPassthroughFXMode::EPassthroughFXMode_None;
			break;
	}
}

inline void Clap()
{
	// Check for required extensions
	if ( g_extHandTracking == nullptr )
		return;

	// Get latest hand joints
	XrHandJointLocationsEXT *leftHand = g_extHandTracking->GetHandJointLocations( XR_HAND_LEFT_EXT );
	XrHandJointLocationsEXT *rightHand = g_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT );

	// Gesture: palms of left and right hands are touching
	if ( leftHand->isActive && rightHand->isActive && ( leftHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 &&
		 ( rightHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
	{
		float fDistance = 0.0f;
		XrVector3f_Distance( &fDistance, &leftHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].pose.position, &rightHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].pose.position );

		if ( fDistance < k_fClapActivationThreshold )
		{
			// Gesture activated - set the previous clap state for checking on successive frame
			g_bClapActive = true;
			return;
		}

		// Hands are active but not in clap gesture
		if ( g_bClapActive )
		{
			// If previous clap state is true, then perform action
#ifdef XR_USE_PLATFORM_ANDROID
			CyclePassthroughFX();
#else
			g_bCyclingPanoramas = true;
#endif

			g_bClapActive = false;
			return;
		}
	}

	// Hands were inactive or not giving valid data
	g_bClapActive = false;
}

inline void CyclePanoramas()
{
	if ( !g_bCyclingPanoramas || g_pRender->skybox->currentScale.x > 1.0f )
		return;

	// todo: refresh rate ext
	auto time = std::chrono::duration< double, std::milli >( std::chrono::high_resolution_clock::now() - g_xrLastPanoramaScaleTime ).count();

	if ( time <  10000000000 )
	{
		if ( g_unCurrentPanoramaIndex >= g_vecPanoramaIndices.size() )
			g_unCurrentPanoramaIndex = 0;

		// Scale Down
		uint32_t unIndex = g_vecPanoramaIndices[ g_unCurrentPanoramaIndex ];
		g_pRender->vecRenderScenes[ unIndex ]->currentScale = { 0.0f, 0.0f, 0.0f };

		// Scale Up
		unIndex = g_vecPanoramaIndices[ g_unCurrentPanoramaIndex + 1 >= g_vecPanoramaIndices.size() ? 0 : g_unCurrentPanoramaIndex + 1 ];
		if ( g_pRender->vecRenderScenes[ unIndex ]->currentScale.x < 5.0f )
		{
			XrVector3f_Scale( &g_pRender->vecRenderScenes[ unIndex ]->currentScale, 0.025f );
		}
		else
		{
			g_bCyclingPanoramas = false;
			g_unCurrentPanoramaIndex++;
		}

		g_xrLastPanoramaScaleTime = std::chrono::high_resolution_clock::now();
	}
}

inline void ActionCycleFX( oxr::Action *pAction, uint32_t unActionStateIndex )
{
#ifdef XR_USE_PLATFORM_ANDROID
	// we don't really care which hand triggered this action, so just run our internal fx function
	// if current value is true
	if ( pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState )
		CyclePassthroughFX();
#else
	// we don't have reliable passthrough in other platforms as of coding this
	// so we'll cycle 360 panos instead
	if ( pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState )
		g_bCyclingPanoramas = true;
#endif
}

inline void ActionHaptic( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	// Haptic params (these are also params of the GenerateHaptic() function
	// adding here for visibility
	uint64_t unDuration = XR_MIN_HAPTIC_DURATION;
	float fAmplitude = 0.5f;
	float fFrequency = XR_FREQUENCY_UNSPECIFIED;

	// Check if the action has subpaths
	if ( !pAction->vecSubactionpaths.empty() )
	{
		g_pInput->GenerateHaptic( pAction->xrActionHandle, pAction->vecSubactionpaths[ unActionStateIndex ], unDuration, fAmplitude, fFrequency );
	}
	else
	{
		g_pInput->GenerateHaptic( pAction->xrActionHandle, XR_NULL_PATH, unDuration, fAmplitude, fFrequency );
	}
}

inline void ShowPressie()
{
	// Animate
	if ( g_bShowPressie )
	{
		g_pRender->vecRenderSectors[ g_unPressieIndex ]->bIsVisible = true;

		// todo: refresh rate ext
		auto time = std::chrono::duration< double, std::milli >( std::chrono::high_resolution_clock::now() - g_xrLastPressieTime ).count();

		if ( time < 10000000000 )
		{
			// Scale up
			if ( g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentScale.x < 0.05f )
			{
				XrVector3f_Scale( &g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentScale, 0.025f );
			}

			// Rotate
			XrQuaternionf newRot {};
			XrQuaternionf_Multiply( &newRot, &g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.orientation, &g_quatRotation );
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.orientation = newRot;

			g_xrLastPressieTime = std::chrono::high_resolution_clock::now();
		}
	}
	else
	{
		g_pRender->vecRenderSectors[ g_unPressieIndex ]->bIsVisible = false;
	}
}

inline void GestureShowPressie()
{
	// Check controller activity - we'll piggyback on the action state poses stored with hand painting actions
	if ( g_ActionPainterLeft.bIsActive && g_ActionPainterRight.bIsActive )
	{
		// Check if aim poses meet
		float fDistance = 0.0f;
		XrVector3f_Distance( &fDistance, &g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->currentPose.position, &g_pRender->vecRenderSectors[ g_unRightHandIndex ]->currentPose.position );

		// we're using the clap threshold as it's wider than the joint activation threshold,
		// so better for controllers
		if ( fDistance < k_fClapActivationThreshold )
		{
			// Gesture activated - move the pressie at palm of left hand and make it visible
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.position = g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->currentPose.position;
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.position.z -= 0.1f; // move forward a bit
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->bIsVisible = g_bShowPressie = true;

			// trigger haptics
			ActionHaptic( g_hapticAction, 0 ); // left hand - as we've used subpaths in creating the action
			ActionHaptic( g_hapticAction, 1 ); // right hand - as we've used subpaths in creating the action

			return;
		}
	}

	// Otherwise, we need hand tracking for gesture detection
	if ( g_extHandTracking == nullptr )
		return;

	// Get latest hand joints
	XrHandJointLocationsEXT *leftHand = g_extHandTracking->GetHandJointLocations( XR_HAND_LEFT_EXT );
	XrHandJointLocationsEXT *rightHand = g_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT );

	// Gesture: palm of left and index finger of right hand meet
	if ( leftHand->isActive && rightHand->isActive && ( leftHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 &&
		 ( rightHand->jointLocations[ XR_HAND_JOINT_INDEX_TIP_EXT ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
	{
		float fDistance = 0.0f;
		XrVector3f_Distance( &fDistance, &leftHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].pose.position, &rightHand->jointLocations[ XR_HAND_JOINT_INDEX_TIP_EXT ].pose.position );

		if ( fDistance < k_fGestureActivationThreshold )
		{
			// Gesture activated - move the pressie at palm of left hand and make it visible
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.position = leftHand->jointLocations[ XR_HAND_JOINT_PALM_EXT ].pose.position;
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->currentPose.position.y += 0.1f; // move up a bit
			g_pRender->vecRenderSectors[ g_unPressieIndex ]->bIsVisible = g_bShowPressie = true;
			return;
		}
	}
}

/**
 * These are utility functions to check game loop conditions
 * For android, we need to be able to check the app state as well
 * so we can process android events data at the appropriate times.
 */
#ifdef XR_USE_PLATFORM_ANDROID
static void app_handle_cmd( struct android_app *app, int32_t cmd )
{
	oxr::AndroidAppState *appState = ( oxr::AndroidAppState * )app->userData;

	switch ( cmd )
	{
		case APP_CMD_RESUME:
		{
			appState->Resumed = true;
			break;
		}
		case APP_CMD_PAUSE:
		{
			appState->Resumed = false;
			break;
		}
		case APP_CMD_DESTROY:
		{
			appState->NativeWindow = NULL;
			break;
		}
		case APP_CMD_INIT_WINDOW:
		{
			appState->NativeWindow = app->window;
			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			appState->NativeWindow = NULL;
			break;
		}
	}
}

bool CheckGameLoopExit( oxr::Provider *oxrProvider ) { return oxrProvider->Instance()->androidApp->onAppCmd = app_handle_cmd; }

#else
bool CheckGameLoopExit( oxr::Provider *oxrProvider ) { return oxrProvider->Session()->GetState() != XR_SESSION_STATE_EXITING; }
#endif

/**
 * These are callback functions that would be registered with the
 * openxr provider to handle render calls at appropriate times in
 * the application.
 */
void PreRender_Callback( uint32_t unSwapchainIndex, uint32_t unImageIndex )
{
	if ( g_xrFrameState.shouldRender )
	{
		// Hand tracking updates - only if controllers aren't present
		if ( !g_ActionPainterLeft.bIsActive || !g_ActionPainterRight.bIsActive )
		{
			UpdateHandTrackingPoses( &g_xrFrameState );
		}

		// Painting updates - if controller is tracking, use actions, otherwise use hand tracking gestures
		if ( g_ActionPainterLeft.bIsActive )
		{
			g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->bIsVisible = true;
			HideHandShapes();

			if ( g_ActionPainterLeft.bCurrentState )
			{
				XrSpaceLocation xrSpaceLocation { XR_TYPE_SPACE_LOCATION };
				if ( XR_UNQUALIFIED_SUCCESS( g_pInput->GetActionPose( &xrSpaceLocation, g_ControllerPoseAction, 0, g_xrFrameState.predictedDisplayTime ) ) )
					Paint( xrSpaceLocation.pose );
			}
		}
		else
		{
			g_pRender->vecRenderSectors[ g_unLeftHandIndex ]->bIsVisible = false;
			Paint( XR_HAND_LEFT_EXT );
		}

		if ( g_ActionPainterRight.bIsActive )
		{
			g_pRender->vecRenderSectors[ g_unRightHandIndex ]->bIsVisible = true;
			HideHandShapes();

			if ( g_ActionPainterRight.bCurrentState )
			{
				XrSpaceLocation xrSpaceLocation { XR_TYPE_SPACE_LOCATION };
				if ( XR_UNQUALIFIED_SUCCESS( g_pInput->GetActionPose( &xrSpaceLocation, g_ControllerPoseAction, 1, g_xrFrameState.predictedDisplayTime ) ) )
					Paint( xrSpaceLocation.pose );
			}
		}
		else
		{
			g_pRender->vecRenderSectors[ g_unRightHandIndex ]->bIsVisible = false;
			Paint( XR_HAND_RIGHT_EXT );
		}

		// Skybox scaling
		ScaleSkybox();

		// 360 Panoramas
		CyclePanoramas();

		// Pressie
		GestureShowPressie();
		ShowPressie();

		// Render
		g_pRender->BeginRender( g_pSession, g_vecFrameLayerProjectionViews, &g_xrFrameState, unSwapchainIndex, unImageIndex, 0.1f, 10000.f );

		// Passthrough adjustments
		AdjustPassthroughSaturation();
		Clap();
	}
}

void PostRender_Callback( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pRender->EndRender(); }
