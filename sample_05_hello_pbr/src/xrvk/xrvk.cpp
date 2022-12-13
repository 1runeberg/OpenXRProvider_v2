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
 * Portions of this code are Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
 * SPDX-License-Identifier: MIT
 *
 */

#include "xrvk.hpp"
#include "vulkanpbr/VulkanUtils.hpp"

namespace xrvk
{
#ifdef XR_USE_PLATFORM_ANDROID
	Render::Render( AAssetManager *pAndroidAssetManager, ELogLevel eLogLevel, bool showSkybox, std::string filenameSkyboxTex, std::string filenameSkyboxModel, VkClearColorValue vkClearColorValue )
		: m_eMinLogLevel( eLogLevel )
#else
	Render::Render( ELogLevel eLogLevel, bool showSkybox, std::string filenameSkyboxTex, std::string filenameSkyboxModel, VkClearColorValue vkClearColorValue )
		: m_eMinLogLevel( eLogLevel )
#endif
	{

		// For android, we need the asset manager for loading files (e.g. models, shaders)
#ifdef XR_USE_PLATFORM_ANDROID
		assert( pAndroidAssetManager );
		m_SharedState.androidAssetManager = pAndroidAssetManager;
#endif
		// set clear color values
		m_SharedState.vkClearValues[ 0 ].color = vkClearColorValue;
		m_SharedState.vkClearValues[ 1 ].depthStencil = { 1.0f, 0 };

		// create skybox and reset params
		skybox = new RenderModel( filenameSkyboxModel );
		skyboxTexture = filenameSkyboxTex;
		skybox->bApplyOffset = false;
		SetSkyboxVisibility( showSkybox );
		XrVector3f_Set( &skybox->currentScale, 1.0f );
		XrPosef_Identity( &skybox->currentPose );

		// Add greeting
		LogInfo( "G'Day! xrvk version %i.%i.%i", 0, 1, 0 );
	};

	Render::~Render()
	{
		// free internal objects
		if ( skybox != nullptr )
			delete skybox;

		for ( auto &renderable : vecRenderScenes )
			delete renderable;

		for ( auto &renderable : vecRenderSectors )
			delete renderable;

		for ( auto &renderable : vecRenderModels )
			delete renderable;

		// free vulkan resources
		if ( m_pVulkanDevice )
			delete m_pVulkanDevice;

		if ( vkDescriptorPool != VK_NULL_HANDLE )
			vkDestroyDescriptorPool( m_SharedState.vkDevice, vkDescriptorPool, nullptr );

		if ( vkPipelineLayout != VK_NULL_HANDLE )
			vkDestroyPipelineLayout( m_SharedState.vkDevice, vkPipelineLayout, nullptr );
	}

	XrResult Render::Init( oxr::Provider *pProvider, const char *pccAppName, uint32_t unAppVersion, const char *pccEngineName, uint32_t unEngineVersion )
	{
		assert( pccAppName );
		assert( pccEngineName );

		XrResult xrResult = XR_SUCCESS;
		m_pProvider = pProvider;

		// For android we need to set the assetmanager for tinygltf
#ifdef XR_USE_PLATFORM_ANDROID
		tinygltf::asset_manager = m_SharedState.androidAssetManager;
#endif

		// (1) Call *required* function GetVulkanGraphicsRequirements
		//       This returns the min-and-max vulkan api level required by the active runtime
		XrGraphicsRequirementsVulkan2KHR xrGraphicsRequirements;
		pProvider->GetVulkanGraphicsRequirements( &xrGraphicsRequirements );

		// (2) Define app info for vulkan
		VkApplicationInfo vkApplicationInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		vkApplicationInfo.pApplicationName = pccAppName;
		vkApplicationInfo.applicationVersion = unAppVersion;
		vkApplicationInfo.pEngineName = pccEngineName;
		vkApplicationInfo.engineVersion = unEngineVersion;
		vkApplicationInfo.apiVersion = VK_API_VERSION_1_0; // we'll use vulkan 1.0 for maximum compatibility. Alternatively, you can check for min/max
														   // runtime supported vulkan api returned by GetVulkanGraphicsRequirements

		// (3) Define vulkan layers and extensions the app needs (the runtime required extensions will be added in by the provider.
		//       We won't need any for this demo
		std::vector< const char * > vecLayers;
		for ( auto validationLayer : m_vecValidationLayers )
		{
			vecLayers.push_back( validationLayer );
		}

		std::vector< const char * > vecExtensions;
		for ( auto validationExtension : m_vecValidationExtensions )
		{
			vecExtensions.push_back( validationExtension );
		}

		// (4) Create vulkan instance
		VkInstanceCreateInfo vkInstanceCreateInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
		vkInstanceCreateInfo.enabledLayerCount = ( uint32_t )vecLayers.size();
		vkInstanceCreateInfo.ppEnabledLayerNames = vecLayers.empty() ? nullptr : vecLayers.data();
		vkInstanceCreateInfo.enabledExtensionCount = ( uint32_t )vecExtensions.size();
		vkInstanceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

		XrVulkanInstanceCreateInfoKHR xrVulkanInstanceCreateInfo { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
		xrVulkanInstanceCreateInfo.systemId = pProvider->Instance()->xrSystemId;
		xrVulkanInstanceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
		xrVulkanInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;
		xrVulkanInstanceCreateInfo.vulkanAllocator = nullptr;

		// ... validation
		VkDebugUtilsMessengerCreateInfoEXT vkDebugCreateInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		vkDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		vkDebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		vkDebugCreateInfo.pfnUserCallback = Debug_Callback;

		vkInstanceCreateInfo.pNext = ( VkDebugUtilsMessengerCreateInfoEXT * )&vkDebugCreateInfo;

		VkResult vkResult = VK_SUCCESS;
		xrResult = pProvider->CreateVulkanInstance( &xrVulkanInstanceCreateInfo, &m_SharedState.vkInstance, &vkResult );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) && vkResult == VK_SUCCESS )
		{
			LogInfo( "Vulkan instance successfully created." );
		}
		else
		{
			LogError( "Error creating vulkan instance with openxr result (%s) and vulkan result (%i)", oxr::XrEnumToString( xrResult ), ( int32_t )vkResult );
			return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
		}

		// (5) Get the vulkan physical device (gpu) used by the runtime
		xrResult = pProvider->GetVulkanGraphicsPhysicalDevice( &m_SharedState.vkPhysicalDevice, m_SharedState.vkInstance );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", oxr::XrEnumToString( xrResult ) );
			return xrResult;
		}

		// (6) Create vks device handler
		m_pVulkanDevice = new vks::VulkanDevice( m_SharedState.vkPhysicalDevice );

#ifdef XR_USE_PLATFORM_ANDROID
		m_pVulkanDevice->androidAssetManager = pProvider->Instance()->androidActivity->assetManager;
#endif

		// (7) Create vulkan logical device
		VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		float fQueuePriorities = 0;
		vkDeviceQueueCreateInfo.queueCount = 1;
		vkDeviceQueueCreateInfo.pQueuePriorities = &fQueuePriorities;

		uint32_t unQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( m_SharedState.vkPhysicalDevice, &unQueueFamilyCount, nullptr );
		std::vector< VkQueueFamilyProperties > vecQueueFamilyProps( unQueueFamilyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( m_SharedState.vkPhysicalDevice, &unQueueFamilyCount, vecQueueFamilyProps.data() );

		for ( uint32_t i = 0; i < unQueueFamilyCount; ++i )
		{
			if ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0u )
			{
				m_SharedState.vkQueueFamilyIndex = vkDeviceQueueCreateInfo.queueFamilyIndex = i;
				break;
			}
		}

		std::vector< const char * > vkDeviceExtensions;

#if defined( _WIN32 )
		vkDeviceExtensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
#endif

		VkDeviceCreateInfo vkDeviceCreateInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		vkDeviceCreateInfo.queueCreateInfoCount = 1;
		vkDeviceCreateInfo.pQueueCreateInfos = &vkDeviceQueueCreateInfo;
		vkDeviceCreateInfo.enabledLayerCount = 0;
		vkDeviceCreateInfo.ppEnabledLayerNames = nullptr;
		vkDeviceCreateInfo.enabledExtensionCount = ( uint32_t )vkDeviceExtensions.size();
		vkDeviceCreateInfo.ppEnabledExtensionNames = vkDeviceExtensions.empty() ? nullptr : vkDeviceExtensions.data();
		vkDeviceCreateInfo.pEnabledFeatures = &m_SharedState.vkPhysicalDeviceFeatures;

		XrVulkanDeviceCreateInfoKHR xrVulkanDeviceCreateInfo { XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
		xrVulkanDeviceCreateInfo.systemId = pProvider->Instance()->xrSystemId;
		xrVulkanDeviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
		xrVulkanDeviceCreateInfo.vulkanCreateInfo = &vkDeviceCreateInfo;
		xrVulkanDeviceCreateInfo.vulkanPhysicalDevice = m_SharedState.vkPhysicalDevice;
		xrVulkanDeviceCreateInfo.vulkanAllocator = nullptr;

		xrResult = pProvider->CreateVulkanDevice( &xrVulkanDeviceCreateInfo, &m_SharedState.vkPhysicalDevice, &m_SharedState.vkInstance, &m_SharedState.vkDevice, &vkResult );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) && vkResult == VK_SUCCESS )
		{
			LogInfo( "Vulkan device successfully created." );
		}
		else
		{
			LogError( "Error creating vulkan device with openxr result (%s) and vulkan result (%i)", oxr::XrEnumToString( xrResult ), ( int32_t )vkResult );
			return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
		}

		// (8) Get device queue
		vkGetDeviceQueue( m_SharedState.vkDevice, vkDeviceQueueCreateInfo.queueFamilyIndex, 0, &m_SharedState.vkQueue );

		// (9) Set vks properties
		m_pVulkanDevice->logicalDevice = m_SharedState.vkDevice;

		// (10) Create graphics binding that we will use to create an openxr session
		m_SharedState.xrGraphicsBinding.instance = m_SharedState.vkInstance;
		m_SharedState.xrGraphicsBinding.physicalDevice = m_SharedState.vkPhysicalDevice;
		m_SharedState.xrGraphicsBinding.device = m_SharedState.vkDevice;
		m_SharedState.xrGraphicsBinding.queueFamilyIndex = vkDeviceQueueCreateInfo.queueFamilyIndex;
		m_SharedState.xrGraphicsBinding.queueIndex = m_SharedState.vkQueueIndex;

		return XR_SUCCESS;
	}

	void Render::CreateRenderResources( oxr::Session *pSession, int64_t nColorFormat, int64_t nDepthFormat, VkExtent2D vkExtent )
	{
		assert( pSession );

		// Set extent
		this->vkExtent = vkExtent;

		// (1) Set shader parameters
		vecUniformBuffers.resize( k_unCommandBufferNum );
		vecDescriptorSets.resize( k_unCommandBufferNum );

		// (2) Create render pass(es)
		CreateRenderPass( nColorFormat, nDepthFormat );

		// (3) Create render target(s) per image in the swapchain including frame buffers
		CreateRenderTargets( pSession, m_vecRenderPasses[ 0 ] );

		// (4) Create vulkan command buffers
		m_vecFrameData.resize( k_unCommandBufferNum, {} );

		for ( uint32_t i = 0; i < k_unCommandBufferNum; i++ )
		{
			// Create a command pool to allocate our command buffer from
			VkCommandPoolCreateInfo cmdPoolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			cmdPoolInfo.queueFamilyIndex = m_SharedState.vkQueueFamilyIndex;
			vkCreateCommandPool( m_SharedState.vkDevice, &cmdPoolInfo, nullptr, &m_vecFrameData[ 0 ].vkCommandPool );

			// Create the command buffer from the command pool
			VkCommandBufferAllocateInfo cmd { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmd.commandPool = m_vecFrameData[ 0 ].vkCommandPool;
			cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmd.commandBufferCount = 1;
			vkAllocateCommandBuffers( m_SharedState.vkDevice, &cmd, &m_vecFrameData[ 0 ].vkCommandBuffer );

			VkFenceCreateInfo fenceInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			vkCreateFence( m_SharedState.vkDevice, &fenceInfo, nullptr, &m_vecFrameData[ 0 ].vkCommandFence );
		}

		// (5) Create command pool
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = m_SharedState.vkQueueFamilyIndex;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT( vkCreateCommandPool( m_SharedState.vkDevice, &cmdPoolInfo, nullptr, &m_pVulkanDevice->commandPool ) );

		// (6) Create pipeline cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT( vkCreatePipelineCache( m_SharedState.vkDevice, &pipelineCacheCreateInfo, nullptr, &m_SharedState.vkPipelineCache ) );
	}

	void Render::BeginRender(
		oxr::Session *pSession,
		std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews,
		XrFrameState *pFrameState,
		uint32_t unSwapchainIndex,
		uint32_t unImageIndex,
		float fNearZ /*= 0.1f*/,
		float fFarZ /*= 100.0f*/,
		XrVector3f v3fScaleEyeView /*= { 1.0f, 1.0f, 1.0f } */ )
	{
		// (1) Set command buffer to recording
		VkCommandBufferBeginInfo cmdBeginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, &cmdBeginInfo );

		// (2) Set render pass info
		VkRenderPassBeginInfo renderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBeginInfo.clearValueCount = ( uint32_t )m_SharedState.vkClearValues.size();
		renderPassBeginInfo.pClearValues = m_SharedState.vkClearValues.data();

		// (3) Get swapchain
		auto *pSwapchain = &pSession->GetSwapchains()[ unSwapchainIndex ];

		// (4) Set extent
		VkExtent2D vkExtent;
		vkExtent.width = pSwapchain->unWidth;
		vkExtent.height = pSwapchain->unHeight;

		// (5) Bind render target
		renderPassBeginInfo.renderPass = m_vecRenderPasses[ 0 ];
		renderPassBeginInfo.framebuffer = m_vec2RenderTargets[ unSwapchainIndex ][ unImageIndex ].vkFrameBuffer;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = vkExtent;

		// (6) Start render pass
		vkCmdBeginRenderPass( m_vecFrameData[ 0 ].vkCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

		// (7) Create the projection matrix
		XrMatrix4x4f matProjection;
		XrMatrix4x4f_CreateProjectionFov( &matProjection, GRAPHICS_VULKAN, vecFrameLayerProjectionViews[ unSwapchainIndex ].fov, fNearZ, fFarZ );

		// (8) Get eye pose
		XrPosef *eyePose = &vecFrameLayerProjectionViews[ unSwapchainIndex ].pose;

		// (9) Create the view matrix (eye transform)
		XrMatrix4x4f matView;
		XrMatrix4x4f_CreateTranslationRotationScale( &matView, &eyePose->position, &eyePose->orientation, &v3fScaleEyeView );

		// (10) Invert
		XrMatrix4x4f matInvertedRigidBodyView;
		XrMatrix4x4f_InvertRigidBody( &matInvertedRigidBodyView, &matView );

		// (11) View Projection
		XrMatrix4x4f matViewProjection;
		XrMatrix4x4f_Multiply( &matViewProjection, &matProjection, &matInvertedRigidBodyView );

		// (12) Draw vismask if available
		if ( m_vecVisMasks.size() > unSwapchainIndex && !m_vecVisMasks[ unSwapchainIndex ].indices.empty() )
		{
			assert( m_vecVisMasks.size() == m_vecVisMaskBuffers.size() );

			// Reverse winding order
			for ( auto &vismask : m_vecVisMasks )
			{
				for ( uint32_t i = 0; i < vismask.indices.size(); i += 3 )
				{
					std::swap( vismask.indices[ i + 1 ], vismask.indices[ i + 2 ] );
				}
			}

			XrMatrix4x4f matVisMask;
			XrMatrix4x4f_CreateIdentity( &matVisMask );

			XrVector3f scaleVisMask;
			XrVector3f_Set( &scaleVisMask, 1.f );
			XrMatrix4x4f_CreateTranslationRotationScale( &matVisMask, &eyePose->position, &eyePose->orientation, &scaleVisMask );

			// calculate mvp
			XrMatrix4x4f mvp;
			XrMatrix4x4f_CreateIdentity( &mvp );
			XrMatrix4x4f_Multiply( &mvp, &matViewProjection, &matVisMask );

			// bind graphics pipeline
			vkCmdBindPipeline( m_vecFrameData[ 0 ].vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.vismask );

			// bind buffers
			const VkDeviceSize offsets[ 1 ] = { 0 };
			vkCmdBindVertexBuffers( m_vecFrameData[ 0 ].vkCommandBuffer, 0, 1, &m_vecVisMaskBuffers[ unSwapchainIndex ].vertexBuffer.buffer, offsets );
			vkCmdBindIndexBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, m_vecVisMaskBuffers[ unSwapchainIndex ].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

			// update push constants
			vkCmdPushConstants( m_vecFrameData[ 0 ].vkCommandBuffer, vkPipelineLayoutVisMask, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( mvp.m ), &mvp.m[ 0 ] );

			// finally, draw
			vkCmdDrawIndexed( m_vecFrameData[ 0 ].vkCommandBuffer, static_cast< uint32_t >( m_vecVisMasks[ unSwapchainIndex ].indices.size() ), 1, 0, 0, 0 );
		}

		// (13) Draw skybox
		UniformBufferSet currentUB = vecUniformBuffers[ 0 ];

		if ( GetSkyboxVisibility() )
		{
			UpdateUniformBuffers( &uboMatricesSkybox, &skyboxUniformBuffer, skybox, &matViewProjection, eyePose );

			// todo: double buffering, vecDescriptorSets[ x ].skybox : x for command buffer index
			vkCmdBindDescriptorSets( m_vecFrameData[ 0 ].vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout, 0, 1, &vecDescriptorSets[ 0 ].skybox, 0, nullptr );
			vkCmdBindPipeline( m_vecFrameData[ 0 ].vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox );
			skybox->gltfModel.draw( m_vecFrameData[ 0 ].vkCommandBuffer );
		}

		// (14) Draw all renderables (recording command buffer)

		// (14.1) Update renderables current poses
		UpdateRenderablePoses( pSession, pFrameState );

		// (14.2) Update renderables shader values UBOs
		UpdateUniformBuffers( &uboMatricesScene, &currentUB.scene, &matViewProjection, eyePose );

		// (14.3) Copy pbr properties to gpu
		memcpy( currentUB.params.mapped, &shaderValuesPbrParams, sizeof( shaderValuesPbrParams ) );

		// (14.4) Draw renderables
		RenderGltfScenes();

		// (15) End render pass
		vkCmdEndRenderPass( m_vecFrameData[ 0 ].vkCommandBuffer );

		// (16) Close command buffer recording
		vkEndCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer );
	}

	void Render::EndRender()
	{
		// Execute command buffer (requires exclusive access to vkQueue)
		// safest after wait swapchain image
		VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_vecFrameData[ 0 ].vkCommandBuffer;
		vkQueueSubmit( m_SharedState.vkQueue, 1, &submitInfo, m_vecFrameData[ 0 ].vkCommandFence );

		const uint32_t timeoutNs = 1 * 1000000000;
		vkWaitForFences( m_SharedState.vkDevice, 1, &m_vecFrameData[ 0 ].vkCommandFence, VK_TRUE, timeoutNs );
		vkResetFences( m_SharedState.vkDevice, 1, &m_vecFrameData[ 0 ].vkCommandFence );
		vkResetCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, 0 );
	}

	void Render::RenderNode( RenderSceneBase *renderable, vkglTF::Node *gltfNode, uint32_t unCmdBufIndex, vkglTF::Material::AlphaMode gltfAlphaMode )
	{
		if ( gltfNode->mesh )
		{
			// update gltf node pose and scale with renderable's
			gltfNode->scale = renderable->GetScale();
			gltfNode->translation = renderable->GetPosition();
			gltfNode->rotation = renderable->GetRotation();
			gltfNode->update();

			// Render mesh primitives
			for ( vkglTF::Primitive *primitive : gltfNode->mesh->primitives )
			{
				if ( primitive->material.alphaMode == gltfAlphaMode )
				{

					VkPipeline pipeline = VK_NULL_HANDLE;
					switch ( gltfAlphaMode )
					{
						case vkglTF::Material::ALPHAMODE_OPAQUE:
						case vkglTF::Material::ALPHAMODE_MASK:
							pipeline = primitive->material.doubleSided ? pipelines.pbrDoubleSided : pipelines.pbr;
							break;
						case vkglTF::Material::ALPHAMODE_BLEND:
							pipeline = pipelines.pbrAlphaBlend;
							break;
					}

					if ( pipeline != vkBoundPipeline )
					{
						vkCmdBindPipeline( m_vecFrameData[ unCmdBufIndex ].vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
						vkBoundPipeline = pipeline;
					}

					const std::vector< VkDescriptorSet > descriptorsets = {
						vecDescriptorSets[ unCmdBufIndex ].scene,
						primitive->material.descriptorSet,
						gltfNode->mesh->uniformBuffer.descriptorSet,
					};
					vkCmdBindDescriptorSets(
						m_vecFrameData[ unCmdBufIndex ].vkCommandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						vkPipelineLayout,
						0,
						static_cast< uint32_t >( descriptorsets.size() ),
						descriptorsets.data(),
						0,
						NULL );

					// Pass material parameters as push constants
					PushConstBlockMaterial pushConstBlockMaterial {};
					pushConstBlockMaterial.emissiveFactor = primitive->material.emissiveFactor;

					// To save push constant space, availability and texture coordinate set are combined
					// -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
					pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
					pushConstBlockMaterial.normalTextureSet = primitive->material.normalTexture != nullptr ? primitive->material.texCoordSets.normal : -1;
					pushConstBlockMaterial.occlusionTextureSet = primitive->material.occlusionTexture != nullptr ? primitive->material.texCoordSets.occlusion : -1;
					pushConstBlockMaterial.emissiveTextureSet = primitive->material.emissiveTexture != nullptr ? primitive->material.texCoordSets.emissive : -1;
					pushConstBlockMaterial.alphaMask = static_cast< float >( primitive->material.alphaMode == vkglTF::Material::ALPHAMODE_MASK );
					pushConstBlockMaterial.alphaMaskCutoff = primitive->material.alphaCutoff;

					// TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

					if ( primitive->material.pbrWorkflows.metallicRoughness )
					{
						// Metallic roughness workflow
						pushConstBlockMaterial.workflow = static_cast< float >( PBR_WORKFLOW_METALLIC_ROUGHNESS );
						pushConstBlockMaterial.baseColorFactor = primitive->material.baseColorFactor;
						pushConstBlockMaterial.metallicFactor = primitive->material.metallicFactor;
						pushConstBlockMaterial.roughnessFactor = primitive->material.roughnessFactor;
						pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive->material.metallicRoughnessTexture != nullptr ? primitive->material.texCoordSets.metallicRoughness : -1;
						pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
					}

					if ( primitive->material.pbrWorkflows.specularGlossiness )
					{
						// Specular glossiness workflow
						pushConstBlockMaterial.workflow = static_cast< float >( PBR_WORKFLOW_SPECULAR_GLOSINESS );
						pushConstBlockMaterial.PhysicalDescriptorTextureSet =
							primitive->material.extension.specularGlossinessTexture != nullptr ? primitive->material.texCoordSets.specularGlossiness : -1;
						pushConstBlockMaterial.colorTextureSet = primitive->material.extension.diffuseTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
						pushConstBlockMaterial.diffuseFactor = primitive->material.extension.diffuseFactor;
						pushConstBlockMaterial.specularFactor = glm::vec4( primitive->material.extension.specularFactor, 1.0f );
					}

					vkCmdPushConstants( m_vecFrameData[ unCmdBufIndex ].vkCommandBuffer, vkPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstBlockMaterial ), &pushConstBlockMaterial );

					if ( primitive->hasIndices )
					{
						vkCmdDrawIndexed( m_vecFrameData[ unCmdBufIndex ].vkCommandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0 );
					}
					else
					{
						vkCmdDraw( m_vecFrameData[ unCmdBufIndex ].vkCommandBuffer, primitive->vertexCount, 1, 0, 0 );
					}
				}
			}
		};
		for ( auto child : gltfNode->children )
		{
			RenderNode( renderable, child, unCmdBufIndex, gltfAlphaMode );
		}
	}

	void Render::LoadAssets()
	{
		assert( skybox );

		// Create vismask buffers if available
		for ( uint32_t i = 0; i < static_cast< uint32_t >( m_vecVisMaskBuffers.size() ); i++ )
		{
			if ( !m_vecVisMasks[ i ].indices.empty() )
			{
				m_vecVisMaskBuffers[ i ].indexBuffer.create(
					m_pVulkanDevice,
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					sizeof( uint32_t ) * m_vecVisMasks[ i ].indices.size(),
					m_vecVisMasks[ i ].indices.data() );

				m_vecVisMaskBuffers[ i ].vertexBuffer.create(
					m_pVulkanDevice,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					sizeof( XrVector2f ) * m_vecVisMasks[ i ].vertices.size(),
					m_vecVisMasks[ i ].vertices.data() );
			}
		}

		// skybox
#if defined( VK_USE_PLATFORM_ANDROID_KHR )
		tinygltf::asset_manager = m_SharedState.androidAssetManager;
		readDirectory( m_SharedState.androidAssetManager, "textures", "*.ktx", mapEnvironments, false );
#else
		readDirectory( "textures", "*.ktx", mapEnvironments, false );
#endif

		// Read textures directory
		if ( m_eMinLogLevel == ELogLevel::LogVerbose )
		{
			LogVerbose( "The following ktx textures were found in this project: " );
			for ( auto &environment : mapEnvironments )
			{
				LogVerbose( "\t%s\t:\t%s", environment.first.c_str(), environment.second.c_str() );
			}
		}

		// Load empty texture
		textures.empty.loadFromFile( "textures/empty.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_SharedState.vkQueue );

		// Load skybox
		skybox->gltfModel.loadFromFile( skybox->sFilename, m_pVulkanDevice, m_SharedState.vkQueue );
		LoadEnvironment( skyboxTexture.c_str() );

		// Load all renderables from storage
		LoadGltfScenes();
	}

	void Render::LoadGltfScene( RenderSceneBase *renderable )
	{
		LogInfo( "gltf file %s started loading", renderable->sFilename.c_str() );

		// reset
		renderable->bIsVisible = false;
		renderable->gltfModel.destroy( m_SharedState.vkDevice );
		nAnimationIndex = 0;
		fAnimationTimer = 0.0f;

		// load async
		auto tStart = std::chrono::high_resolution_clock::now();
		renderable->gltfModel.loadFromFile( renderable->sFilename, m_pVulkanDevice, m_SharedState.vkQueue );
		auto tFileLoad = std::chrono::duration< double, std::milli >( std::chrono::high_resolution_clock::now() - tStart ).count();

		renderable->bIsVisible = true;
		LogInfo( "gltf file %s loaded. Took %lf ms", renderable->sFilename.c_str(), tFileLoad );
	}

	void Render::LoadGltfScenes()
	{
		// Scenes
		for ( auto &renderable : vecRenderScenes )
		{
			LoadGltfScene(renderable );
		}

		// Sectors
		for ( auto &renderable : vecRenderSectors )
		{
			LoadGltfScene(renderable );
		}

		// Models
		for ( auto &renderable : vecRenderModels )
		{
			LoadGltfScene(renderable );
		}
	}

	void Render::PrepareAllPipelines()
	{
		GenerateBRDFLUT();
		GenerateCubemaps();
		PrepareUniformBuffers();
		SetupDescriptors();
		PreparePipelines();
	}

	void Render::LoadEnvironment( std::string sFilename )
	{
		LogInfo( "Loading environment %s", sFilename.c_str() );
		if ( textures.environmentCube.image )
		{
			textures.environmentCube.destroy();
			textures.irradianceCube.destroy();
			textures.prefilteredCube.destroy();
		}
		textures.environmentCube.loadFromFile( sFilename, VK_FORMAT_R16G16B16A16_SFLOAT, m_pVulkanDevice, m_SharedState.vkQueue );
		GenerateCubemaps();
	}

	void Render::GenerateCubemaps()
	{
		enum Target
		{
			IRRADIANCE = 0,
			PREFILTEREDENV = 1
		};

		for ( uint32_t target = 0; target < PREFILTEREDENV + 1; target++ )
		{
			vks::TextureCubeMap cubemap;
			auto tStart = std::chrono::high_resolution_clock::now();

			VkFormat format;
			int32_t dim;

			switch ( target )
			{
				case IRRADIANCE:
					format = VK_FORMAT_R32G32B32A32_SFLOAT;
					dim = 64;
					break;
				case PREFILTEREDENV:
					format = VK_FORMAT_R16G16B16A16_SFLOAT;
					dim = 512;
					break;
			};

			const uint32_t numMips = static_cast< uint32_t >( floor( log2( dim ) ) ) + 1;

			// Create target cubemap
			{
				// Image
				VkImageCreateInfo imageCI {};
				imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCI.imageType = VK_IMAGE_TYPE_2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = numMips;
				imageCI.arrayLayers = 6;
				imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
				VK_CHECK_RESULT( vkCreateImage( m_SharedState.vkDevice, &imageCI, nullptr, &cubemap.image ) );

				VkMemoryRequirements memReqs;
				vkGetImageMemoryRequirements( m_SharedState.vkDevice, cubemap.image, &memReqs );

				VkMemoryAllocateInfo memAllocInfo {};
				memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
				VK_CHECK_RESULT( vkAllocateMemory( m_SharedState.vkDevice, &memAllocInfo, nullptr, &cubemap.deviceMemory ) );
				VK_CHECK_RESULT( vkBindImageMemory( m_SharedState.vkDevice, cubemap.image, cubemap.deviceMemory, 0 ) );

				// View
				VkImageViewCreateInfo viewCI {};
				viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
				viewCI.format = format;
				viewCI.subresourceRange = {};
				viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				viewCI.subresourceRange.levelCount = numMips;
				viewCI.subresourceRange.layerCount = 6;
				viewCI.image = cubemap.image;
				VK_CHECK_RESULT( vkCreateImageView( m_SharedState.vkDevice, &viewCI, nullptr, &cubemap.view ) );

				// Sampler
				VkSamplerCreateInfo samplerCI {};
				samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCI.magFilter = VK_FILTER_LINEAR;
				samplerCI.minFilter = VK_FILTER_LINEAR;
				samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				samplerCI.minLod = 0.0f;
				samplerCI.maxLod = static_cast< float >( numMips );
				samplerCI.maxAnisotropy = 1.0f;
				samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
				VK_CHECK_RESULT( vkCreateSampler( m_SharedState.vkDevice, &samplerCI, nullptr, &cubemap.sampler ) );
			}

			// FB, Att, RP, Pipe, etc.
			VkAttachmentDescription attDesc {};
			// Color attachment
			attDesc.format = format;
			attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpassDescription {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;

			// Use subpass dependencies for layout transitions
			std::array< VkSubpassDependency, 2 > dependencies;
			dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 0 ].dstSubpass = 0;
			dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[ 0 ].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[ 0 ].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			dependencies[ 1 ].srcSubpass = 0;
			dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[ 1 ].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[ 1 ].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Renderpass
			VkRenderPassCreateInfo renderPassCI {};
			renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCI.attachmentCount = 1;
			renderPassCI.pAttachments = &attDesc;
			renderPassCI.subpassCount = 1;
			renderPassCI.pSubpasses = &subpassDescription;
			renderPassCI.dependencyCount = 2;
			renderPassCI.pDependencies = dependencies.data();
			VkRenderPass renderpass;
			VK_CHECK_RESULT( vkCreateRenderPass( m_SharedState.vkDevice, &renderPassCI, nullptr, &renderpass ) );

			struct Offscreen
			{
				VkImage image;
				VkImageView view;
				VkDeviceMemory memory;
				VkFramebuffer framebuffer;
			} offscreen;

			// Create offscreen framebuffer
			{
				// Image
				VkImageCreateInfo imageCI {};
				imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCI.imageType = VK_IMAGE_TYPE_2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = 1;
				imageCI.arrayLayers = 1;
				imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				VK_CHECK_RESULT( vkCreateImage( m_SharedState.vkDevice, &imageCI, nullptr, &offscreen.image ) );

				VkMemoryRequirements memReqs;
				vkGetImageMemoryRequirements( m_SharedState.vkDevice, offscreen.image, &memReqs );
				VkMemoryAllocateInfo memAllocInfo {};
				memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

				VK_CHECK_RESULT( vkAllocateMemory( m_SharedState.vkDevice, &memAllocInfo, nullptr, &offscreen.memory ) );
				VK_CHECK_RESULT( vkBindImageMemory( m_SharedState.vkDevice, offscreen.image, offscreen.memory, 0 ) );

				// View
				VkImageViewCreateInfo viewCI {};
				viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCI.format = format;
				viewCI.flags = 0;
				viewCI.subresourceRange = {};
				viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				viewCI.subresourceRange.baseMipLevel = 0;
				viewCI.subresourceRange.levelCount = 1;
				viewCI.subresourceRange.baseArrayLayer = 0;
				viewCI.subresourceRange.layerCount = 1;
				viewCI.image = offscreen.image;
				VK_CHECK_RESULT( vkCreateImageView( m_SharedState.vkDevice, &viewCI, nullptr, &offscreen.view ) );

				// Framebuffer
				VkFramebufferCreateInfo framebufferCI {};
				framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCI.renderPass = renderpass;
				framebufferCI.attachmentCount = 1;
				framebufferCI.pAttachments = &offscreen.view;
				framebufferCI.width = dim;
				framebufferCI.height = dim;
				framebufferCI.layers = 1;
				VK_CHECK_RESULT( vkCreateFramebuffer( m_SharedState.vkDevice, &framebufferCI, nullptr, &offscreen.framebuffer ) );

				VkCommandBuffer layoutCmd = m_pVulkanDevice->createCommandBuffer( VK_COMMAND_BUFFER_LEVEL_PRIMARY, true );
				VkImageMemoryBarrier imageMemoryBarrier {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.image = offscreen.image;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				vkCmdPipelineBarrier( layoutCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
				m_pVulkanDevice->flushCommandBuffer( layoutCmd, m_SharedState.vkQueue, true );
			}

			// Descriptors
			VkDescriptorSetLayout descriptorsetlayout;
			VkDescriptorSetLayoutBinding setLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
			descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCI.pBindings = &setLayoutBinding;
			descriptorSetLayoutCI.bindingCount = 1;
			VK_CHECK_RESULT( vkCreateDescriptorSetLayout( m_SharedState.vkDevice, &descriptorSetLayoutCI, nullptr, &descriptorsetlayout ) );

			// Descriptor Pool
			VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
			VkDescriptorPoolCreateInfo descriptorPoolCI {};
			descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolCI.poolSizeCount = 1;
			descriptorPoolCI.pPoolSizes = &poolSize;
			descriptorPoolCI.maxSets = 2;
			VkDescriptorPool descriptorpool;
			VK_CHECK_RESULT( vkCreateDescriptorPool( m_SharedState.vkDevice, &descriptorPoolCI, nullptr, &descriptorpool ) );

			// Descriptor sets
			VkDescriptorSet descriptorset;
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo {};
			descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocInfo.descriptorPool = descriptorpool;
			descriptorSetAllocInfo.pSetLayouts = &descriptorsetlayout;
			descriptorSetAllocInfo.descriptorSetCount = 1;
			VK_CHECK_RESULT( vkAllocateDescriptorSets( m_SharedState.vkDevice, &descriptorSetAllocInfo, &descriptorset ) );

			VkWriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorset;
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.pImageInfo = &textures.environmentCube.descriptor;
			vkUpdateDescriptorSets( m_SharedState.vkDevice, 1, &writeDescriptorSet, 0, nullptr );

			struct PushBlockIrradiance
			{
				glm::mat4 mvp;
				float deltaPhi = ( 2.0f * float( M_PI ) ) / 180.0f;
				float deltaTheta = ( 0.5f * float( M_PI ) ) / 64.0f;
			} pushBlockIrradiance;

			struct PushBlockPrefilterEnv
			{
				glm::mat4 mvp;
				float roughness;
				uint32_t numSamples = 32u;
			} pushBlockPrefilterEnv;

			// Pipeline layout
			VkPipelineLayout pipelinelayout;
			VkPushConstantRange pushConstantRange {};
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			switch ( target )
			{
				case IRRADIANCE:
					pushConstantRange.size = sizeof( PushBlockIrradiance );
					break;
				case PREFILTEREDENV:
					pushConstantRange.size = sizeof( PushBlockPrefilterEnv );
					break;
			};

			VkPipelineLayoutCreateInfo pipelineLayoutCI {};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = 1;
			pipelineLayoutCI.pSetLayouts = &descriptorsetlayout;
			pipelineLayoutCI.pushConstantRangeCount = 1;
			pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
			VK_CHECK_RESULT( vkCreatePipelineLayout( m_SharedState.vkDevice, &pipelineLayoutCI, nullptr, &pipelinelayout ) );

			// Pipeline
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI {};
			inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkPipelineRasterizationStateCreateInfo rasterizationStateCI {};
			rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
			rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizationStateCI.lineWidth = 1.0f;

			VkPipelineColorBlendAttachmentState blendAttachmentState {};
			blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			blendAttachmentState.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo colorBlendStateCI {};
			colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCI.attachmentCount = 1;
			colorBlendStateCI.pAttachments = &blendAttachmentState;

			VkPipelineDepthStencilStateCreateInfo depthStencilStateCI {};
			depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCI.depthTestEnable = VK_FALSE;
			depthStencilStateCI.depthWriteEnable = VK_FALSE;
			depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthStencilStateCI.front = depthStencilStateCI.back;
			depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;

			VkPipelineViewportStateCreateInfo viewportStateCI {};
			viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCI.viewportCount = 0; // 1;
			viewportStateCI.scissorCount = 0;  // 1;

			VkPipelineMultisampleStateCreateInfo multisampleStateCI {};
			multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			std::vector< VkDynamicState > dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicStateCI {};
			dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
			dynamicStateCI.dynamicStateCount = static_cast< uint32_t >( dynamicStateEnables.size() );

			// Vertex input state
			VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof( vkglTF::Model::Vertex ), VK_VERTEX_INPUT_RATE_VERTEX };
			VkVertexInputAttributeDescription vertexInputAttribute = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

			VkPipelineVertexInputStateCreateInfo vertexInputStateCI {};
			vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputStateCI.vertexBindingDescriptionCount = 1;
			vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
			vertexInputStateCI.vertexAttributeDescriptionCount = 1;
			vertexInputStateCI.pVertexAttributeDescriptions = &vertexInputAttribute;

			std::array< VkPipelineShaderStageCreateInfo, 2 > shaderStages;

			VkGraphicsPipelineCreateInfo pipelineCI {};
			pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCI.layout = pipelinelayout;
			pipelineCI.renderPass = renderpass;
			pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
			pipelineCI.pVertexInputState = &vertexInputStateCI;
			pipelineCI.pRasterizationState = &rasterizationStateCI;
			pipelineCI.pColorBlendState = &colorBlendStateCI;
			pipelineCI.pMultisampleState = &multisampleStateCI;
			pipelineCI.pViewportState = &viewportStateCI;
			pipelineCI.pDepthStencilState = &depthStencilStateCI;
			pipelineCI.pDynamicState = &dynamicStateCI;
			pipelineCI.stageCount = 2;
			pipelineCI.pStages = shaderStages.data();
			pipelineCI.renderPass = renderpass;

#ifdef XR_USE_PLATFORM_ANDROID
			shaderStages[ 0 ] = loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT );
#else
			shaderStages[ 0 ] = loadShader( m_SharedState.vkDevice, "shaders/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT );
#endif
			switch ( target )
			{
				case IRRADIANCE:
#ifdef XR_USE_PLATFORM_ANDROID
					shaderStages[ 1 ] = loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT );
#else
					shaderStages[ 1 ] = loadShader( m_SharedState.vkDevice, "shaders/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT );
#endif
					break;

				case PREFILTEREDENV:
#ifdef XR_USE_PLATFORM_ANDROID
					shaderStages[ 1 ] = loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT );
#else
					shaderStages[ 1 ] = loadShader( m_SharedState.vkDevice, "shaders/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT );
#endif
					break;
			};
			VkPipeline pipeline;
			VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipeline ) );
			for ( auto shaderStage : shaderStages )
			{
				vkDestroyShaderModule( m_SharedState.vkDevice, shaderStage.module, nullptr );
			}

			// Render cubemap
			VkClearValue clearValues[ 1 ];
			clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

			VkRenderPassBeginInfo renderPassBeginInfo {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderpass;
			renderPassBeginInfo.framebuffer = offscreen.framebuffer;
			renderPassBeginInfo.renderArea.extent.width = dim;
			renderPassBeginInfo.renderArea.extent.height = dim;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			std::vector< glm::mat4 > matrices = {
				glm::rotate( glm::rotate( glm::mat4( 1.0f ), glm::radians( 90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
				glm::rotate( glm::rotate( glm::mat4( 1.0f ), glm::radians( -90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
				glm::rotate( glm::mat4( 1.0f ), glm::radians( -90.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
				glm::rotate( glm::mat4( 1.0f ), glm::radians( 90.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
				glm::rotate( glm::mat4( 1.0f ), glm::radians( 180.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) ),
				glm::rotate( glm::mat4( 1.0f ), glm::radians( 180.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) ),
			};

			VkCommandBuffer cmdBuf = m_pVulkanDevice->createCommandBuffer( VK_COMMAND_BUFFER_LEVEL_PRIMARY, false );

			VkViewport viewport {};
			viewport.width = ( float )dim;
			viewport.height = ( float )dim;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor {};
			scissor.extent.width = dim;
			scissor.extent.height = dim;

			VkImageSubresourceRange subresourceRange {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = numMips;
			subresourceRange.layerCount = 6;

			// Change image layout for all cubemap faces to transfer destination
			{
				m_pVulkanDevice->beginCommandBuffer( cmdBuf );
				VkImageMemoryBarrier imageMemoryBarrier {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.image = cubemap.image;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				vkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
				m_pVulkanDevice->flushCommandBuffer( cmdBuf, m_SharedState.vkQueue, false );
			}

			for ( uint32_t m = 0; m < numMips; m++ )
			{
				for ( uint32_t f = 0; f < 6; f++ )
				{
					m_pVulkanDevice->beginCommandBuffer( cmdBuf );

					viewport.width = static_cast< float >( dim * std::pow( 0.5f, m ) );
					viewport.height = static_cast< float >( dim * std::pow( 0.5f, m ) );
					vkCmdSetViewport( cmdBuf, 0, 1, &viewport );
					vkCmdSetScissor( cmdBuf, 0, 1, &scissor );

					// Render scene from cube face's point of view
					vkCmdBeginRenderPass( cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

					// Pass parameters for current pass using a push constant block
					switch ( target )
					{
						case IRRADIANCE:
							pushBlockIrradiance.mvp = glm::perspective( ( float )( M_PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * matrices[ f ];
							vkCmdPushConstants( cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushBlockIrradiance ), &pushBlockIrradiance );
							break;
						case PREFILTEREDENV:
							pushBlockPrefilterEnv.mvp = glm::perspective( ( float )( M_PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * matrices[ f ];
							pushBlockPrefilterEnv.roughness = ( float )m / ( float )( numMips - 1 );
							vkCmdPushConstants( cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushBlockPrefilterEnv ), &pushBlockPrefilterEnv );
							break;
					};

					vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
					vkCmdBindDescriptorSets( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL );

					VkDeviceSize offsets[ 1 ] = { 0 };

					skybox->gltfModel.draw( cmdBuf );

					vkCmdEndRenderPass( cmdBuf );

					VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
					subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					subresourceRange.baseMipLevel = 0;
					subresourceRange.levelCount = numMips;
					subresourceRange.layerCount = 6;

					{
						VkImageMemoryBarrier imageMemoryBarrier {};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.image = offscreen.image;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
						vkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
					}

					// Copy region for transfer from framebuffer to cube face
					VkImageCopy copyRegion {};

					copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copyRegion.srcSubresource.baseArrayLayer = 0;
					copyRegion.srcSubresource.mipLevel = 0;
					copyRegion.srcSubresource.layerCount = 1;
					copyRegion.srcOffset = { 0, 0, 0 };

					copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copyRegion.dstSubresource.baseArrayLayer = f;
					copyRegion.dstSubresource.mipLevel = m;
					copyRegion.dstSubresource.layerCount = 1;
					copyRegion.dstOffset = { 0, 0, 0 };

					copyRegion.extent.width = static_cast< uint32_t >( viewport.width );
					copyRegion.extent.height = static_cast< uint32_t >( viewport.height );
					copyRegion.extent.depth = 1;

					vkCmdCopyImage( cmdBuf, offscreen.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion );

					{
						VkImageMemoryBarrier imageMemoryBarrier {};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.image = offscreen.image;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
						vkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
					}

					m_pVulkanDevice->flushCommandBuffer( cmdBuf, m_SharedState.vkQueue, false );
				}
			}

			{
				m_pVulkanDevice->beginCommandBuffer( cmdBuf );
				VkImageMemoryBarrier imageMemoryBarrier {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.image = cubemap.image;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				vkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
				m_pVulkanDevice->flushCommandBuffer( cmdBuf, m_SharedState.vkQueue, false );
			}

			vkDestroyRenderPass( m_SharedState.vkDevice, renderpass, nullptr );
			vkDestroyFramebuffer( m_SharedState.vkDevice, offscreen.framebuffer, nullptr );
			vkFreeMemory( m_SharedState.vkDevice, offscreen.memory, nullptr );
			vkDestroyImageView( m_SharedState.vkDevice, offscreen.view, nullptr );
			vkDestroyImage( m_SharedState.vkDevice, offscreen.image, nullptr );
			vkDestroyDescriptorPool( m_SharedState.vkDevice, descriptorpool, nullptr );
			vkDestroyDescriptorSetLayout( m_SharedState.vkDevice, descriptorsetlayout, nullptr );
			vkDestroyPipeline( m_SharedState.vkDevice, pipeline, nullptr );
			vkDestroyPipelineLayout( m_SharedState.vkDevice, pipelinelayout, nullptr );

			cubemap.descriptor.imageView = cubemap.view;
			cubemap.descriptor.sampler = cubemap.sampler;
			cubemap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			cubemap.device = m_pVulkanDevice;

			switch ( target )
			{
				case IRRADIANCE:
					textures.irradianceCube = cubemap;
					break;
				case PREFILTEREDENV:
					textures.prefilteredCube = cubemap;
					shaderValuesPbrParams.prefilteredCubeMipLevels = static_cast< float >( numMips );
					break;
			};

			auto tEnd = std::chrono::high_resolution_clock::now();
			auto tDiff = std::chrono::duration< double, std::milli >( tEnd - tStart ).count();
			std::cout << "Generating cube map with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
		}
	}

	void Render::GenerateBRDFLUT()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
		const int32_t dim = 512;

		// Image
		VkImageCreateInfo imageCI {};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT( vkCreateImage( m_SharedState.vkDevice, &imageCI, nullptr, &textures.lutBrdf.image ) );

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements( m_SharedState.vkDevice, textures.lutBrdf.image, &memReqs );
		VkMemoryAllocateInfo memAllocInfo {};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK_RESULT( vkAllocateMemory( m_SharedState.vkDevice, &memAllocInfo, nullptr, &textures.lutBrdf.deviceMemory ) );
		VK_CHECK_RESULT( vkBindImageMemory( m_SharedState.vkDevice, textures.lutBrdf.image, textures.lutBrdf.deviceMemory, 0 ) );

		// View
		VkImageViewCreateInfo viewCI {};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = textures.lutBrdf.image;
		VK_CHECK_RESULT( vkCreateImageView( m_SharedState.vkDevice, &viewCI, nullptr, &textures.lutBrdf.view ) );

		// Sampler
		VkSamplerCreateInfo samplerCI {};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT( vkCreateSampler( m_SharedState.vkDevice, &samplerCI, nullptr, &textures.lutBrdf.sampler ) );

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array< VkSubpassDependency, 2 > dependencies;
		dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 0 ].dstSubpass = 0;
		dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[ 0 ].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[ 0 ].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[ 1 ].srcSubpass = 0;
		dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[ 1 ].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[ 1 ].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassCI {};
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();

		VkRenderPass renderpass;
		VK_CHECK_RESULT( vkCreateRenderPass( m_SharedState.vkDevice, &renderPassCI, nullptr, &renderpass ) );

		VkFramebufferCreateInfo framebufferCI {};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &textures.lutBrdf.view;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		VK_CHECK_RESULT( vkCreateFramebuffer( m_SharedState.vkDevice, &framebufferCI, nullptr, &framebuffer ) );

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
		descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		VK_CHECK_RESULT( vkCreateDescriptorSetLayout( m_SharedState.vkDevice, &descriptorSetLayoutCI, nullptr, &descriptorsetlayout ) );

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI {};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &descriptorsetlayout;
		VK_CHECK_RESULT( vkCreatePipelineLayout( m_SharedState.vkDevice, &pipelineLayoutCI, nullptr, &pipelinelayout ) );

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI {};
		inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCI {};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateCI.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState {};
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendStateCI {};
		colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendStateCI.attachmentCount = 1;
		colorBlendStateCI.pAttachments = &blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI {};
		depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCI.depthTestEnable = VK_FALSE;
		depthStencilStateCI.depthWriteEnable = VK_FALSE;
		depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilStateCI.front = depthStencilStateCI.back;
		depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;

		VkPipelineViewportStateCreateInfo viewportStateCI {};
		viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCI.viewportCount = 1;
		viewportStateCI.scissorCount = 1;

		VkPipelineMultisampleStateCreateInfo multisampleStateCI {};
		multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		std::vector< VkDynamicState > dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI {};
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
		dynamicStateCI.dynamicStateCount = static_cast< uint32_t >( dynamicStateEnables.size() );

		VkPipelineVertexInputStateCreateInfo emptyInputStateCI {};
		emptyInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		std::array< VkPipelineShaderStageCreateInfo, 2 > shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI {};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.layout = pipelinelayout;
		pipelineCI.renderPass = renderpass;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pVertexInputState = &emptyInputStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();

		// Look-up-table (from BRDF) pipeline
#ifdef XR_USE_PLATFORM_ANDROID
		shaderStages = {
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#else
		shaderStages = {
			loadShader( m_SharedState.vkDevice, "shaders/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.vkDevice, "shaders/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#endif

		VkPipeline pipeline;
		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipeline ) );

		for ( auto shaderStage : shaderStages )
		{
			vkDestroyShaderModule( m_SharedState.vkDevice, shaderStage.module, nullptr );
		}

		// Render
		VkClearValue clearValues[ 1 ];
		clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = framebuffer;

		VkCommandBuffer cmdBuf = m_pVulkanDevice->createCommandBuffer( VK_COMMAND_BUFFER_LEVEL_PRIMARY, true );
		vkCmdBeginRenderPass( cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

		VkViewport viewport {};
		viewport.width = ( float )dim;
		viewport.height = ( float )dim;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor {};
		scissor.extent.width = dim;
		scissor.extent.height = dim;

		vkCmdSetViewport( cmdBuf, 0, 1, &viewport );
		vkCmdSetScissor( cmdBuf, 0, 1, &scissor );
		vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		vkCmdDraw( cmdBuf, 3, 1, 0, 0 );
		vkCmdEndRenderPass( cmdBuf );
		m_pVulkanDevice->flushCommandBuffer( cmdBuf, m_SharedState.vkQueue );

		vkQueueWaitIdle( m_SharedState.vkQueue );

		vkDestroyPipeline( m_SharedState.vkDevice, pipeline, nullptr );
		vkDestroyPipelineLayout( m_SharedState.vkDevice, pipelinelayout, nullptr );
		vkDestroyRenderPass( m_SharedState.vkDevice, renderpass, nullptr );
		vkDestroyFramebuffer( m_SharedState.vkDevice, framebuffer, nullptr );
		vkDestroyDescriptorSetLayout( m_SharedState.vkDevice, descriptorsetlayout, nullptr );

		textures.lutBrdf.descriptor.imageView = textures.lutBrdf.view;
		textures.lutBrdf.descriptor.sampler = textures.lutBrdf.sampler;
		textures.lutBrdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.lutBrdf.device = m_pVulkanDevice;

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration< double, std::milli >( tEnd - tStart ).count();
		LogInfo( "Generating BRDF LUT took %lf ms", tDiff );
	}

	void Render::PrepareUniformBuffers()
	{
		// skybox ubo set
		skyboxUniformBuffer.create( m_pVulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof( UBOMatrices ) );

		// renderables ubo sets
		for ( auto &uniformBuffer : vecUniformBuffers )
		{
			uniformBuffer.scene.create( m_pVulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof( UBOMatrices ) );
			uniformBuffer.params.create(
				m_pVulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof( ShaderValuesParams ) );
		}
	}

	void Render::UpdateUniformBuffers( UBOMatrices *uboMatrices, Buffer *buffer, RenderSceneBase *renderable, XrMatrix4x4f *matViewProjection, XrPosef *eyePose )
	{
		if ( !renderable->bIsVisible )
			return;

		// Retrieve the model matrix
		XrMatrix4x4f matModel;
		renderable->GetMatrix( &matModel );

		// Update ubo
		uboMatrices->vp = *matViewProjection;
		uboMatrices->model = matModel;
		uboMatrices->eyePos = eyePose->position;

		memcpy( buffer->mapped, uboMatrices, sizeof( UBOMatrices ) );
	}

	void Render::UpdateUniformBuffers( UBOMatrices *uboMatrices, Buffer *buffer, XrMatrix4x4f *matViewProjection, XrPosef *eyePose )
	{
		// Scenes
		for ( auto &renderable : vecRenderScenes )
		{
			UpdateUniformBuffers( uboMatrices, buffer, renderable, matViewProjection, eyePose );
		}

		// Sectors
		for ( auto &renderable : vecRenderSectors )
		{
			UpdateUniformBuffers( uboMatrices, buffer, renderable, matViewProjection, eyePose );
		}

		// Models
		for ( auto &renderable : vecRenderModels )
		{
			UpdateUniformBuffers( uboMatrices, buffer, renderable, matViewProjection, eyePose );
		}
	}

	void Render::UpdateRenderablePoses( oxr::Session *pSession, XrFrameState *pFrameState )
	{
		assert( pSession );
		assert( pFrameState );

		XrSpaceLocation xrSpaceLocation { XR_TYPE_SPACE_LOCATION };
		pSession->LocateAppSpace( pFrameState->predictedDisplayTime, &xrSpaceLocation );

		// Scenes
		for ( auto &renderable : vecRenderScenes )
		{
			renderable->currentPose = xrSpaceLocation.pose;
		}

		// Sectors
		for ( auto &renderable : vecRenderSectors )
		{
			if ( renderable->xrSpace != XR_NULL_HANDLE )
			{
				XrSpaceLocation renderableSpaceLocation { XR_TYPE_SPACE_LOCATION };
				pSession->LocateSpace( pSession->GetReferenceSpace(), renderable->xrSpace, pFrameState->predictedDisplayTime, &renderableSpaceLocation );
				renderable->currentPose = renderableSpaceLocation.pose;
			}
			else
			{
				renderable->currentPose = xrSpaceLocation.pose;
			}
		}

		// Models
		for ( auto &renderable : vecRenderModels )
		{
			if ( renderable->xrSpace != XR_NULL_HANDLE )
			{
				XrSpaceLocation renderableSpaceLocation { XR_TYPE_SPACE_LOCATION };
				pSession->LocateSpace( pSession->GetReferenceSpace(), renderable->xrSpace, pFrameState->predictedDisplayTime, &renderableSpaceLocation );
				renderable->currentPose = renderableSpaceLocation.pose;
			}
			else
			{
				renderable->currentPose = xrSpaceLocation.pose;
			}
		}
	}

	void Render::CalculateDescriptorScope( vkglTF::Model *gltfModel, uint32_t *imageSamplerCount, uint32_t *materialCount, uint32_t *meshCount )
	{
		for ( auto &material : gltfModel->materials )
		{
			*imageSamplerCount += 5;
			*materialCount = *materialCount + 1;
		}
		for ( auto node : skybox->gltfModel.linearNodes )
		{
			if ( node->mesh )
			{
				*meshCount = *meshCount + 1;
			}
		}
	}

	void Render::AllocateDescriptorSet( vkglTF::Model *gltfModel )
	{
		for ( auto &material : gltfModel->materials )
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo {};
			descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocInfo.descriptorPool = vkDescriptorPool;
			descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.material;
			descriptorSetAllocInfo.descriptorSetCount = 1;
			VK_CHECK_RESULT( vkAllocateDescriptorSets( m_SharedState.vkDevice, &descriptorSetAllocInfo, &material.descriptorSet ) );

			std::vector< VkDescriptorImageInfo > imageDescriptors = {
				textures.empty.descriptor,
				textures.empty.descriptor,
				material.normalTexture ? material.normalTexture->descriptor : textures.empty.descriptor,
				material.occlusionTexture ? material.occlusionTexture->descriptor : textures.empty.descriptor,
				material.emissiveTexture ? material.emissiveTexture->descriptor : textures.empty.descriptor };

			// TODO: glTF specs states that metallic roughness should be preferred, even if specular glossiness is present
			if ( material.pbrWorkflows.metallicRoughness )
			{
				if ( material.baseColorTexture )
				{
					imageDescriptors[ 0 ] = material.baseColorTexture->descriptor;
				}
				if ( material.metallicRoughnessTexture )
				{
					imageDescriptors[ 1 ] = material.metallicRoughnessTexture->descriptor;
				}
			}

			if ( material.pbrWorkflows.specularGlossiness )
			{
				if ( material.extension.diffuseTexture )
				{
					imageDescriptors[ 0 ] = material.extension.diffuseTexture->descriptor;
				}
				if ( material.extension.specularGlossinessTexture )
				{
					imageDescriptors[ 1 ] = material.extension.specularGlossinessTexture->descriptor;
				}
			}

			std::array< VkWriteDescriptorSet, 5 > writeDescriptorSets {};
			for ( size_t i = 0; i < imageDescriptors.size(); i++ )
			{
				writeDescriptorSets[ i ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ i ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets[ i ].descriptorCount = 1;
				writeDescriptorSets[ i ].dstSet = material.descriptorSet;
				writeDescriptorSets[ i ].dstBinding = static_cast< uint32_t >( i );
				writeDescriptorSets[ i ].pImageInfo = &imageDescriptors[ i ];
			}

			vkUpdateDescriptorSets( m_SharedState.vkDevice, static_cast< uint32_t >( writeDescriptorSets.size() ), writeDescriptorSets.data(), 0, NULL );
		}
	}

	void Render::SetupDescriptors()
	{
		/*
			Descriptor Pool
		*/
		uint32_t imageSamplerCount = 0;
		uint32_t materialCount = 0;
		uint32_t meshCount = 0;

		// Environment samplers (radiance, irradiance, brdf lut)
		imageSamplerCount += 3;

		// Scenes
		for ( auto &renderable : vecRenderScenes )
		{
			CalculateDescriptorScope( &renderable->gltfModel, &imageSamplerCount, &materialCount, &meshCount );
		}

		// Sectors
		for ( auto &renderable : vecRenderSectors )
		{
			CalculateDescriptorScope( &renderable->gltfModel, &imageSamplerCount, &materialCount, &meshCount );
		}

		// Models
		for ( auto &renderable : vecRenderModels )
		{
			CalculateDescriptorScope( &renderable->gltfModel, &imageSamplerCount, &materialCount, &meshCount );
		}

		std::vector< VkDescriptorPoolSize > poolSizes = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ( 4 + meshCount ) * k_unCommandBufferNum }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount * k_unCommandBufferNum } };
		VkDescriptorPoolCreateInfo descriptorPoolCI {};
		descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCI.poolSizeCount = 2;
		descriptorPoolCI.pPoolSizes = poolSizes.data();
		descriptorPoolCI.maxSets = ( 2 + materialCount + meshCount ) * k_unCommandBufferNum;
		VK_CHECK_RESULT( vkCreateDescriptorPool( m_SharedState.vkDevice, &descriptorPoolCI, nullptr, &vkDescriptorPool ) );

		/*
			Descriptor sets
		*/

		// Scene (matrices and environment maps)
		{
			std::vector< VkDescriptorSetLayoutBinding > setLayoutBindings = {
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			};
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
			descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
			descriptorSetLayoutCI.bindingCount = static_cast< uint32_t >( setLayoutBindings.size() );
			VK_CHECK_RESULT( vkCreateDescriptorSetLayout( m_SharedState.vkDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.scene ) );

			for ( auto i = 0; i < vecDescriptorSets.size(); i++ )
			{

				VkDescriptorSetAllocateInfo descriptorSetAllocInfo {};
				descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptorSetAllocInfo.descriptorPool = vkDescriptorPool;
				descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.scene;
				descriptorSetAllocInfo.descriptorSetCount = 1;
				VK_CHECK_RESULT( vkAllocateDescriptorSets( m_SharedState.vkDevice, &descriptorSetAllocInfo, &vecDescriptorSets[ i ].scene ) );

				std::array< VkWriteDescriptorSet, 5 > writeDescriptorSets {};

				writeDescriptorSets[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ 0 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSets[ 0 ].descriptorCount = 1;
				writeDescriptorSets[ 0 ].dstSet = vecDescriptorSets[ i ].scene;
				writeDescriptorSets[ 0 ].dstBinding = 0;
				writeDescriptorSets[ 0 ].pBufferInfo = &vecUniformBuffers[ i ].scene.descriptor;

				writeDescriptorSets[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ 1 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSets[ 1 ].descriptorCount = 1;
				writeDescriptorSets[ 1 ].dstSet = vecDescriptorSets[ i ].scene;
				writeDescriptorSets[ 1 ].dstBinding = 1;
				writeDescriptorSets[ 1 ].pBufferInfo = &vecUniformBuffers[ i ].params.descriptor;

				writeDescriptorSets[ 2 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ 2 ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets[ 2 ].descriptorCount = 1;
				writeDescriptorSets[ 2 ].dstSet = vecDescriptorSets[ i ].scene;
				writeDescriptorSets[ 2 ].dstBinding = 2;
				writeDescriptorSets[ 2 ].pImageInfo = &textures.irradianceCube.descriptor;

				writeDescriptorSets[ 3 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ 3 ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets[ 3 ].descriptorCount = 1;
				writeDescriptorSets[ 3 ].dstSet = vecDescriptorSets[ i ].scene;
				writeDescriptorSets[ 3 ].dstBinding = 3;
				writeDescriptorSets[ 3 ].pImageInfo = &textures.prefilteredCube.descriptor;

				writeDescriptorSets[ 4 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[ 4 ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets[ 4 ].descriptorCount = 1;
				writeDescriptorSets[ 4 ].dstSet = vecDescriptorSets[ i ].scene;
				writeDescriptorSets[ 4 ].dstBinding = 4;
				writeDescriptorSets[ 4 ].pImageInfo = &textures.lutBrdf.descriptor;

				vkUpdateDescriptorSets( m_SharedState.vkDevice, static_cast< uint32_t >( writeDescriptorSets.size() ), writeDescriptorSets.data(), 0, NULL );
			}
		}

		// Material (samplers)
		{
			std::vector< VkDescriptorSetLayoutBinding > setLayoutBindings = {
				{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			};
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
			descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
			descriptorSetLayoutCI.bindingCount = static_cast< uint32_t >( setLayoutBindings.size() );
			VK_CHECK_RESULT( vkCreateDescriptorSetLayout( m_SharedState.vkDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.material ) );

			// Scenes: Per-Material descriptor sets
			for ( auto &renderable : vecRenderScenes )
			{
				AllocateDescriptorSet( &renderable->gltfModel );
			}

			// Sectors: Per-Material descriptor sets
			for ( auto &renderable : vecRenderSectors )
			{
				AllocateDescriptorSet( &renderable->gltfModel );
			}

			// Models: Per-Material descriptor sets
			for ( auto &renderable : vecRenderModels )
			{
				AllocateDescriptorSet( &renderable->gltfModel );
			}

			// Model node (matrices)
			{
				std::vector< VkDescriptorSetLayoutBinding > setLayoutBindings = {
					{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
				};
				VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
				descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
				descriptorSetLayoutCI.bindingCount = static_cast< uint32_t >( setLayoutBindings.size() );
				VK_CHECK_RESULT( vkCreateDescriptorSetLayout( m_SharedState.vkDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.node ) );

				// Scenes: Per-Node descriptor set
				for ( auto &renderable : vecRenderScenes )
				{
					for ( auto &node : renderable->gltfModel.nodes )
					{
						SetupNodeDescriptorSet( node );
					}
				}

				// Sectors: Per-Node descriptor set
				for ( auto &renderable : vecRenderSectors )
				{
					for ( auto &node : renderable->gltfModel.nodes )
					{
						SetupNodeDescriptorSet( node );
					}
				}

				// Models: Per-Node descriptor set
				for ( auto &renderable : vecRenderModels )
				{
					for ( auto &node : renderable->gltfModel.nodes )
					{
						SetupNodeDescriptorSet( node );
					}
				}
			}
		}

		// Skybox (fixed set)
		for ( auto i = 0; i < vecUniformBuffers.size(); i++ )
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo {};
			descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocInfo.descriptorPool = vkDescriptorPool;
			descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.scene;
			descriptorSetAllocInfo.descriptorSetCount = 1;
			VK_CHECK_RESULT( vkAllocateDescriptorSets( m_SharedState.vkDevice, &descriptorSetAllocInfo, &vecDescriptorSets[ i ].skybox ) );

			std::array< VkWriteDescriptorSet, 3 > writeDescriptorSets {};

			writeDescriptorSets[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[ 0 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[ 0 ].descriptorCount = 1;
			writeDescriptorSets[ 0 ].dstSet = vecDescriptorSets[ i ].skybox;
			writeDescriptorSets[ 0 ].dstBinding = 0;
			writeDescriptorSets[ 0 ].pBufferInfo = &skyboxUniformBuffer.descriptor;

			writeDescriptorSets[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[ 1 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[ 1 ].descriptorCount = 1;
			writeDescriptorSets[ 1 ].dstSet = vecDescriptorSets[ i ].skybox;
			writeDescriptorSets[ 1 ].dstBinding = 1;
			writeDescriptorSets[ 1 ].pBufferInfo = &vecUniformBuffers[ i ].params.descriptor;

			writeDescriptorSets[ 2 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[ 2 ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[ 2 ].descriptorCount = 1;
			writeDescriptorSets[ 2 ].dstSet = vecDescriptorSets[ i ].skybox;
			writeDescriptorSets[ 2 ].dstBinding = 2;
			writeDescriptorSets[ 2 ].pImageInfo = &textures.prefilteredCube.descriptor;

			vkUpdateDescriptorSets( m_SharedState.vkDevice, static_cast< uint32_t >( writeDescriptorSets.size() ), writeDescriptorSets.data(), 0, nullptr );
		}
	}

	void Render::SetupNodeDescriptorSet( vkglTF::Node *node )
	{
		if ( node->mesh )
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo {};
			descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocInfo.descriptorPool = vkDescriptorPool;
			descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.node;
			descriptorSetAllocInfo.descriptorSetCount = 1;
			VK_CHECK_RESULT( vkAllocateDescriptorSets( m_SharedState.vkDevice, &descriptorSetAllocInfo, &node->mesh->uniformBuffer.descriptorSet ) );

			VkWriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = node->mesh->uniformBuffer.descriptorSet;
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.pBufferInfo = &node->mesh->uniformBuffer.descriptor;

			vkUpdateDescriptorSets( m_SharedState.vkDevice, 1, &writeDescriptorSet, 0, nullptr );
		}

		for ( auto &child : node->children )
		{
			SetupNodeDescriptorSet( child );
		}
	}

	void Render::PreparePipelines()
	{
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

		VkRect2D scissor = { { 0, 0 }, vkExtent };
		VkViewport viewport = { 0.0f, 0.0f, ( float )vkExtent.width, ( float )vkExtent.height, 0.0f, 1.0f };
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

		// PIPELINE LAYOUT: VisMask (push constants)
		VkPushConstantRange vkPushConstantVisMask = {};
		vkPushConstantVisMask.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vkPushConstantVisMask.offset = 0;
		vkPushConstantVisMask.size = 4 * 4 * sizeof( float );

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfoVismask { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutCreateInfoVismask.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfoVismask.pPushConstantRanges = &vkPushConstantVisMask;
		VK_CHECK_RESULT( vkCreatePipelineLayout( m_SharedState.vkDevice, &pipelineLayoutCreateInfoVismask, nullptr, &vkPipelineLayoutVisMask ) );

		// VERTEX BINDINGS: VisMask
		VkVertexInputBindingDescription vertexInputBindingVisMask = { 0, sizeof( XrVector2f ), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector< VkVertexInputAttributeDescription > vertexInputAttributesVisMask = { { 0, 0, VK_FORMAT_R32G32_SFLOAT } };

		VkPipelineVertexInputStateCreateInfo vertexInputStateCIVisMask {};
		vertexInputStateCIVisMask.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCIVisMask.vertexBindingDescriptionCount = 1;
		vertexInputStateCIVisMask.pVertexBindingDescriptions = &vertexInputBindingVisMask;
		vertexInputStateCIVisMask.vertexAttributeDescriptionCount = static_cast< uint32_t >( vertexInputAttributesVisMask.size() );
		vertexInputStateCIVisMask.pVertexAttributeDescriptions = vertexInputAttributesVisMask.data();

		// PIPELINE LAYOUT: PBR (push constants)
		const std::vector< VkDescriptorSetLayout > setLayouts = { descriptorSetLayouts.scene, descriptorSetLayouts.material, descriptorSetLayouts.node };
		VkPipelineLayoutCreateInfo pipelineLayoutCI {};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = static_cast< uint32_t >( setLayouts.size() );
		pipelineLayoutCI.pSetLayouts = setLayouts.data();
		VkPushConstantRange pushConstantRange {};
		pushConstantRange.size = sizeof( PushConstBlockMaterial );
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT( vkCreatePipelineLayout( m_SharedState.vkDevice, &pipelineLayoutCI, nullptr, &vkPipelineLayout ) );

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
		pipelineCI.renderPass = m_vecRenderPasses[ 0 ];
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

		// PIPELINE: vismask
		pipelineCI.layout = vkPipelineLayoutVisMask;
		pipelineCI.pVertexInputState = &vertexInputStateCIVisMask;

		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

#ifdef XR_USE_PLATFORM_ANDROID
		shaderStages = {
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/vismask.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/vismask.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#else
		shaderStages = {
			loadShader( m_SharedState.vkDevice, "shaders/vismask.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.vkDevice, "shaders/vismask.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#endif

		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.vismask ) );

		// cleanup
		for ( auto shaderStage : shaderStages )
		{
			vkDestroyShaderModule( m_SharedState.vkDevice, shaderStage.module, nullptr );
		}

		// ALL OTHER PIPELINES
		pipelineCI.layout = vkPipelineLayout;
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

		// PIPELINE: Skybox (background cube)
#ifdef XR_USE_PLATFORM_ANDROID
		shaderStages = {
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#else
		shaderStages = {
			loadShader( m_SharedState.vkDevice, "shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.vkDevice, "shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#endif

		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox ) );

		// cleanup
		for ( auto shaderStage : shaderStages )
		{
			vkDestroyShaderModule( m_SharedState.vkDevice, shaderStage.module, nullptr );
		}

		// PIPELINE: PBR
#ifdef XR_USE_PLATFORM_ANDROID
		shaderStages = {
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/pbr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ),
			loadShader( m_SharedState.androidAssetManager, m_SharedState.vkDevice, "shaders/pbr_khr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#else
		shaderStages = {
			loadShader( m_SharedState.vkDevice, "shaders/pbr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT ), loadShader( m_SharedState.vkDevice, "shaders/pbr_khr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT ) };
#endif

		rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
		depthStencilStateCI.depthWriteEnable = VK_TRUE;
		depthStencilStateCI.depthTestEnable = VK_TRUE;

		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr ) );

		// PIPELINE: PBR (double sided)
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbrDoubleSided ) );

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
		VK_CHECK_RESULT( vkCreateGraphicsPipelines( m_SharedState.vkDevice, m_SharedState.vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbrAlphaBlend ) );

		// cleanup
		for ( auto shaderStage : shaderStages )
		{
			vkDestroyShaderModule( m_SharedState.vkDevice, shaderStage.module, nullptr );
		}
	}

	uint32_t Render::AddRenderScene( std::string sFilename, XrVector3f scale )
	{
		uint32_t unSize = static_cast< uint32_t >( vecRenderScenes.size() );

		RenderScene *newRenderable = new RenderScene( sFilename );
		newRenderable->Reset( scale );
		vecRenderScenes.push_back( newRenderable );

		return unSize;
	}

	uint32_t Render::AddRenderSector( std::string sFilename, XrVector3f scale, XrSpace xrSpace )
	{
		uint32_t unSize = static_cast< uint32_t >( vecRenderSectors.size() );

		RenderSector *newRenderable = new RenderSector( sFilename );
		newRenderable->Reset( scale );
		newRenderable->xrSpace = xrSpace;
		vecRenderSectors.push_back( newRenderable );

		return unSize;
	}

	uint32_t Render::AddRenderModel( std::string sFilename, XrVector3f scale, XrSpace xrSpace )
	{
		uint32_t unSize = static_cast< uint32_t >( vecRenderModels.size() );

		RenderModel *newRenderable = new RenderModel( sFilename );
		newRenderable->Reset( scale );
		newRenderable->xrSpace = xrSpace;
		vecRenderModels.push_back( newRenderable );

		return unSize;
	}

	void Render::CreateVisMasks( uint32_t unNum )
	{
		m_vecVisMasks.resize( unNum );
		m_vecVisMaskBuffers.resize( unNum );
	}

	void Render::SetSkyboxVisibility( bool bNewVisibility )
	{
		m_bShowSkybox = bNewVisibility;

		if ( skybox != nullptr )
			skybox->bIsVisible = bNewVisibility;
	}

	bool Render::GetSkyboxVisibility()
	{
		if ( skybox != nullptr )
			return skybox->bIsVisible && m_bShowSkybox;

		return m_bShowSkybox;
	}

	void Render::RenderGltfScene( RenderSceneBase *renderable )
	{
		if ( !renderable->bIsVisible )
			return;

		const vkglTF::Model *gltfModel = &renderable->gltfModel;

		vkCmdBindVertexBuffers( m_vecFrameData[ 0 ].vkCommandBuffer, 0, 1, &gltfModel->vertices.buffer, vkDeviceSizeOffsets );

		if ( &gltfModel->indices.buffer != VK_NULL_HANDLE )
		{
			vkCmdBindIndexBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, gltfModel->indices.buffer, 0, VK_INDEX_TYPE_UINT32 );
		}

		vkBoundPipeline = VK_NULL_HANDLE;

		// Opaque primitives first
		for ( auto node : gltfModel->nodes )
		{
			RenderNode( renderable, node, 0, vkglTF::Material::ALPHAMODE_OPAQUE );
		}

		// Alpha masked primitives
		for ( auto node : gltfModel->nodes )
		{
			RenderNode( renderable, node, 0, vkglTF::Material::ALPHAMODE_MASK );
		}

		// Transparent primitives
		// TODO: Correct depth sorting
		for ( auto node : gltfModel->nodes )
		{
			RenderNode( renderable, node, 0, vkglTF::Material::ALPHAMODE_BLEND );
		}
	}

	void Render::RenderGltfScenes()
	{
		// Scenes
		for ( auto &renderable : vecRenderScenes )
		{
			RenderGltfScene( renderable );
		}

		// Sectors
		for ( auto &renderable : vecRenderSectors )
		{
			RenderGltfScene( renderable );
		}

		for ( auto &renderable : vecRenderModels )
		{
			RenderGltfScene( renderable );
		}
	}

	void Render::CreateRenderPass( int64_t nColorFormat, int64_t nDepthFormat, uint32_t nIndex /*= 0*/ )
	{
		assert( nColorFormat != VK_FORMAT_UNDEFINED );
		assert( nDepthFormat != VK_FORMAT_UNDEFINED );

		// define color and depth attachments
		std::array< VkAttachmentDescription, 2 > at = {};
		VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		// setup color attachment
		at[ colorRef.attachment ].format = ( VkFormat )nColorFormat;
		at[ colorRef.attachment ].samples = VK_SAMPLE_COUNT_1_BIT;
		at[ colorRef.attachment ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		at[ colorRef.attachment ].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[ colorRef.attachment ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[ colorRef.attachment ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[ colorRef.attachment ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		at[ colorRef.attachment ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// setup depth attachment
		at[ depthRef.attachment ].format = ( VkFormat )nDepthFormat;
		at[ depthRef.attachment ].samples = VK_SAMPLE_COUNT_1_BIT;
		at[ depthRef.attachment ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		at[ depthRef.attachment ].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[ depthRef.attachment ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[ depthRef.attachment ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[ depthRef.attachment ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		at[ depthRef.attachment ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Create subpass(es)
		VkSubpassDescription subpasses[ 1 ] = {};
		subpasses[ 0 ].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[ 0 ].pDepthStencilAttachment = &depthRef;
		subpasses[ 0 ].colorAttachmentCount = 1;
		subpasses[ 0 ].pColorAttachments = &colorRef;

		// finally, create render pass
		VkRenderPassCreateInfo rpInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rpInfo.attachmentCount = 2;
		rpInfo.pAttachments = at.data();
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = subpasses;

		vkCreateRenderPass( m_SharedState.vkDevice, &rpInfo, nullptr, &m_vecRenderPasses[ nIndex ] );
	}

	void Render::CreateRenderTargets( oxr::Session *pSession, VkRenderPass vkRenderPass )
	{
		if ( pSession->GetSwapchains().empty() )
			return;

		uint32_t nSwapchainSize = ( uint32_t )pSession->GetSwapchains().size();
		m_vec2RenderTargets.resize( nSwapchainSize );

		for ( uint32_t i = 0; i < nSwapchainSize; i++ )
		{
			const oxr::Swapchain *oxrSwapchain = &pSession->GetSwapchains()[ i ];
			uint32_t nImageNum = ( uint32_t )oxrSwapchain->vecColorTextures.size();
			m_vec2RenderTargets[ i ].resize( nImageNum );

			for ( uint32_t j = 0; j < nImageNum; j++ )
			{
				std::array< VkImageView, 2 > attachments {};
				uint32_t attachmentCount = 0;

				// Create color image view
				const XrSwapchainImageVulkan2KHR *vkColorSwapchainImage = &oxrSwapchain->vecColorTextures[ j ];
				m_vec2RenderTargets[ i ][ j ].vkColorImage = vkColorSwapchainImage->image;

				if ( vkColorSwapchainImage->image != VK_NULL_HANDLE )
				{
					VkImageViewCreateInfo colorViewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
					colorViewInfo.image = vkColorSwapchainImage->image;
					colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					colorViewInfo.format = oxrSwapchain->vulkanTextureFormats.vkColorTextureFormat;
					colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
					colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
					colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
					colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
					colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					colorViewInfo.subresourceRange.baseMipLevel = 0;
					colorViewInfo.subresourceRange.levelCount = 1;
					colorViewInfo.subresourceRange.baseArrayLayer = 0;
					colorViewInfo.subresourceRange.layerCount = 1;

					vkCreateImageView( m_SharedState.vkDevice, &colorViewInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkColorView );
					attachments[ attachmentCount++ ] = m_vec2RenderTargets[ i ][ j ].vkColorView;
				}

				// Create depth image view
				const XrSwapchainImageVulkan2KHR *vkDepthSwapchainImage = &oxrSwapchain->vecDepthTextures[ j ];
				m_vec2RenderTargets[ i ][ j ].vkDepthImage = vkDepthSwapchainImage->image;

				if ( vkDepthSwapchainImage != VK_NULL_HANDLE )
				{
					VkImageViewCreateInfo depthViewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
					depthViewInfo.image = vkDepthSwapchainImage->image;
					depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					depthViewInfo.format = oxrSwapchain->vulkanTextureFormats.vkDepthTextureFormat;
					depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
					depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
					depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
					depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
					depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					depthViewInfo.subresourceRange.baseMipLevel = 0;
					depthViewInfo.subresourceRange.levelCount = 1;
					depthViewInfo.subresourceRange.baseArrayLayer = 0;
					depthViewInfo.subresourceRange.layerCount = 1;

					vkCreateImageView( m_SharedState.vkDevice, &depthViewInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkDepthView );
					attachments[ attachmentCount++ ] = m_vec2RenderTargets[ i ][ j ].vkDepthView;
				}

				VkFramebufferCreateInfo fbInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				fbInfo.renderPass = vkRenderPass;
				fbInfo.attachmentCount = attachmentCount;
				fbInfo.pAttachments = attachments.data();
				fbInfo.width = oxrSwapchain->unWidth;
				fbInfo.height = oxrSwapchain->unHeight;
				fbInfo.layers = 1;
				vkCreateFramebuffer( m_SharedState.vkDevice, &fbInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkFrameBuffer );
			}
		}
	}

	void RenderScene::GetMatrix( XrMatrix4x4f *matrix ) { XrMatrix4x4f_CreateTranslationRotationScale( matrix, &currentPose.position, &currentPose.orientation, &currentScale ); }

	void RenderSector::GetMatrix( XrMatrix4x4f *matrix ) { XrMatrix4x4f_CreateTranslationRotationScale( matrix, &currentPose.position, &currentPose.orientation, &currentScale ); }

	void RenderModel::GetMatrix( XrMatrix4x4f *matrix )
	{
		if ( bApplyOffset )
		{
			XrMatrix4x4f matCurrent, matOffset;
			XrMatrix4x4f_CreateTranslationRotationScale( &matCurrent, &currentPose.position, &currentPose.orientation, &currentScale );
			XrMatrix4x4f_CreateTranslationRotationScale( &matOffset, &offsetPosition, &offsetRotation, &currentScale );

			XrMatrix4x4f_Multiply( matrix, &matCurrent, &matOffset );
		}
		else
		{
			XrMatrix4x4f_CreateTranslationRotationScale( matrix, &currentPose.position, &currentPose.orientation, &currentScale );
		}
	}

} // namespace xrvk