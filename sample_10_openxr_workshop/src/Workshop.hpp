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

// Generated by cmake by populating project_config.h.in
#include "project_config.h"

// Openxr template app
#include <app.hpp>

// App defines
#define APP_NAME "openxr_workshop"
#define ENGINE_NAME "openxr_provider"
#define LOG_CATEGORY_APP "openxr_workshop"

using namespace oxr;

namespace oxa
{
	class Workshop : public XrApp
	{
	  public:
		// Create app from template
		Workshop();

		// Destroy app
		~Workshop();

		// Interfaces - XrApp
#ifdef XR_USE_PLATFORM_ANDROID
		XrResult Start( struct android_app *app ) override;
#else
		XrResult Start() override;
#endif

		// Game loop condition
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

		void CacheExtensions();

		void Prep();

		// Game loop
		void RunGameLoop();

		// Session handling
		void ProcessSessionStateChanges();

		// Custom events handling
		void ProcessExtensionEvents();

		// Callback handling
		void PrepareRender( uint32_t unSwapchainIndex, uint32_t unImageIndex );
		void SubmitRender( uint32_t unSwapchainIndex, uint32_t unImageIndex );
		void RegisterRenderCallbacks();

		// Custom graphics pipelines
		void PrepareCustomShapesPipelines();
		void PrepareCustomGraphicsPipelines();

		// ... custom pipelines here ...
		void PrepareFloorGridPipeline();
		void PrepareStencilPipelines();

		// Renderables
		void AddSceneAssets();

		// Input handling
		XrResult CreateActionsets();
		XrResult CreateActions();
		XrResult SuggestBindings();
		XrResult AttachActionsets();
		XrResult AddActionsetsForSync();

		// Gestures
		inline bool IsTwoHandedGestureActive(
			XrHandJointEXT leftJointA,
			XrHandJointEXT leftJointB,
			XrHandJointEXT rightJointA,
			XrHandJointEXT rightJointB,
			XrVector3f *outReferencePosition_Left,
			XrVector3f *outReferencePosition_Right,
			bool *outActivated,
			float *fCacheValue );

		inline void SmoothLocoGesture();

		// Portal gesture handling
		inline void Clap();
		inline bool IsPortalAdjustmentActive( XrVector3f *outThumbPosition_Left, XrVector3f *outThumbPosition_Right );
		inline void AdjustPortalShear( float fCurrentPortalShearValue );
		inline void AdjustPortalShear();
		inline void AdjustPortalShear_Controllers();
		inline void RevealSecretRoom();
		inline void CyclePortal();
		inline void ResetPassthrough();
		inline void CheckPlayerLeftRoom( uint32_t unCurrentRoom );

		// Supported extensions
		struct supported_exts
		{
			oxr::ExtFBPassthrough *fbPassthrough = nullptr;
			oxr::ExtFBRefreshRate *fbRefreshRate = nullptr;
		} workshopExtensions;

		// Graphics pipelines
		struct graphics_pipelines
		{
			uint32_t unGridGraphicsPipeline = 0;
			uint32_t unStencilMask = 0;
			uint32_t unStencilFill = 0;
			uint32_t unStencilOut = 0;
		} workshopPipelines;

		// Level assets - renderable indices (Scene, Sector, Model) in the level

		// struct level_shapes{} workshopShapes;

		struct level_scenes
		{
			// xr
			uint32_t unSpotMask = 0;

			// vr
			uint32_t unFloorSpot = 0;

			// passthrough only
			uint32_t unGridFloor = 0;
			uint32_t unGridFloorHighlight = 0;

			// level - locomotion
			uint32_t unAncientRuins = 0;
			uint32_t unPortal = 0;
			uint32_t unRoom1 = 0;
			uint32_t unRoom2 = 0;

		} workshopScenes;

		enum class EPortalState
		{
			PortalOff = 1,
			Portal1 = 2,
			Portal2 = 3,
			PortalEMax
		};

		struct mechanics
		{
			bool bPlayerInSecretRoom = false;
			bool bIsRoom1Current = true;   // todo: should really be an enum or vector
			bool bColorPassthrough = true; // todo: should really be an enum so we can cycle through more fx

			// Gestures
			float fClapActivationThreshold = 0.07f;
			bool bClapActive = false;

			// Portal
			EPortalState eCurrentPortalState = EPortalState::PortalOff;
			float fCurrentPortalShearValue = 0.0f;
			float fPreviousPortalShearValue = 0.0f;
			float fPortalShearingStride = 0.1f;
			float fPortalMaxScale = 14.95f;
			float fPortalMinScale = 1.0f;
			float fPortalMaxShear = 4.08f;
			float fPortalMinShear = 0.25f;
			float fPortalEnlargeFactor = 3.5f;
			float fPortalShrinkFactor = 0.25f;

			// Portal - Controllers
			bool bPortalControllers = false;
			bool bPortalLeft = false;
			bool bPortalRight = false;

			// Portal - Gestures
			bool bPortalGesture = false;
			float fPortalValueOnActivation = 0.f;
			float fGestureActivationThreshold = 0.025f;
			XrVector3f vec3fThumbPosLeft = { 0.f, 0.f, 0.f };
			XrVector3f vec3fThumbPosRight = { 0.f, 0.f, 0.f };

		} workshopMechanics;

		// struct level_sectors{} workshopSectors;
		// struct level_models{} workshopModels;

		// Actionsets
		struct actionsets
		{
			oxr::ActionSet *locomotion = nullptr;
			oxr::ActionSet *portal = nullptr;
		} workshopActionsets;

		// Actions
		struct actions
		{
			oxr::Action *vec2SmoothLoco = nullptr;
			oxr::Action *bEnableSmoothLoco = nullptr;
			oxr::Action *bShearPortal = nullptr;
			oxr::Action *bCyclePortal = nullptr;
			oxr::Action *bTogglePassthroughModes = nullptr;
		} workshopActions;

		// Locomotion parameters
		struct locomotion_params
		{
			float fSmoothActivationPoint = 0.5f;

			float fSmoothFwd = 0.1f;
			float fSmoothBack = 0.1f;
			float fSmoothLeft = 0.075f;
			float fSmoothRight = 0.075f;
		} locomotionParams;

		// Android events handling
#ifdef XR_USE_PLATFORM_ANDROID
		void AndroidPoll();
#endif

	  private:
		bool m_bProcessRenderFrame = false;
		bool m_bProcessInputFrame = false;

		oxr::RenderImageCallback releaseCallback;
		oxr::RenderImageCallback waitCallback;

		// data from extensions
		float m_fCurrentRefreshRate = 90.f;
	};

} // namespace oxa
