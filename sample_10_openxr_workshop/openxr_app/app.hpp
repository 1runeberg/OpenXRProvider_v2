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

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <openxr_provider.h>
#include <xrvk/xrvk.hpp>

#include <physics.hpp>

namespace oxr
{
	class XrApp
	{

	  public:
		XrApp();

		~XrApp();

#ifdef XR_USE_PLATFORM_ANDROID
		XrResult Init(
			struct android_app *app,
			std::string sAppName,
			OxrVersion32 unVersion,
			ELogLevel eLogLevel = ELogLevel::LogDebug,
			bool bCreateRenderer = true,
			bool bCreateSession = true,
			bool bCreateInput = true );
#else
		XrResult Init( std::string sAppName, OxrVersion32 unVersion, ELogLevel eLogLevel = ELogLevel::LogDebug, bool bCreateRenderer = true, bool bCreateSession = true, bool bCreateInput = true );
#endif
		void Cleanup();

		// Getters
		// Pointer to renderer
		xrvk::Render *GetRender() { return m_pRender; }

		// Pointer to session handling object of the openxr provider library
		oxr::Session *GetSession() { return m_pSession; }

		// Pointer to input handling object of the openxr provider library
		oxr::Input *GetInput() { return m_pInput; }

		// Event data packet sent by the openxr runtime during polling
		XrEventDataBaseHeader *GetEventData() { return m_xrEventDataBaseheader; }

		// Directions
		XrVector3f GetForwardVector( XrQuaternionf *quat );
		XrVector3f GetUpVector( XrQuaternionf *quat );
		XrVector3f GetLeftVector( XrQuaternionf *quat );

		XrVector3f FlipVector( XrVector3f *vec );

		XrVector3f GetBackVector( XrQuaternionf *quat );
		XrVector3f GetDownVector( XrQuaternionf *quat );
		XrVector3f GetRightVector( XrQuaternionf *quat );

		// Utility functions
		void AddDebugMeshes();

		inline void AddVectors( XrVector3f *vecInOut, XrVector3f *vecAdd )
		{
			vecInOut->x += vecAdd->x;
			vecInOut->y += vecAdd->y;
			vecInOut->z += vecAdd->z;
		}

		inline void MultiplyVectors(XrVector3f* vecInOut, XrVector3f* vecMultiplier) 
		{
			vecInOut->x *= vecMultiplier->x;
			vecInOut->y *= vecMultiplier->y;
			vecInOut->z *= vecMultiplier->z;
		}

		inline void ScaleVector( XrVector3f *vecInOut, float fScale )
		{
			vecInOut->x *= fScale;
			vecInOut->y *= fScale;
			vecInOut->z *= fScale;
		}

		inline void UpdateHandJoints( XrHandEXT hand, XrHandJointLocationEXT *handJoints )
		{
			uint8_t unOffset = hand == XR_HAND_LEFT_EXT ? 0 : XR_HAND_JOINT_COUNT_EXT;

			for ( uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++ )
			{
				if ( ( handJoints[ i ].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) != 0 )
				{
					m_pRender->vecShapes[ i + unOffset ]->pose = handJoints[ i ].pose;
					m_pRender->vecShapes[ i + unOffset ]->scale = { handJoints[ i ].radius, handJoints[ i ].radius, handJoints[ i ].radius };
				}
			}
		}

		inline void UpdateHandTrackingPoses()
		{
			if ( m_extHandTracking )
			{
				// Update the hand joints poses for this frame
				m_extHandTracking->LocateHandJoints( XR_HAND_LEFT_EXT, m_pSession->GetAppSpace(), m_xrFrameState.predictedDisplayTime );
				m_extHandTracking->LocateHandJoints( XR_HAND_RIGHT_EXT, m_pSession->GetAppSpace(), m_xrFrameState.predictedDisplayTime );

				// Retrieve updated hand poses
				XrHandJointLocationsEXT *leftHand = m_extHandTracking->GetHandJointLocations( XR_HAND_LEFT_EXT );
				XrHandJointLocationsEXT *rightHand = m_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT );

				// Finally, update cube poses representing the hand joints
				if ( leftHand->isActive )
					UpdateHandJoints( XR_HAND_LEFT_EXT, leftHand->jointLocations );

				if ( rightHand->isActive )
					UpdateHandJoints( XR_HAND_RIGHT_EXT, m_extHandTracking->GetHandJointLocations( XR_HAND_RIGHT_EXT )->jointLocations );
			}
		}

		// Interfaces
#ifdef XR_USE_PLATFORM_ANDROID
		virtual XrResult Start( struct android_app *app ) = 0;

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
#else
		virtual XrResult Start() = 0;
#endif

		// Requested extensions
		std::vector< const char * > vecRequestedExtensions;

		// Requested texture formats
		std::vector< int64_t > vecRequestedTextureFormats;
		std::vector< int64_t > vecRequestedDepthFormats;

		// Pose states
		bool bGripLeftIsActive = false;
		bool bGripRightIsActive = false;
		bool bAimLeftIsActive = false;
		bool bAimRightIsActive = false;

		// Haptic params
		uint64_t unHapticDuration = XR_MIN_HAPTIC_DURATION;
		float fHapticAmplitude = 0.5f;
		float fHapticFrequency = XR_FREQUENCY_UNSPECIFIED;

		// Minimum set of supported controllers
		BaseController baseController {};
		ValveIndex controllerIndex {};
		OculusTouch controllerTouch {};
		HTCVive controllerVive {};
		MicrosoftMixedReality controllerMR {};

	  protected:
		// Pointer to the openxr provider library
		std::unique_ptr< Provider > m_pProvider = nullptr;

		// Pointer to renderer
		xrvk::Render *m_pRender = nullptr;

		// Pointer to session handling object of the openxr provider library
		oxr::Session *m_pSession = nullptr;

		// Pointer to input handling object of the openxr provider library
		oxr::Input *m_pInput = nullptr;

		// Future for input thread
		std::future< XrResult > m_inputThread;

		// Event data packet sent by the openxr runtime during polling
		XrEventDataBaseHeader *m_xrEventDataBaseheader = nullptr;

		// Current openxr session state
		XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;

		// Latest openxr framestate - this is filled in on the render call
		XrFrameState m_xrFrameState { XR_TYPE_FRAME_STATE };

		// Projection views and layers to be rendered
		std::vector< XrCompositionLayerProjectionView > m_vecFrameLayerProjectionViews;
		std::vector< XrCompositionLayerBaseHeader * > m_vecFrameLayers;

		// Projection plane
		float m_fNearZ = 0.1f;
		float m_fFarZ = 10000.f;

		// Default controller actions
		oxr::ActionSet *m_pActionsetMain = nullptr;
		oxr::Action *m_pActionGripPose = nullptr;
		oxr::Action *m_pActionAimPose = nullptr;
		oxr::Action *m_pActionHaptic = nullptr;

		// Minimal extensions
		oxr::ExtHandTracking *m_extHandTracking = nullptr; // Hand tracking extension implementation, if present

		// Debug shape
		bool m_bDrawDebug = false;
		Shapes::Shape m_debugShape;

		// Debug pyramid
		std::vector< unsigned short > g_vecPyramidIndices = {
			0,
			1,
			2, // Back (+Z)
			3,
			4,
			5, // Left (-X)
			6,
			7,
			8, // Right (+X)
			9,
			10,
			11, // Bottom (-Y)
		};

		std::vector< Shapes::Vertex > g_vecPyramidVertices = {
			PYRAMID_SIDE( Shapes::BASE_L, Shapes::TOP, Shapes::BASE_R, Shapes::OpenXRPurple )	  // Back (+Z)
			PYRAMID_SIDE( Shapes::BASE_L, Shapes::TIP, Shapes::TOP, Shapes::BeyondRealityYellow ) // Left (-X)
			PYRAMID_SIDE( Shapes::BASE_R, Shapes::TOP, Shapes::TIP, Shapes::HomageOrange )		  // Right (+X)
			PYRAMID_SIDE( Shapes::BASE_R, Shapes::TIP, Shapes::BASE_L, Shapes::CyberCyan )		  // Bottom (-Y)
		};
	};

} // namespace oxr
