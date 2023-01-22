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

#include "Workshop.hpp"

using namespace oxr;

// Callback handling
oxa::Workshop *g_pWorkshop = nullptr;

// Input callbacks
inline void Callback_SmoothLoco( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	// todo: other orientation reference types (e.g. controller, tracker, etc)

	XrVector2f *currentState = &pAction->vecActionStates[ unActionStateIndex ].stateVector2f.currentState;

	// Forward / Back
	if ( currentState->y > g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetForwardVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		dir.y = 0.f; // ignore flight
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothFwd );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
	else if ( currentState->y < -g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetBackVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		dir.y = 0.f; // ignore flight
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothBack );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}

	// Left / Right
	if ( currentState->x > g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetRightVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		dir.y = 0.f; // ignore flight
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothRight );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
	else if ( currentState->x < -g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetLeftVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		dir.y = 0.f; // ignore flight
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothRight );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
}

inline void Callback_EnableSmoothLoco( oxr::Action *pAction, uint32_t unActionStateIndex )
{
	bool bVisibility = pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState;

	// turn on/off pertinent locomotion guides
	if (g_pWorkshop->workshopExtensions.fbPassthrough)
	{
		g_pWorkshop->GetRender()->vecRenderScenes[ g_pWorkshop->workshopScenes.unSpotMask ]->bIsVisible = bVisibility;
		g_pWorkshop->GetRender()->vecRenderScenes[ g_pWorkshop->workshopScenes.unGridFloorHighlight ]->bIsVisible = bVisibility;
		g_pWorkshop->GetRender()->vecRenderScenes[ g_pWorkshop->workshopScenes.unGridFloor ]->bIsVisible = bVisibility;
	}
	else
	{
		g_pWorkshop->GetRender()->vecRenderScenes[g_pWorkshop->workshopScenes.unSpotMask]->bIsVisible = false; // always off in vr
		g_pWorkshop->GetRender()->vecRenderScenes[ g_pWorkshop->workshopScenes.unFloorSpot ]->bIsVisible = true; // always visible in vr

		// Enlarge area 
		if (bVisibility)
		{
			g_pWorkshop->GetRender()->vecRenderScenes[g_pWorkshop->workshopScenes.unFloorSpot]->currentScale = { 100.f , 100.f, 100.f };
		}
		else
		{
			g_pWorkshop->GetRender()->vecRenderScenes[g_pWorkshop->workshopScenes.unFloorSpot]->currentScale = { 3.f , 3.f, 3.f };
		}
	}
}

// Render callbacks
inline void Callback_PreRender( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pWorkshop->PrepareRender( unSwapchainIndex, unImageIndex ); }
inline void Callback_PostRender( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pWorkshop->SubmitRender( unSwapchainIndex, unImageIndex ); }

namespace oxa
{
	Workshop::Workshop() {}

	Workshop::~Workshop()
	{
		// Cleanup actions
		if ( m_pInput )
		{
			if ( workshopActions.vec2SmoothLoco )
				delete workshopActions.vec2SmoothLoco;
		}

		// Cleanup actionsets
		if ( m_pInput )
		{
			if ( workshopActionsets.locomotion )
				delete workshopActionsets.locomotion;
		}

		// Cleanup extensions
		if ( workshopExtensions.fbPassthrough )
			delete workshopExtensions.fbPassthrough;

		if ( workshopExtensions.fbRefreshRate )
			delete workshopExtensions.fbRefreshRate;
	}

#ifdef XR_USE_PLATFORM_ANDROID
	XrResult Workshop::Start( struct android_app *app )
#else
	XrResult Workshop::Start()
#endif
	{
		g_pWorkshop = this; // needed for the global callbacks

		// (1) Create openxr instance and session

		// ... you can add requested extensions with ecRequestedExtensions.push_back( EXT NAME ) ...
		vecRequestedExtensions.push_back( XR_FB_PASSTHROUGH_EXTENSION_NAME );
		vecRequestedExtensions.push_back( XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME );

		// .. you can add requested texture formats in vecRequestedTextureFormats and vecRequestedDepthFormats ...

#ifdef XR_USE_PLATFORM_ANDROID
		XrResult xrResult = Workshop::Init( app, APP_NAME, OXR_MAKE_VERSION32( 0, 1, 0 ) );
#else
		XrResult xrResult = Workshop::Init( APP_NAME, OXR_MAKE_VERSION32( 0, 1, 0 ) );
#endif

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (2) Cache additional extensions this app will use
		CacheExtensions();

		// (3) Add debug assets to render (comment out when ready with your own game assets)
		AddDebugMeshes();

		// (4) ... Add assets to render here (vecRenderScene/Sector/Model) ...
		AddSceneAssets();

		// (5) Start loading gltf assets from disk
		m_pRender->LoadAssets();

		// (6) Prepare pbr pipelines
		m_pRender->PrepareAllPipelines();

		// (7) ... Add custom shapes/basic geometry pipelines here (if any) ...
		PrepareCustomShapesPipelines();

		// (8) ... Add custom graphics pipelines here (if any) ...
		PrepareCustomGraphicsPipelines();

		// (9) Register render call backs - input callbacks are defined when creating an action
		//     update Callback_PreRender and Callback_PostRender with your render code
		RegisterRenderCallbacks();

		// (10) ... Create actionset/s here (template creates a main actionset by default) ...
		xrResult = CreateActionsets();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (11) ... Create action/s here (template creates pose actions by default for the controllers) ...
		xrResult = CreateActions();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (12) ... Suggest binding/s here...
		xrResult = SuggestBindings();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (13) ... Attach actionset/s to session ...
		xrResult = AttachActionsets();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (14) ... Add action set for the next input sync ...
		xrResult = AddActionsetsForSync();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (15) ... Set skybox ...
		m_pRender->SetSkyboxVisibility( true );

		// (16) ... Set additional prep for this app ..
		Prep();

		// (17) Run main loop
		RunGameLoop();

		return XR_SUCCESS;
	}

	void Workshop::CacheExtensions()
	{
#ifdef XR_USE_PLATFORM_ANDROID
		// fb passthrough (with quest pro at least) seems unreliable in PC atm
		workshopExtensions.fbPassthrough = static_cast< oxr::ExtFBPassthrough * >( m_pProvider->Instance()->extHandler.GetExtension( XR_FB_PASSTHROUGH_EXTENSION_NAME ) );
		if ( workshopExtensions.fbPassthrough )
		{
			if ( !XR_UNQUALIFIED_SUCCESS( workshopExtensions.fbPassthrough->Init() ) )
			{
				delete workshopExtensions.fbPassthrough;
				workshopExtensions.fbPassthrough = nullptr;
			}
		}
#endif

		workshopExtensions.fbRefreshRate = static_cast< oxr::ExtFBRefreshRate * >( m_pProvider->Instance()->extHandler.GetExtension( XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME ) );
		if ( workshopExtensions.fbRefreshRate )
		{
			if ( !XR_UNQUALIFIED_SUCCESS( workshopExtensions.fbRefreshRate->Init() ) )
			{
				delete workshopExtensions.fbRefreshRate;
				workshopExtensions.fbRefreshRate = nullptr;
			}
		}
	}

	void Workshop::Prep()
	{
		// Passthrough
		if ( workshopExtensions.fbPassthrough )
		{
			workshopExtensions.fbPassthrough->StartPassThrough();
			workshopExtensions.fbPassthrough->SetModeToDefault();
			m_pRender->SetSkyboxVisibility( false );
		}
		else
		{
			// modify skybox for vr mode 
			m_pRender->SetSkyboxVisibility( true );
			m_pRender->skybox->currentScale = { 10.f, 10.f , 10.f };
		}

		// Refresh rate
		if ( workshopExtensions.fbRefreshRate )
		{
			// Log supported refresh rates (if extension is available and active)
			std::vector< float > vecSupportedRefreshRates;
			workshopExtensions.fbRefreshRate->GetSupportedRefreshRates( vecSupportedRefreshRates );

			oxr::LogDebug( LOG_CATEGORY_APP, "The following %i refresh rate(s) are supported by the hardware and runtime", ( uint32_t )vecSupportedRefreshRates.size() );
			for ( auto &refreshRate : vecSupportedRefreshRates )
				oxr::LogDebug( LOG_CATEGORY_APP, "\tRefresh rate supported: %f", refreshRate );

			// Request best available refresh rate (runtime chooses, specific refresh rate can also be specified here from result of GetSupportedRefreshRates()
			workshopExtensions.fbRefreshRate->RequestRefreshRate( 0.0f );

			// Retrieve current refresh rate
			m_fCurrentRefreshRate = workshopExtensions.fbRefreshRate->GetCurrentRefreshRate();
			oxr::LogDebug( LOG_CATEGORY_APP, "Current display refresh rate is: %f", m_fCurrentRefreshRate );
		}

		// Hide default floor
		if ( workshopExtensions.fbPassthrough )
		{
			m_pRender->vecRenderScenes[ workshopScenes.unFloorSpot ]->bIsVisible = false; // hide in passthrough
		}
		else
		{
			m_pRender->vecRenderScenes[ workshopScenes.unFloorSpot ]->bIsVisible = true; // always show in vr
		}

		// Hide smooth loco guides
		m_pRender->vecRenderScenes[ workshopScenes.unSpotMask ]->bIsVisible = false;
		m_pRender->vecRenderScenes[ workshopScenes.unGridFloorHighlight ]->bIsVisible = false;
		m_pRender->vecRenderScenes[ workshopScenes.unGridFloor ]->bIsVisible = false;
	}

	void Workshop::RunGameLoop()
	{
		XrResult xrResult = XR_SUCCESS;

		while ( CheckGameLoopExit( m_pProvider.get() ) )
		{

#ifdef XR_USE_PLATFORM_ANDROID
			// For android we need in addition, to poll android events to proceed to the session ready state
			AndroidPoll();
#endif
			// (1) Poll runtime for openxr events
			m_xrEventDataBaseheader = m_pProvider->PollXrEvents();

			// (2) Handle events
			if ( m_xrEventDataBaseheader )
			{
				// Handle session state changes
				ProcessSessionStateChanges();

				// Handle extension events
				ProcessExtensionEvents();
			}

			// (3) Input
			if ( m_bProcessInputFrame && m_pInput )
			{
				m_inputThread = std::async( std::launch::async, &oxr::Input::ProcessInput, m_pInput );
			}

			// (4) Render
			if ( m_bProcessRenderFrame )
			{
				XrCompositionLayerFlags xrCompositionLayerFlags = 0;
				m_vecFrameLayers.clear();

				// ...Add additional frame layers here (if any, e.g. passthrough)...
				if ( workshopExtensions.fbPassthrough )
				{
					xrCompositionLayerFlags =
						XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

					m_vecFrameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( workshopExtensions.fbPassthrough->GetCompositionLayer() ) );
				}

				m_vecFrameLayerProjectionViews.resize( m_pSession->GetSwapchains().size(), { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } );
				m_pSession->RenderFrameWithLayers( m_vecFrameLayerProjectionViews, m_vecFrameLayers, &m_xrFrameState, xrCompositionLayerFlags );
			}
		}
	}

	void Workshop::ProcessSessionStateChanges()
	{
		if ( m_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
		{
			m_sessionState = m_pProvider->Session()->GetState();

			if ( m_sessionState == XR_SESSION_STATE_READY )
			{
				// Start session - begin the app's frame loop here
				oxr::LogInfo( LOG_CATEGORY_APP, "App frame loop starts here." );
				XrResult xrResult = m_pProvider->Session()->Begin();
				if ( XR_UNQUALIFIED_SUCCESS( xrResult ) ) // defaults to stereo (vr)
				{
					// Start processing render frames
					m_bProcessRenderFrame = true;
				}
				else
				{
					m_bProcessRenderFrame = false;
					oxr::LogError( LOG_CATEGORY_APP, "Unable to start openxr session (%s)", oxr::XrEnumToString( xrResult ) );
				}
			}
			else if ( m_sessionState == XR_SESSION_STATE_FOCUSED )
			{
				// Start input
				m_bProcessInputFrame = true;
			}
			else if ( m_sessionState == XR_SESSION_STATE_STOPPING )
			{
				// End session - end input
				m_bProcessRenderFrame = m_bProcessInputFrame = false;

				// End session - end the app's frame loop here
				oxr::LogInfo( LOG_CATEGORY_APP, "App frame loop ends here." );
				if ( XR_UNQUALIFIED_SUCCESS( m_pProvider->Session()->End() ) )
					m_bProcessRenderFrame = false;
			}
		}
	}

	void Workshop::ProcessExtensionEvents()
	{
		// Display refresh rate
		if ( workshopExtensions.fbRefreshRate && m_xrEventDataBaseheader->type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB )
		{
			auto xrExtFBRefreshRateChangedEvent = *reinterpret_cast< const XrEventDataDisplayRefreshRateChangedFB * >( m_xrEventDataBaseheader );
			m_fCurrentRefreshRate = xrExtFBRefreshRateChangedEvent.toDisplayRefreshRate;

			oxr::LogDebug( LOG_CATEGORY_APP, "Display refresh rate changed from: %f to %f", xrExtFBRefreshRateChangedEvent.fromDisplayRefreshRate, m_fCurrentRefreshRate );
		}
	}

	void Workshop::RegisterRenderCallbacks()
	{
		// Register render callbacks
		// We'll build the command buffers after acquire and submit to the queue after wait
		// to ensure that the runtime isn't using VkQueue at the same time (most runtimes don't)

		releaseCallback.fnCallback = Callback_PreRender;
		m_pProvider->Session()->RegisterWaitSwapchainImageImageCallback( &releaseCallback );

		waitCallback.fnCallback = Callback_PostRender;
		m_pProvider->Session()->RegisterWaitSwapchainImageImageCallback( &waitCallback );
	}

	void Workshop::PrepareCustomShapesPipelines() {}

	void Workshop::PrepareCustomGraphicsPipelines()
	{
		PrepareFloorGridPipeline();
		PrepareStencilPipelines();
	}

	void Workshop::PrepareFloorGridPipeline()
	{
		assert( m_pRender->GetRenderPasses()[ 0 ] != VK_NULL_HANDLE );

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI {};
		inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCI {};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateCI.depthClampEnable = VK_FALSE;
		rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCI.depthBiasEnable = VK_FALSE;
		rasterizationStateCI.depthBiasConstantFactor = 0;
		rasterizationStateCI.depthBiasClamp = 0;
		rasterizationStateCI.depthBiasSlopeFactor = 0;
		rasterizationStateCI.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState {};
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendStateCI {};
		colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCI.attachmentCount = 1;
		colorBlendStateCI.pAttachments = &blendAttachmentState;
		colorBlendStateCI.logicOpEnable = VK_FALSE;
		colorBlendStateCI.logicOp = VK_LOGIC_OP_NO_OP;
		colorBlendStateCI.blendConstants[ 0 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 1 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 2 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 3 ] = 1.0f;

		VkRect2D scissor = { { 0, 0 }, m_pRender->vkExtent };
		VkViewport viewport = { 0.0f, 0.0f, ( float )m_pRender->vkExtent.width, ( float )m_pRender->vkExtent.height, 0.0f, 1.0f };
		VkPipelineViewportStateCreateInfo viewportStateCI {};
		viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCI.viewportCount = 1;
		viewportStateCI.pViewports = &viewport;
		viewportStateCI.scissorCount = 1;
		viewportStateCI.pScissors = &scissor;

		// TODO: MSAA support
		VkPipelineMultisampleStateCreateInfo multisampleStateCI {};
		multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// TODO: Expose Dynamics states to apps
		std::vector< VkDynamicState > dynamicStateEnables; // = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI {};
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
		dynamicStateCI.dynamicStateCount = static_cast< uint32_t >( dynamicStateEnables.size() );

		// VERTEX BINDINGS: PBR
		VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof( vkglTF::Model::Vertex ), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector< VkVertexInputAttributeDescription > vertexInputAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof( float ) * 3 },
			{ 2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof( float ) * 6 },
			{ 3, 0, VK_FORMAT_R32G32_SFLOAT, sizeof( float ) * 8 },
			{ 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 10 },
			{ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 14 },
			{ 6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 18 } };
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI {};
		vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast< uint32_t >( vertexInputAttributes.size() );
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		depthStencilStateCI.depthWriteEnable = VK_TRUE;
		depthStencilStateCI.depthTestEnable = VK_TRUE;
		depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		// Graphics Pipelines
		std::array< VkPipelineShaderStageCreateInfo, 2 > shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCI.renderPass = m_pRender->GetRenderPasses()[ 0 ];
		pipelineCI.subpass = 0;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pTessellationState = nullptr;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.stageCount = static_cast< uint32_t >( shaderStages.size() );
		pipelineCI.pStages = shaderStages.data();

		// ALL OTHER PIPELINES
		pipelineCI.layout = m_pRender->vkPipelineLayout;
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

		// PIPELINE: PBR
		rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
		depthStencilStateCI.depthWriteEnable = VK_TRUE;
		depthStencilStateCI.depthTestEnable = VK_TRUE;

		// PIPELINE: ALPHA BLEND
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		// Create the graphics pipeline
		workshopPipelines.unGridGraphicsPipeline = m_pRender->AddCustomPipeline( "shaders/pbr.vert.spv", "shaders/floor_grid.frag.spv", &pipelineCI );

		// Assign this pipeline to assets
		m_pRender->vecRenderScenes[ workshopScenes.unFloorSpot ]->vkPipeline = m_pRender->GetCustomPipelines()[ workshopPipelines.unGridGraphicsPipeline ];
	}

	void Workshop::PrepareStencilPipelines()
	{
		assert( m_pRender->GetRenderPasses()[ 0 ] != VK_NULL_HANDLE );

		// Vertex bindings
		VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof( vkglTF::Model::Vertex ), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector< VkVertexInputAttributeDescription > vertexInputAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof( float ) * 3 },
			{ 2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof( float ) * 6 },
			{ 3, 0, VK_FORMAT_R32G32_SFLOAT, sizeof( float ) * 8 },
			{ 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 10 },
			{ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 14 },
			{ 6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof( float ) * 18 } };
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI {};
		vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast< uint32_t >( vertexInputAttributes.size() );
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI {};
		inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI {};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateCI.depthClampEnable = VK_FALSE;
		rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCI.depthBiasEnable = VK_FALSE;
		rasterizationStateCI.depthBiasConstantFactor = 0;
		rasterizationStateCI.depthBiasClamp = 0;
		rasterizationStateCI.depthBiasSlopeFactor = 0;
		rasterizationStateCI.lineWidth = 1.0f;

		// Color blending
		VkPipelineColorBlendAttachmentState blendAttachmentState {};
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendStateCI {};
		colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCI.attachmentCount = 1;
		colorBlendStateCI.pAttachments = &blendAttachmentState;
		colorBlendStateCI.logicOpEnable = VK_FALSE;
		colorBlendStateCI.logicOp = VK_LOGIC_OP_NO_OP;
		colorBlendStateCI.blendConstants[ 0 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 1 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 2 ] = 1.0f;
		colorBlendStateCI.blendConstants[ 3 ] = 1.0f;

		// Viewport
		VkRect2D scissor = { { 0, 0 }, m_pRender->vkExtent };
		VkViewport viewport = { 0.0f, 0.0f, ( float )m_pRender->vkExtent.width, ( float )m_pRender->vkExtent.height, 0.0f, 1.0f };
		VkPipelineViewportStateCreateInfo viewportStateCI {};
		viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCI.viewportCount = 1;
		viewportStateCI.pViewports = &viewport;
		viewportStateCI.scissorCount = 1;
		viewportStateCI.pScissors = &scissor;

		// TODO: MSAA support
		VkPipelineMultisampleStateCreateInfo multisampleStateCI {};
		multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// TODO: Expose Dynamics states to apps
		std::vector< VkDynamicState > dynamicStateEnables; // = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI {};
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
		dynamicStateCI.dynamicStateCount = static_cast< uint32_t >( dynamicStateEnables.size() );

		// Depth and stencil
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		depthStencilStateCI.depthWriteEnable = VK_TRUE;
		depthStencilStateCI.depthTestEnable = VK_TRUE;
		depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		// Graphics Pipeline setup
		VkGraphicsPipelineCreateInfo pipelineCI { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCI.renderPass = m_pRender->GetRenderPasses()[ 0 ];
		pipelineCI.subpass = 0;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pTessellationState = nullptr;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;

		// We'll create two pipelines with just variations in stencil handling

		// stencil pass (mask - write to stencil buffer)
		pipelineCI.layout = m_pRender->vkPipelineLayout;
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;

		// rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilStateCI.depthTestEnable = VK_FALSE;
		depthStencilStateCI.stencilTestEnable = VK_TRUE;
		depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilStateCI.back.failOp = VK_STENCIL_OP_REPLACE;
		depthStencilStateCI.back.depthFailOp = VK_STENCIL_OP_REPLACE;
		depthStencilStateCI.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencilStateCI.back.compareMask = 0xff;
		depthStencilStateCI.back.writeMask = 0xff;
		depthStencilStateCI.back.reference = 1;
		depthStencilStateCI.front = depthStencilStateCI.back;

		workshopPipelines.unStencilMask = m_pRender->AddCustomPipeline( "shaders/pbr.vert.spv", "shaders/pbr_khr.frag.spv", &pipelineCI );

		// stencil fill pass
		depthStencilStateCI.back.compareOp = VK_COMPARE_OP_EQUAL;
		depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCI.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCI.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencilStateCI.front = depthStencilStateCI.back;
		depthStencilStateCI.depthTestEnable = VK_TRUE;
		workshopPipelines.unStencilFill = m_pRender->AddCustomPipeline( "shaders/pbr.vert.spv", "shaders/pbr_khr.frag.spv", &pipelineCI );

		// stencil fill out
		depthStencilStateCI.back.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCI.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCI.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencilStateCI.front = depthStencilStateCI.back;
		depthStencilStateCI.depthTestEnable = VK_TRUE;
		workshopPipelines.unStencilOut = m_pRender->AddCustomPipeline( "shaders/pbr.vert.spv", "shaders/floor_grid_grey.frag.spv", &pipelineCI );

		// Assign these pipelines to assets
		m_pRender->vecRenderScenes[ workshopScenes.unSpotMask ]->vkPipeline = m_pRender->GetCustomPipelines()[ workshopPipelines.unStencilMask ];

		m_pRender->vecRenderScenes[ workshopScenes.unGridFloorHighlight ]->vkPipeline = m_pRender->GetCustomPipelines()[ workshopPipelines.unStencilFill ];
		m_pRender->vecRenderScenes[ workshopScenes.unGridFloor ]->vkPipeline = m_pRender->GetCustomPipelines()[ workshopPipelines.unStencilOut ];
	}

	void Workshop::AddSceneAssets()
	{
		// Add default floor 
		workshopScenes.unFloorSpot = m_pRender->AddRenderScene( "models/floor_spot.glb", { 3.f, 1.f, 3.f } );


		// locomotion grid spot
		workshopScenes.unSpotMask = m_pRender->AddRenderScene( "models/floor_spot_loco.glb", { 10.f, 10.f, 10.f } );
		m_pRender->vecRenderScenes[ workshopScenes.unSpotMask ]->bMovesWithPlayer = true;

		// passthrough - locomotion grid floor (inside and outside stencil areas)
		workshopScenes.unGridFloorHighlight = m_pRender->AddRenderScene( "models/floor_grid_highlight.glb", { .1f, 1.f, .1f } );
		workshopScenes.unGridFloor = m_pRender->AddRenderScene( "models/floor_grid.glb", { .1f, 1.f, .1f } );

		m_pRender->AddRenderScene( "models/ancient_ruins.glb", { 2.f, 2.f, 2.f } );
	}

	XrResult Workshop::CreateActionsets()
	{
		workshopActionsets.locomotion = new oxr::ActionSet();
		return m_pInput->CreateActionSet( workshopActionsets.locomotion, "locomotion", "Locomotion controls" );
	}

	XrResult Workshop::CreateActions()
	{
		XrResult xrResult = XR_SUCCESS;

		workshopActions.vec2SmoothLoco = new Action( XR_ACTION_TYPE_VECTOR2F_INPUT, &Callback_SmoothLoco );
		xrResult = m_pInput->CreateAction( workshopActions.vec2SmoothLoco, workshopActionsets.locomotion, "loco_smooth", "Locomotion - Smooth", { "/user/hand/left", "/user/hand/right" } );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		workshopActions.bEnableSmoothLoco = new Action( XR_ACTION_TYPE_BOOLEAN_INPUT, &Callback_EnableSmoothLoco );
		xrResult = m_pInput->CreateAction(
			workshopActions.bEnableSmoothLoco, workshopActionsets.locomotion, "enable_loco_passthrough", "Enable Locomotion - Passthrough", { "/user/hand/left", "/user/hand/right" } );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		return XR_SUCCESS;
	}

	XrResult Workshop::SuggestBindings()
	{
		// smooth loco
		m_pInput->AddBinding( &baseController, workshopActions.vec2SmoothLoco->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::AxisControl, Controller::Qualifier::None );
		m_pInput->AddBinding( &baseController, workshopActions.vec2SmoothLoco->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::AxisControl, Controller::Qualifier::None );

		m_pInput->AddBinding( &baseController, workshopActions.bEnableSmoothLoco->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::AxisControl, Controller::Qualifier::Touch );
		m_pInput->AddBinding( &baseController, workshopActions.bEnableSmoothLoco->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::AxisControl, Controller::Qualifier::Touch );

		return m_pInput->SuggestBindings( &baseController, nullptr );
	}

	XrResult Workshop::AttachActionsets()
	{
		std::vector< XrActionSet > vecActionSets = { m_pActionsetMain->xrActionSetHandle, workshopActionsets.locomotion->xrActionSetHandle };

		XrResult xrResult = m_pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		return XR_SUCCESS;
	}

	XrResult Workshop::AddActionsetsForSync()
	{
		XrResult xrResult = m_pInput->AddActionsetForSync( m_pActionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		xrResult = m_pInput->AddActionsetForSync( workshopActionsets.locomotion );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		return XR_SUCCESS;
	}

	void Workshop::PrepareRender( uint32_t unSwapchainIndex, uint32_t unImageIndex )
	{
		// Check if we're supposed to render on this frame
		if ( !m_xrFrameState.shouldRender )
			return;

		// Check if we need to update draw debug shapes
		if ( m_extHandTracking && m_bDrawDebug )
			UpdateHandTrackingPoses();

		// Render
		m_pRender->BeginRender( m_pSession, m_vecFrameLayerProjectionViews, &m_xrFrameState, unSwapchainIndex, unImageIndex, m_fNearZ, m_fFarZ );
	}

	void Workshop::SubmitRender( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { m_pRender->EndRender(); }

#ifdef XR_USE_PLATFORM_ANDROID
	void Workshop::AndroidPoll()
	{
		// For android we need in addition, to poll android events to proceed to the session ready state
		while ( true )
		{
			// We'll wait indefinitely (-1ms) for android poll results until we get to the android app resumed state
			// and/or the openxr session has started.
			const int nTimeout = ( !m_pProvider->Instance()->androidAppState.Resumed && m_pProvider->Session()->IsSessionRunning() ) ? -1 : 0;

			int nEvents;
			struct android_poll_source *androidPollSource;
			if ( ALooper_pollAll( nTimeout, nullptr, &nEvents, ( void ** )&androidPollSource ) < 0 )
			{
				break;
			}

			// Process android event
			if ( androidPollSource )
			{
				androidPollSource->process( m_pProvider->Instance()->androidApp, androidPollSource );
			}
		}
	}
#endif

} // namespace oxa
