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
 * Portions of this code Copyright (c) 2019-2022, The Khronos Group Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "renderer_vulkan.h"
#include "openxr.h"
#include "openxr/xr_linear.h"

XrResult VulkanRenderer::Init( oxr::Provider *pProvider, oxr::AppInstanceInfo *pAppInstanceInfo )
{
	assert( pProvider );
	assert( pAppInstanceInfo );

	XrResult xrResult = XR_SUCCESS;
	m_pProvider = pProvider;

	// (1) Call *required* function GetVulkanGraphicsRequirements
	//       This returns the min-and-max vulkan api level required by the active runtime
	XrGraphicsRequirementsVulkan2KHR xrGraphicsRequirements;
	pProvider->GetVulkanGraphicsRequirements( &xrGraphicsRequirements );

	// (2) Define app info for vulkan
	VkApplicationInfo vkApplicationInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	vkApplicationInfo.pApplicationName = pAppInstanceInfo->sAppName.c_str();
	vkApplicationInfo.applicationVersion = pAppInstanceInfo->unAppVersion;
	vkApplicationInfo.pEngineName = pAppInstanceInfo->sEngineName.c_str();
	vkApplicationInfo.engineVersion = pAppInstanceInfo->unEngineVersion;
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
	xrResult = pProvider->CreateVulkanInstance( &xrVulkanInstanceCreateInfo, &m_vkInstance, &vkResult );
	if ( XR_UNQUALIFIED_SUCCESS( xrResult ) && vkResult == VK_SUCCESS )
	{
		oxr::Log( oxr::ELogLevel::LogInfo, m_sLogCategory, "Vulkan instance successfully created." );
	}
	else
	{
		oxr::LogError( LOG_CATEGORY_RENDER_VK, "Error creating vulkan instance with openxr result (%s) and vulkan result (%i)", oxr::XrEnumToString( xrResult ), ( int32_t )vkResult );
		return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
	}

	// (5) Get the vulkan physical device (gpu) used by the runtime
	xrResult = pProvider->GetVulkanGraphicsPhysicalDevice( &m_vkPhysicalDevice, m_vkInstance );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		oxr::LogError( LOG_CATEGORY_RENDER_VK, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", oxr::XrEnumToString( xrResult ) );
		return xrResult;
	}

	// (6) Create vulkan device
	VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	float fQueuePriorities = 0;
	vkDeviceQueueCreateInfo.queueCount = 1;
	vkDeviceQueueCreateInfo.pQueuePriorities = &fQueuePriorities;

	uint32_t unQueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, nullptr );
	std::vector< VkQueueFamilyProperties > vecQueueFamilyProps( unQueueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( m_vkPhysicalDevice, &unQueueFamilyCount, vecQueueFamilyProps.data() );

	for ( uint32_t i = 0; i < unQueueFamilyCount; ++i )
	{
		if ( ( vecQueueFamilyProps[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0u )
		{
			m_vkQueueFamilyIndex = vkDeviceQueueCreateInfo.queueFamilyIndex = i;
			break;
		}
	}

	std::vector< const char * > vkDeviceExtensions;
	VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures {};

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
	vkDeviceCreateInfo.pEnabledFeatures = &vkPhysicalDeviceFeatures;

	XrVulkanDeviceCreateInfoKHR xrVulkanDeviceCreateInfo { XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
	xrVulkanDeviceCreateInfo.systemId = pProvider->Instance()->xrSystemId;
	xrVulkanDeviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
	xrVulkanDeviceCreateInfo.vulkanCreateInfo = &vkDeviceCreateInfo;
	xrVulkanDeviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
	xrVulkanDeviceCreateInfo.vulkanAllocator = nullptr;

	xrResult = pProvider->CreateVulkanDevice( &xrVulkanDeviceCreateInfo, &m_vkPhysicalDevice, &m_vkInstance, &m_vkDevice, &vkResult );
	if ( XR_UNQUALIFIED_SUCCESS( xrResult ) && vkResult == VK_SUCCESS )
	{
		oxr::Log( oxr::ELogLevel::LogInfo, m_sLogCategory, "Vulkan device successfully created." );
	}
	else
	{
		oxr::LogError( m_sLogCategory, "Error creating vulkan device with openxr result (%s) and vulkan result (%i)", oxr::XrEnumToString( xrResult ), ( int32_t )vkResult );
		return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
	}

	// (7) Create graphics binding that we will use to create an openxr session
	vkGetDeviceQueue( m_vkDevice, vkDeviceQueueCreateInfo.queueFamilyIndex, 0, &m_vkQueue );

	m_xrGraphicsBinding.instance = m_vkInstance;
	m_xrGraphicsBinding.physicalDevice = m_vkPhysicalDevice;
	m_xrGraphicsBinding.device = m_vkDevice;
	m_xrGraphicsBinding.queueFamilyIndex = vkDeviceQueueCreateInfo.queueFamilyIndex;
	m_xrGraphicsBinding.queueIndex = 0;

	return XR_SUCCESS;
}

void VulkanRenderer::Cleanup()
{
	// Destroy command pool(s)
	for ( auto frameData : m_vecFrameData )
	{
		if ( frameData.vkCommandPool != VK_NULL_HANDLE )
		{
			vkDestroyCommandPool( m_vkDevice, frameData.vkCommandPool, nullptr );
		}
	}

	// Destroy pipeline(s)
	for ( auto pipeline : m_vecPipelines )
	{
		if ( pipeline != VK_NULL_HANDLE )
		{
			vkDestroyPipeline( m_vkDevice, pipeline, nullptr );
		}
	}

	// Destroy pipeline layout(s)
	for ( auto pipelineLayout : m_vecPipelinelayouts )
	{
		if ( pipelineLayout != VK_NULL_HANDLE )
		{
			vkDestroyPipelineLayout( m_vkDevice, pipelineLayout, nullptr );
		}
	}

	// Destroy render pass(es)
	for ( auto renderPass : m_vecRenderPasses )
	{
		if ( renderPass != VK_NULL_HANDLE )
		{
			vkDestroyRenderPass( m_vkDevice, renderPass, nullptr );
		}
	}

	// Destroy vulkan instance
	if ( m_vkInstance != VK_NULL_HANDLE )
	{
		vkDestroyInstance( m_vkInstance, nullptr );
		m_vkInstance = VK_NULL_HANDLE;
	}
}

void VulkanRenderer::InitVulkanResources( oxr::Session *pSession, VertexBuffer *pVertexBuffer, int64_t nColorFormat, int64_t nDepthFormat, VkExtent2D vkExtent )
{
	// Create render pass(es)
	CreateRenderPass( nColorFormat, nDepthFormat );

	// Create render target(s) per image in the swapchain including frame buffers
	CreateRenderTargets( pSession, m_vecRenderPasses[ 0 ] );

	// Create pipeline layout(s)
	VkPushConstantRange vkPCR = {};
	vkPCR.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vkPCR.offset = 0;
	vkPCR.size = 4 * 4 * sizeof( float );
	CreatePipelineLayout( vkPCR );

	// Initialize vertex buffer
	pVertexBuffer->Init( m_vkPhysicalDevice, m_vkDevice );
	m_pVertexBuffer = pVertexBuffer;

	// Create graphics pipeline(s)
	CreateGraphicsPipeline_SimpleAsset( vkExtent );

	// Create vulkan command buffers
	m_vecFrameData.resize( k_unCommandBufferNum, {} );
	for ( uint32_t i = 0; i < k_unCommandBufferNum; i++ )
	{
		// Create a command pool to allocate our command buffer from
		VkCommandPoolCreateInfo cmdPoolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPoolInfo.queueFamilyIndex = m_vkQueueFamilyIndex;
		vkCreateCommandPool( m_vkDevice, &cmdPoolInfo, nullptr, &m_vecFrameData[ 0 ].vkCommandPool );

		// Create the command buffer from the command pool
		VkCommandBufferAllocateInfo cmd { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmd.commandPool = m_vecFrameData[ 0 ].vkCommandPool;
		cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd.commandBufferCount = 1;
		vkAllocateCommandBuffers( m_vkDevice, &cmd, &m_vecFrameData[ 0 ].vkCommandBuffer );

		VkFenceCreateInfo fenceInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		vkCreateFence( m_vkDevice, &fenceInfo, nullptr, &m_vecFrameData[ 0 ].vkCommandFence );
	}
}

void VulkanRenderer::CreateRenderPass( int64_t nColorFormat, int64_t nDepthFormat, uint32_t nIndex )
{
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	std::array< VkAttachmentDescription, 2 > at = {};

	VkRenderPassCreateInfo rpInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpInfo.attachmentCount = 0;
	rpInfo.pAttachments = at.data();
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;

	if ( nColorFormat != VK_FORMAT_UNDEFINED )
	{
		colorRef.attachment = rpInfo.attachmentCount++;

		at[ colorRef.attachment ].format = ( VkFormat )nColorFormat;
		at[ colorRef.attachment ].samples = VK_SAMPLE_COUNT_1_BIT;
		at[ colorRef.attachment ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		at[ colorRef.attachment ].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[ colorRef.attachment ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[ colorRef.attachment ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[ colorRef.attachment ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		at[ colorRef.attachment ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
	}

	if ( nDepthFormat != VK_FORMAT_UNDEFINED )
	{
		depthRef.attachment = rpInfo.attachmentCount++;

		at[ depthRef.attachment ].format = ( VkFormat )nDepthFormat;
		at[ depthRef.attachment ].samples = VK_SAMPLE_COUNT_1_BIT;
		at[ depthRef.attachment ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		at[ depthRef.attachment ].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		at[ depthRef.attachment ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		at[ depthRef.attachment ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		at[ depthRef.attachment ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		at[ depthRef.attachment ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		subpass.pDepthStencilAttachment = &depthRef;
	}

	vkCreateRenderPass( m_vkDevice, &rpInfo, nullptr, &m_vecRenderPasses[ nIndex ] );
}

void VulkanRenderer::CreateRenderTargets( oxr::Session *pSession, VkRenderPass vkRenderPass )
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

				vkCreateImageView( m_vkDevice, &colorViewInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkColorView );
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

				vkCreateImageView( m_vkDevice, &depthViewInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkDepthView );
				attachments[ attachmentCount++ ] = m_vec2RenderTargets[ i ][ j ].vkDepthView;
			}

			VkFramebufferCreateInfo fbInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			fbInfo.renderPass = vkRenderPass;
			fbInfo.attachmentCount = attachmentCount;
			fbInfo.pAttachments = attachments.data();
			fbInfo.width = oxrSwapchain->unWidth;
			fbInfo.height = oxrSwapchain->unHeight;
			fbInfo.layers = 1;
			vkCreateFramebuffer( m_vkDevice, &fbInfo, nullptr, &m_vec2RenderTargets[ i ][ j ].vkFrameBuffer );
		}
	}
}

void VulkanRenderer::CreatePipelineLayout( VkPushConstantRange vkPCR, uint32_t nIndex )
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &vkPCR;
	vkCreatePipelineLayout( m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vecPipelinelayouts[ nIndex ] );
}

void VulkanRenderer::CreateGraphicsPipeline_SimpleAsset( VkExtent2D vkExtent, uint32_t nLayoutIndex, uint32_t nRenderPassIndex, uint32_t nIndex )
{
	// (1) Define programmable stages
	auto vertShader = CreateShaderModule( "shaders/vert.spv" );
	auto fragShader = CreateShaderModule( "shaders/frag.spv" );

	std::string sFunctionEntrypoint = "main";
	auto vertShaderStage = CreateShaderStage( VK_SHADER_STAGE_VERTEX_BIT, &vertShader, sFunctionEntrypoint );
	auto fragShaderStage = CreateShaderStage( VK_SHADER_STAGE_FRAGMENT_BIT, &fragShader, sFunctionEntrypoint );

	std::vector< VkPipelineShaderStageCreateInfo > shaderStages = { vertShaderStage, fragShaderStage };

	// (2) Define fixed Function stages
	uint32_t numCubeIdicies = sizeof( Geometry::c_cubeIndices ) / sizeof( Geometry::c_cubeIndices[ 0 ] );
	uint32_t numCubeVerticies = sizeof( Geometry::c_cubeVertices ) / sizeof( Geometry::c_cubeVertices[ 0 ] );

	m_memAllocator.Init( m_vkPhysicalDevice, m_vkDevice );

	m_pVertexBuffer->Create( numCubeIdicies, numCubeVerticies );
	m_pVertexBuffer->Init(
		m_vkDevice, &m_memAllocator, { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Geometry::Vertex, Position ) }, { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Geometry::Vertex, Color ) } } );
	m_pVertexBuffer->UpdateIndices( Geometry::c_cubeIndices, numCubeIdicies, 0 );
	m_pVertexBuffer->UpdateVertices( Geometry::c_cubeVertices, numCubeVerticies, 0 );

	VkPipelineVertexInputStateCreateInfo vertexInputInfo { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &m_pVertexBuffer->bindDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = ( uint32_t )m_pVertexBuffer->attrDesc.size();
	vertexInputInfo.pVertexAttributeDescriptions = m_pVertexBuffer->attrDesc.data();

	std::vector< VkDynamicState > vecDynamicStates; // = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = static_cast< uint32_t >( vecDynamicStates.size() );
	dynamicState.pDynamicStates = vecDynamicStates.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineRasterizationStateCreateInfo rasterizer { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0;
	rasterizer.depthBiasClamp = 0;
	rasterizer.depthBiasSlopeFactor = 0;
	rasterizer.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment {};
	colorBlendAttachment.blendEnable = 0;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlending { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_NO_OP;
	colorBlending.blendConstants[ 0 ] = 1.0f;
	colorBlending.blendConstants[ 1 ] = 1.0f;
	colorBlending.blendConstants[ 2 ] = 1.0f;
	colorBlending.blendConstants[ 3 ] = 1.0f;

	VkRect2D scissor = { { 0, 0 }, vkExtent };

	// Will invert y after projection
	VkViewport viewport = { 0.0f, 0.0f, ( float )vkExtent.width, ( float )vkExtent.height, 0.0f, 1.0f };

	VkPipelineViewportStateCreateInfo viewportState { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineMultisampleStateCreateInfo multisampling { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkGraphicsPipelineCreateInfo pipeInfo { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeInfo.stageCount = ( uint32_t )shaderStages.size();
	pipeInfo.pStages = shaderStages.data();
	pipeInfo.pVertexInputState = &vertexInputInfo;
	pipeInfo.pInputAssemblyState = &inputAssembly;
	pipeInfo.pTessellationState = nullptr;
	pipeInfo.pViewportState = &viewportState;
	pipeInfo.pRasterizationState = &rasterizer;
	pipeInfo.pMultisampleState = &multisampling;
	pipeInfo.pDepthStencilState = nullptr;
	pipeInfo.pColorBlendState = &colorBlending;

	if ( dynamicState.dynamicStateCount > 0 )
	{
		pipeInfo.pDynamicState = &dynamicState;
	}

	pipeInfo.layout = m_vecPipelinelayouts[ nLayoutIndex ];
	pipeInfo.renderPass = m_vecRenderPasses[ nRenderPassIndex ];
	pipeInfo.subpass = 0;

	// (3) Finally, create the graphics pipeline - whew!
	VkResult vkResult = vkCreateGraphicsPipelines( m_vkDevice, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_vecPipelines[ nIndex ] );

	// (4) Cleanup
	vkDestroyShaderModule( m_vkDevice, vertShader, nullptr );
	vkDestroyShaderModule( m_vkDevice, fragShader, nullptr );
}

void VulkanRenderer::PreRender(
	oxr::Session *pSession,
	std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews,
	XrFrameState *pFrameState,
	uint32_t unSwapchainIndex,
	uint32_t unImageIndex )
{
	// Set command buffer to recording
	VkCommandBufferBeginInfo cmdBeginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, &cmdBeginInfo );

	static std::array< VkClearValue, 2 > clearValues;
	clearValues[ 0 ].color.float32[ 0 ] = m_colorClear[ 0 ];
	clearValues[ 0 ].color.float32[ 1 ] = m_colorClear[ 1 ];
	clearValues[ 0 ].color.float32[ 2 ] = m_colorClear[ 2 ];
	clearValues[ 0 ].color.float32[ 3 ] = m_colorClear[ 3 ];

	clearValues[ 0 ].color.int32[ 0 ] = 0;
	clearValues[ 0 ].color.int32[ 1 ] = 0;
	clearValues[ 0 ].color.int32[ 2 ] = 1;
	clearValues[ 0 ].color.int32[ 3 ] = 1;

	clearValues[ 0 ].color.uint32[ 0 ] = 0;
	clearValues[ 0 ].color.uint32[ 1 ] = 0;
	clearValues[ 0 ].color.uint32[ 2 ] = 1;
	clearValues[ 0 ].color.uint32[ 3 ] = 1;

	clearValues[ 1 ].depthStencil.depth = 1.0f;
	clearValues[ 1 ].depthStencil.stencil = 0;

	m_ClearColor.float32[ 0 ] = 0.2f; // m_colorClear[ 0 ];
	m_ClearColor.float32[ 1 ] = 0.2f; // m_colorClear[ 1 ];
	m_ClearColor.float32[ 2 ] = 1.f;  // m_colorClear[ 2 ];
	m_ClearColor.float32[ 3 ] = .5f;  // m_colorClear[ 3 ];

	m_ClearColor.int32[ 0 ] = 0;
	m_ClearColor.int32[ 1 ] = 0;
	m_ClearColor.int32[ 2 ] = 100;
	m_ClearColor.int32[ 3 ] = 100;

	m_ClearColor.uint32[ 0 ] = 0;
	m_ClearColor.uint32[ 1 ] = 0;
	m_ClearColor.uint32[ 2 ] = 100;
	m_ClearColor.uint32[ 3 ] = 100;

	VkRenderPassBeginInfo renderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBeginInfo.clearValueCount = ( uint32_t )clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();

	// Get swapchain
	auto *pSwapchain = &pSession->GetSwapchains()[ unSwapchainIndex ];

	// Bind render target
	renderPassBeginInfo.renderPass = m_vecRenderPasses[ 0 ];
	renderPassBeginInfo.framebuffer = m_vec2RenderTargets[ unSwapchainIndex ][ unImageIndex ].vkFrameBuffer;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };

	VkExtent2D vkExtent;
	vkExtent.width = pSwapchain->unWidth;
	vkExtent.height = pSwapchain->unHeight;
	renderPassBeginInfo.renderArea.extent = vkExtent;

	vkCmdBeginRenderPass( m_vecFrameData[ 0 ].vkCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
	vkCmdBindPipeline( m_vecFrameData[ 0 ].vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vecPipelines[ 0 ] );

	// Bind index and vertex buffers
	vkCmdBindIndexBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, m_pVertexBuffer->idxBuf, 0, VK_INDEX_TYPE_UINT16 );
	m_offset = 0;
	vkCmdBindVertexBuffers( m_vecFrameData[ 0 ].vkCommandBuffer, 0, 1, &m_pVertexBuffer->vtxBuf, &m_offset );

	// Compute the view-projection transform.
	// Note all matrices (including OpenXR's) are column-major, right-handed.
	const auto &pose = vecFrameLayerProjectionViews[ unSwapchainIndex ].pose;
	XrMatrix4x4f proj;
	XrMatrix4x4f_CreateProjectionFov( &proj, GRAPHICS_VULKAN, vecFrameLayerProjectionViews[ unSwapchainIndex ].fov, 0.05f, 100.0f );
	XrMatrix4x4f toView;
	XrVector3f scale { 1.f, 1.f, 1.f };
	XrMatrix4x4f_CreateTranslationRotationScale( &toView, &pose.position, &pose.orientation, &scale );
	XrMatrix4x4f view;
	XrMatrix4x4f_InvertRigidBody( &view, &toView );
	XrMatrix4x4f vp;
	XrMatrix4x4f_Multiply( &vp, &proj, &view );

	// Find current world origin
	XrSpaceLocation xrSpaceLocation { XR_TYPE_SPACE_LOCATION };
	pSession->LocateAppSpace( pFrameState->predictedDisplayTime, &xrSpaceLocation );

	// Add z offset (move forward by 1m and up by 1.5m)
	// We can also create a new reference space so we don't have to apply manual offsets (see next demo)
	xrSpaceLocation.pose.position.z -= 1.0f;
	xrSpaceLocation.pose.position.y += 1.5f;

	// Render cubes (just one for now)
	std::vector< Mesh > vecMeshes;
	vecMeshes.push_back( Mesh { xrSpaceLocation.pose, { 0.25f, 0.25f, 0.25f } } );
	for ( const Mesh &mesh : vecMeshes )
	{
		// Compute the model-view-projection transform and push it.
		XrMatrix4x4f model;
		XrMatrix4x4f_CreateTranslationRotationScale( &model, &mesh.Pose.position, &mesh.Pose.orientation, &mesh.Scale );
		XrMatrix4x4f mvp;
		XrMatrix4x4f_Multiply( &mvp, &vp, &model );
		vkCmdPushConstants( m_vecFrameData[ 0 ].vkCommandBuffer, m_vecPipelinelayouts[ 0 ], VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( mvp.m ), &mvp.m[ 0 ] );

		// Draw the cube.
		vkCmdDrawIndexed( m_vecFrameData[ 0 ].vkCommandBuffer, m_pVertexBuffer->count.idx, 1, 0, 0, 0 );
	}

	// End render pass
	vkCmdEndRenderPass( m_vecFrameData[ 0 ].vkCommandBuffer );

	// Close command buffer recording
	vkEndCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer );
}

void VulkanRenderer::StartRender()
{
	// Execute command buffer (requires exclusive access to vkQueue)
	VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vecFrameData[ 0 ].vkCommandBuffer;
	vkQueueSubmit( m_vkQueue, 1, &submitInfo, m_vecFrameData[ 0 ].vkCommandFence );

	const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
	vkWaitForFences( m_vkDevice, 1, &m_vecFrameData[ 0 ].vkCommandFence, VK_TRUE, timeoutNs );
	vkResetFences( m_vkDevice, 1, &m_vecFrameData[ 0 ].vkCommandFence );
	vkResetCommandBuffer( m_vecFrameData[ 0 ].vkCommandBuffer, 0 );
}

VkShaderModule VulkanRenderer::CreateShaderModule( const std::string &sFilename )
{
	auto spirvShaderCode = readFile( sFilename );

	// Create shader module
	VkShaderModuleCreateInfo createInfo {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirvShaderCode.size();
	createInfo.pCode = reinterpret_cast< const uint32_t * >( spirvShaderCode.data() );

	VkShaderModule shaderModule;
	if ( vkCreateShaderModule( m_vkDevice, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS )
	{
		throw std::runtime_error( "failed to create shader module!" );
	}

	return shaderModule;
}

VkPipelineShaderStageCreateInfo VulkanRenderer::CreateShaderStage( VkShaderStageFlagBits flagShaderStage, VkShaderModule *pShaderModule, const std::string &sEntrypoint )
{

	// Create shader stage
	VkPipelineShaderStageCreateInfo shaderStageInfo {};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = flagShaderStage;
	shaderStageInfo.module = *pShaderModule;
	shaderStageInfo.pName = sEntrypoint.c_str();

	return shaderStageInfo;
}
