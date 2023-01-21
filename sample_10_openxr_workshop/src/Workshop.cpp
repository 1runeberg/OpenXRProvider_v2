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
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothFwd );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
	else if (currentState->y < -g_pWorkshop->locomotionParams.fSmoothActivationPoint)
	{
		XrVector3f dir = g_pWorkshop->GetBackVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothBack );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}

	// Left / Right
	if ( currentState->x > g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetRightVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothRight );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
	else if ( currentState->x < -g_pWorkshop->locomotionParams.fSmoothActivationPoint )
	{
		XrVector3f dir = g_pWorkshop->GetLeftVector( &g_pWorkshop->GetRender()->currentHmdState.orientation );
		g_pWorkshop->ScaleVector( &dir, g_pWorkshop->locomotionParams.fSmoothRight );
		g_pWorkshop->AddVectors( &g_pWorkshop->GetRender()->playerWorldState.position, &dir );
	}
}

// Render callbacks
inline void Callback_PreRender( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pWorkshop->PrepareRender( unSwapchainIndex, unImageIndex ); }
inline void Callback_PostRender( uint32_t unSwapchainIndex, uint32_t unImageIndex ) { g_pWorkshop->SubmitRender( unSwapchainIndex, unImageIndex ); }

namespace oxa
{
	Workshop::Workshop() {}

	Workshop::~Workshop() {}

#ifdef XR_USE_PLATFORM_ANDROID
	XrResult Workshop::Start( struct android_app *app )
#else
	XrResult Workshop::Start()
#endif
	{
		g_pWorkshop = this; // needed for the global callbacks

		// (1) Create openxr instance and session - you can add requested texture formats in vecRequestedTextureFormats and vecRequestedDepthFormats
#ifdef XR_USE_PLATFORM_ANDROID
		XrResult xrResult = Workshop::Init( app, APP_NAME, OXR_MAKE_VERSION32( 0, 1, 0 ) );
#else
		XrResult xrResult = Workshop::Init( APP_NAME, OXR_MAKE_VERSION32( 0, 1, 0 ) );
#endif

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (4) Add debug assets to render (comment out when ready with your own game assets)
		AddDebugMeshes();

		// (5) ... Add assets to render here (vecRenderScene/Sector/Model) ...
		AddSceneAssets();

		// (6) Start loading gltf assets from disk
		m_pRender->LoadAssets();

		// (7) Prepare pbr pipelines
		m_pRender->PrepareAllPipelines();

		// () ... Add custom shapes/basic geometry pipelines here (if any) ...
		PrepareCustomShapesPipelines();

		// () ... Add custom graphics pipelines here (if any) ...
		PrepareCustomGraphicsPipelines();

		// (8) Register render call backs - input callbacks are defined when creating an action
		//     update Callback_PreRender and Callback_PostRender with your render code
		RegisterRenderCallbacks();

		// (9) ... Create actionset/s here (template creates a main actionset by default) ...
		xrResult = CreateActionsets();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (10) ... Create action/s here (template creates pose actions by default for the controllers) ...
		xrResult = CreateActions();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (11) ... Suggest binding/s here...
		xrResult = SuggestBindings();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (12) ... Attach actionset/s to session ...
		xrResult = AttachActionsets();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (13) ... Add action set for the next input sync ...
		xrResult = AddActionsetsForSync();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// (14) ... Set skybox ...
		m_pRender->SetSkyboxVisibility( true );

		// (15) Run main loop
		RunGameLoop();

		return XR_SUCCESS;
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

	void Workshop::PrepareCustomGraphicsPipelines() { PrepareFloorGridPipeline(); }

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

		// PIPELINE LAYOUT: PBR (push constants)
		// const std::vector< VkDescriptorSetLayout > setLayouts = { m_pRender->descriptorSetLayouts.scene, m_pRender->descriptorSetLayouts.material, m_pRender->descriptorSetLayouts.node };
		// VkPipelineLayoutCreateInfo pipelineLayoutCI {};
		// pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		// pipelineLayoutCI.setLayoutCount = static_cast< uint32_t >( setLayouts.size() );
		// pipelineLayoutCI.pSetLayouts = setLayouts.data();
		// VkPushConstantRange pushConstantRange {};
		// pushConstantRange.size = sizeof( xrvk::Render::PushConstBlockMaterial );
		// pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		// pipelineLayoutCI.pushConstantRangeCount = 1;
		// pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		// VK_CHECK_RESULT( vkCreatePipelineLayout( m_SharedState.vkDevice, &pipelineLayoutCI, nullptr, &vkPipelineLayout ) );

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
		uint32_t unGridGraphicsPipelineIdx = m_pRender->AddCustomPipeline( "shaders/pbr.vert.spv", "shaders/floor_grid.frag.spv", &pipelineCI );

		// Assign this pipeline to assets
		m_pRender->vecRenderScenes[ m_unFloorSpotIdx ]->vkPipeline = m_pRender->GetCustomPipelines()[ unGridGraphicsPipelineIdx ];
	}

	void Workshop::AddSceneAssets()
	{
		// Add default floor
		m_unFloorSpotIdx = m_pRender->AddRenderScene( "models/floor_spot.glb", { 3.f, 1.f, 3.f } );
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
		return m_pInput->CreateAction( workshopActions.vec2SmoothLoco, workshopActionsets.locomotion, "loco_smooth", "Locomotion - Smooth", { "/user/hand/left", "/user/hand/right" } );
	}

	XrResult Workshop::SuggestBindings()
	{
		m_pInput->AddBinding( &baseController, workshopActions.vec2SmoothLoco->xrActionHandle, XR_HAND_LEFT_EXT, Controller::Component::AxisControl, Controller::Qualifier::None );
		m_pInput->AddBinding( &baseController, workshopActions.vec2SmoothLoco->xrActionHandle, XR_HAND_RIGHT_EXT, Controller::Component::AxisControl, Controller::Qualifier::None );

		XrResult xrResult = m_pInput->SuggestBindings( &baseController, nullptr );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		return XR_SUCCESS;
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
