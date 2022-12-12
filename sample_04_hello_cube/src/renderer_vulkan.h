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

#pragma once
#include <openxr_provider.h>
#define LOG_CATEGORY_RENDER_VK "VulkanRender"

static const uint32_t k_unCommandBufferNum = 2; // (double buffer)
typedef std::array< float, 4 > Colorf;

namespace Geometry
{

	struct Vertex
	{
		XrVector3f Position;
		XrVector3f Color;
	};

	constexpr XrVector3f Red { 1, 0, 0 };
	constexpr XrVector3f DarkRed { 0.25f, 0, 0 };
	constexpr XrVector3f Green { 0, 1, 0 };
	constexpr XrVector3f DarkGreen { 0, 0.25f, 0 };
	constexpr XrVector3f Blue { 0, 0, 1 };
	constexpr XrVector3f DarkBlue { 0, 0, 0.25f };

	// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
	constexpr XrVector3f LBB { -0.5f, -0.5f, -0.5f };
	constexpr XrVector3f LBF { -0.5f, -0.5f, 0.5f };
	constexpr XrVector3f LTB { -0.5f, 0.5f, -0.5f };
	constexpr XrVector3f LTF { -0.5f, 0.5f, 0.5f };
	constexpr XrVector3f RBB { 0.5f, -0.5f, -0.5f };
	constexpr XrVector3f RBF { 0.5f, -0.5f, 0.5f };
	constexpr XrVector3f RTB { 0.5f, 0.5f, -0.5f };
	constexpr XrVector3f RTF { 0.5f, 0.5f, 0.5f };

#define CUBE_SIDE( V1, V2, V3, V4, V5, V6, COLOR ) { V1, COLOR }, { V2, COLOR }, { V3, COLOR }, { V4, COLOR }, { V5, COLOR }, { V6, COLOR },

	constexpr Vertex c_cubeVertices[] = {
		CUBE_SIDE( LTB, LBF, LBB, LTB, LTF, LBF, DarkRed )	 // -X
		CUBE_SIDE( RTB, RBB, RBF, RTB, RBF, RTF, Red )		 // +X
		CUBE_SIDE( LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen ) // -Y
		CUBE_SIDE( LTB, RTB, RTF, LTB, RTF, LTF, Green )	 // +Y
		CUBE_SIDE( LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue )	 // -Z
		CUBE_SIDE( LBF, LTF, RTF, LBF, RTF, RBF, Blue )		 // +Z
	};

	// Winding order is clockwise. Each side uses a different color.
	constexpr unsigned short c_cubeIndices[] = {
		0,	1,	2,	3,	4,	5,	// -X
		6,	7,	8,	9,	10, 11, // +X
		12, 13, 14, 15, 16, 17, // -Y
		18, 19, 20, 21, 22, 23, // +Y
		24, 25, 26, 27, 28, 29, // -Z
		30, 31, 32, 33, 34, 35, // +Z
	};

} // namespace Geometry

struct RenderTarget
{
	VkImage vkColorImage = VK_NULL_HANDLE;
	VkImage vkDepthImage = VK_NULL_HANDLE;
	VkImageView vkColorView = VK_NULL_HANDLE;
	VkImageView vkDepthView = VK_NULL_HANDLE;
	VkFramebuffer vkFrameBuffer = VK_NULL_HANDLE;
};

struct FrameData
{
	VkCommandPool vkCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;
	VkFence vkCommandFence = VK_NULL_HANDLE;
};

struct MemoryAllocator
{
	void Init( VkPhysicalDevice physicalDevice, VkDevice device )
	{
		m_vkDevice = device;
		vkGetPhysicalDeviceMemoryProperties( physicalDevice, &m_memProps );
	}

	static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	void Allocate( VkMemoryRequirements const &memReqs, VkDeviceMemory *mem, VkFlags flags = defaultFlags, const void *pNext = nullptr ) const
	{
		// Search memtypes to find first index with those properties
		for ( uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i )
		{
			if ( ( memReqs.memoryTypeBits & ( 1 << i ) ) != 0u )
			{
				// Type is available, does it match user properties?
				if ( ( m_memProps.memoryTypes[ i ].propertyFlags & flags ) == flags )
				{
					VkMemoryAllocateInfo memAlloc { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, pNext };
					memAlloc.allocationSize = memReqs.size;
					memAlloc.memoryTypeIndex = i;
					vkAllocateMemory( m_vkDevice, &memAlloc, nullptr, mem );
					return;
				}
			}
		}
		// assert( "Memory format not supported" );
	}

  private:
	VkDevice m_vkDevice { VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties m_memProps {};
};

struct VertexBuffer
{
	VkBuffer idxBuf { VK_NULL_HANDLE };
	VkDeviceMemory idxMem { VK_NULL_HANDLE };
	VkBuffer vtxBuf { VK_NULL_HANDLE };
	VkDeviceMemory vtxMem { VK_NULL_HANDLE };
	VkVertexInputBindingDescription bindDesc {};
	std::vector< VkVertexInputAttributeDescription > attrDesc {};
	struct
	{
		uint32_t idx;
		uint32_t vtx;
	} count = { 0, 0 };

	void Init( VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice )
	{
		m_vkPhysicalDevice = vkPhysicalDevice;
		m_vkDevice = vkDevice;
	}

	// Destructor
	~VertexBuffer()
	{
		if ( m_vkDevice != VK_NULL_HANDLE )
		{
			if ( idxBuf != VK_NULL_HANDLE )
			{
				vkDestroyBuffer( m_vkDevice, idxBuf, nullptr );
			}
			if ( idxMem != VK_NULL_HANDLE )
			{
				vkFreeMemory( m_vkDevice, idxMem, nullptr );
			}
			if ( vtxBuf != VK_NULL_HANDLE )
			{
				vkDestroyBuffer( m_vkDevice, vtxBuf, nullptr );
			}
			if ( vtxMem != VK_NULL_HANDLE )
			{
				vkFreeMemory( m_vkDevice, vtxMem, nullptr );
			}
		}
		idxBuf = VK_NULL_HANDLE;
		idxMem = VK_NULL_HANDLE;
		vtxBuf = VK_NULL_HANDLE;
		vtxMem = VK_NULL_HANDLE;
		bindDesc = {};
		attrDesc.clear();
		count = { 0, 0 };
		m_vkDevice = VK_NULL_HANDLE;
	}

	bool Create( uint32_t idxCount, uint32_t vtxCount )
	{
		VkBufferCreateInfo bufInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bufInfo.size = sizeof( uint16_t ) * idxCount;
		vkCreateBuffer( m_vkDevice, &bufInfo, nullptr, &idxBuf );
		AllocateBufferMemory( idxBuf, &idxMem );
		vkBindBufferMemory( m_vkDevice, idxBuf, idxMem, 0 );

		bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufInfo.size = sizeof( Geometry::Vertex ) * vtxCount;
		vkCreateBuffer( m_vkDevice, &bufInfo, nullptr, &vtxBuf );
		AllocateBufferMemory( vtxBuf, &vtxMem );
		vkBindBufferMemory( m_vkDevice, vtxBuf, vtxMem, 0 );

		bindDesc.binding = 0;
		bindDesc.stride = sizeof( Geometry::Vertex );
		bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		count = { idxCount, vtxCount };

		return true;
	}

	void Init( VkDevice device, const MemoryAllocator *memAllocator, const std::vector< VkVertexInputAttributeDescription > &attr )
	{
		m_vkDevice = device;
		m_pMemAllocator = memAllocator;
		attrDesc = attr;
	}

	void UpdateIndices( const uint16_t *data, uint32_t elements, uint32_t offset = 0 )
	{
		uint16_t *map = nullptr;
		vkMapMemory( m_vkDevice, idxMem, sizeof( map[ 0 ] ) * offset, sizeof( map[ 0 ] ) * elements, 0, ( void ** )&map );
		for ( size_t i = 0; i < elements; ++i )
		{
			map[ i ] = data[ i ];
		}
		vkUnmapMemory( m_vkDevice, idxMem );
	}

	void UpdateVertices( const Geometry::Vertex *data, uint32_t elements, uint32_t offset = 0 )
	{
		Geometry::Vertex *map = nullptr;
		vkMapMemory( m_vkDevice, vtxMem, sizeof( map[ 0 ] ) * offset, sizeof( map[ 0 ] ) * elements, 0, ( void ** )&map );
		for ( size_t i = 0; i < elements; ++i )
		{
			map[ i ] = data[ i ];
		}
		vkUnmapMemory( m_vkDevice, vtxMem );
	}

  private:
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;
	const MemoryAllocator *m_pMemAllocator = nullptr;

	void AllocateBufferMemory( VkBuffer buf, VkDeviceMemory *mem ) const
	{
		static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( m_vkPhysicalDevice, &memProperties );

		VkMemoryRequirements memReq = {};
		vkGetBufferMemoryRequirements( m_vkDevice, buf, &memReq );

		// Search memtypes to find first index with those properties
		for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
		{
			if ( ( memReq.memoryTypeBits & ( 1 << i ) ) != 0u )
			{
				// Type is available, does it match user properties?
				if ( ( memProperties.memoryTypes[ i ].propertyFlags & defaultFlags ) == defaultFlags )
				{
					VkMemoryAllocateInfo memAlloc { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
					memAlloc.allocationSize = memReq.size;
					memAlloc.memoryTypeIndex = i;
					vkAllocateMemory( m_vkDevice, &memAlloc, nullptr, mem );
					return;
				}
			}
		}
	}
};

struct Mesh
{
	XrPosef Pose;
	XrVector3f Scale;
};

class VulkanRenderer
{
  public:
	VulkanRenderer( Colorf colorClear )
		: m_colorClear( colorClear ) {};

	~VulkanRenderer() { Cleanup(); };

	XrGraphicsBindingVulkan2KHR *GetVulkanGraphicsBinding() { return &m_xrGraphicsBinding; }

	XrResult Init( oxr::Provider *pProvider, oxr::AppInstanceInfo *pAppInstanceInfo );

	void Cleanup();

	void InitVulkanResources( oxr::Session *pSession, VertexBuffer *pVertexBuffer, int64_t nColorFormat, int64_t nDepthFormat, VkExtent2D vkExtent );

	void CreateRenderPass( int64_t nColorFormat, int64_t nDepthFormat, uint32_t nIndex = 0 );

	void CreateRenderTargets( oxr::Session *pSession, VkRenderPass vkRenderPass );

	void CreatePipelineLayout( VkPushConstantRange vkPCR, uint32_t nIndex = 0 );

	void CreateGraphicsPipeline_SimpleAsset( VkExtent2D vkExtent, uint32_t nLayoutIndex = 0, uint32_t nRenderPassIndex = 0, uint32_t nIndex = 0 );

	void
		PreRender( oxr::Session *pSession, std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews, XrFrameState *pFrameState, uint32_t unSwapchainIndex, uint32_t unImageIndex );

	void StartRender();

	VkShaderModule CreateShaderModule( const std::string &sFilename );

	VkPipelineShaderStageCreateInfo CreateShaderStage( VkShaderStageFlagBits flagShaderStage, VkShaderModule *pShaderModule, const std::string &sEntrypoint );

	// Vulkan debug logging
	static VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData )
	{
		if ( messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
		{
			oxr::Log( oxr::ELogLevel::LogDebug, "[Vulkan Validation] %s", pCallbackData->pMessage );
		}

		return VK_FALSE;
	}

	// Shader loading (bytecode only - spirv)
#ifdef XR_USE_PLATFORM_ANDROID
	std::vector< char > readFile( const std::string &filename )
	{
		AAsset *file = AAssetManager_open( m_pProvider->Instance()->androidActivity->assetManager, filename.c_str(), AASSET_MODE_BUFFER );
		size_t fileLength = AAsset_getLength( file );
		char *fileContent = new char[ fileLength ];
		AAsset_read( file, fileContent, fileLength );
		AAsset_close( file );

		std::vector< char > vec( fileContent, fileContent + fileLength );
		return vec;
	}
#else
	static std::vector< char > readFile( const std::string &filename )
	{
		std::ifstream file( filename, std::ios::ate | std::ios::binary );

		if ( !file.is_open() )
		{
			std::filesystem::path cwd = std::filesystem::current_path();
			Log( oxr::ELogLevel::LogError, "VkRenderer", "Unable to read file: %s (%s)", filename.c_str(), cwd.generic_string().c_str() );
			throw std::runtime_error( "failed to open file!" );
		}

		size_t fileSize = ( size_t )file.tellg();
		std::vector< char > buffer( fileSize );

		file.seekg( 0 );
		file.read( buffer.data(), fileSize );

		file.close();
		return buffer;
	}
#endif

	void setImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask )
	{
		// Create an image barrier object
		VkImageMemoryBarrier imageMemoryBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		// Source layouts (old)
		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch ( oldImageLayout )
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Target layouts (new)
		// Destination access mask controls the dependency for the new image layout
		switch ( newImageLayout )
		{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if ( imageMemoryBarrier.srcAccessMask == 0 )
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier( cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
	}

  private:
	const std::vector< const char * > m_vecValidationLayers;	 // = {  "VK_LAYER_KHRONOS_validation" };
	const std::vector< const char * > m_vecValidationExtensions; // = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

	std::string m_sLogCategory = LOG_CATEGORY_RENDER_VK;
	oxr::Provider *m_pProvider = nullptr;
	Colorf m_colorClear;
	VkClearColorValue m_ClearColor;

	XrGraphicsBindingVulkan2KHR m_xrGraphicsBinding { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };
	uint32_t m_vkQueueFamilyIndex = 0;
	VkQueue m_vkQueue = VK_NULL_HANDLE;
	VkInstance m_vkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_vkDevice = VK_NULL_HANDLE;

	std::vector< std::vector< RenderTarget > > m_vec2RenderTargets;

	std::vector< FrameData > m_vecFrameData {};
	std::vector< VkRenderPass > m_vecRenderPasses { VK_NULL_HANDLE };
	std::vector< VkPipelineLayout > m_vecPipelinelayouts { VK_NULL_HANDLE };
	std::vector< VkPipeline > m_vecPipelines { VK_NULL_HANDLE };

	MemoryAllocator m_memAllocator {};
	VkDeviceSize m_offset = 0;
	VertexBuffer *m_pVertexBuffer = nullptr;
};