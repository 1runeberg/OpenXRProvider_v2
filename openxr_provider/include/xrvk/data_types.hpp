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

#pragma once
#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

// platform specific
#ifdef _WIN32
	#define VK_USE_PLATFORM_WIN32_KHR 1
	#include <windows.h>
#endif

// vulkan
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <vulkan/vulkan.h>

// TODO: internal openxr calls to decouple this from provider
// maybe callbacks???
#include <openxr_provider.h>

// openxr
#include <openxr.h>
#include <openxr_platform.h>
#include <openxr_platform_defines.h>
#include <openxr_reflection.h>

// maths
#include "openxr/xr_linear.h"

// glm, gli
#include <gli/gli.hpp>
#include <glm/glm.hpp>

// internal
#include "log.hpp"

// vks
#include "vulkanpbr/VulkanTexture.hpp"
#include "vulkanpbr/VulkanglTFModel.h"

namespace xrvk
{
	struct SharedState
	{
		XrGraphicsBindingVulkan2KHR xrGraphicsBinding { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };
		std::array< VkClearValue, 2 > vkClearValues;

		VkInstance vkInstance = VK_NULL_HANDLE;
		VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures {};

		VkDevice vkDevice = VK_NULL_HANDLE;
		VkQueue vkQueue = VK_NULL_HANDLE;
		uint32_t vkQueueFamilyIndex = 0;
		uint32_t vkQueueIndex = 0;

		VkPipelineCache vkPipelineCache = VK_NULL_HANDLE;

		// Android specific
#ifdef XR_USE_PLATFORM_ANDROID
		AAssetManager *androidAssetManager = nullptr;
#endif
	};

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

	struct Buffer
	{
		VkDevice device = VK_NULL_HANDLE;
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDescriptorBufferInfo descriptor{};
		int32_t count = 0;
		void *mapped = nullptr;
		void create( vks::VulkanDevice *device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, bool map = true )
		{
			this->device = device->logicalDevice;
			device->createBuffer( usageFlags, memoryPropertyFlags, size, &buffer, &memory );
			descriptor = { buffer, 0, size };
			if ( map )
			{
				VK_CHECK_RESULT( vkMapMemory( device->logicalDevice, memory, 0, size, 0, &mapped ) );
			}
		}

		void create( vks::VulkanDevice *device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void *data )
		{
			this->device = device->logicalDevice;
			device->createBuffer( usageFlags, memoryPropertyFlags, size, &buffer, &memory, data );
			descriptor = { buffer, 0, size };

			VK_CHECK_RESULT( vkMapMemory( device->logicalDevice, memory, 0, size, 0, &mapped ) );
		}

		void destroy()
		{
			if ( mapped )
			{
				unmap();
			}
			vkDestroyBuffer( device, buffer, nullptr );
			vkFreeMemory( device, memory, nullptr );
			buffer = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
		}
		void map() { VK_CHECK_RESULT( vkMapMemory( device, memory, 0, VK_WHOLE_SIZE, 0, &mapped ) ); }
		void unmap()
		{
			if ( mapped )
			{
				vkUnmapMemory( device, memory );
				mapped = nullptr;
			}
		}
		void flush( VkDeviceSize size = VK_WHOLE_SIZE )
		{
			VkMappedMemoryRange mappedRange {};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = memory;
			mappedRange.size = size;
			VK_CHECK_RESULT( vkFlushMappedMemoryRanges( device, 1, &mappedRange ) );
		}
	};

	struct RenderSceneBase
	{
		// data payload
		bool bIsVisible = true;
		std::string sFilename;
		vkglTF::Model gltfModel;
		VkPipeline vkPipeline = VK_NULL_HANDLE;

		// custom info - gameplay or exts
		void *pSpaceLocationExtChain = nullptr;
		XrTime xrTimeOverride = 0;

		// current state
		XrPosef currentPose;
		XrVector3f currentScale = { 1.0f, 1.0f, 1.0f };

		// animation
		bool bPlayAnimations = false;
		int32_t fAnimIndex = 0;
		float fAnimTimer = 0.0f;
		float fAnimSpeed = 0.01f;

		RenderSceneBase( std::string filename )
			: sFilename( filename )
		{
		}
		~RenderSceneBase() {}

		// common functions
		void Reset()
		{
			bIsVisible = true;
			XrVector3f_Set( &currentScale, 1.0f );
			XrPosef_Identity( &currentPose );
		}

		void Reset( XrVector3f scale )
		{
			bIsVisible = true;
			currentScale = scale;
			XrPosef_Identity( &currentPose );
		}

		// virtual functions
		virtual void GetMatrix( XrMatrix4x4f *matrix ) = 0;

		// functions
		glm::vec3 GetScale() { return glm::vec3( currentScale.x, currentScale.y, currentScale.z ); }
		virtual glm::vec3 GetPosition() = 0;
		glm::quat GetRotation() { return glm::quat( currentPose.orientation.w, currentPose.orientation.x, currentPose.orientation.y, currentPose.orientation.z ); }

		void PlayAnimations()
		{
			if ( !bPlayAnimations )
				return;

			// Play animation
			uint32_t unAnimCount = static_cast< uint32_t >( gltfModel.animations.size() );
			if ( unAnimCount > 0 )
			{
				for ( uint32_t i = 0; i < unAnimCount; i++ )
				{
					fAnimTimer += fAnimSpeed;
					if ( fAnimTimer > gltfModel.animations[ i ].end )
					{
						fAnimTimer -= gltfModel.animations[ i ].end;
					}

					gltfModel.updateAnimation( 0, fAnimTimer );
				}
			}
		}
	};

	struct RenderScene : RenderSceneBase
	{
		RenderScene( std::string filename )
			: RenderSceneBase( filename )
		{
		}
		~RenderScene() {}

		glm::vec3 GetPosition() override { return glm::vec3( currentPose.position.x, currentPose.position.y, currentPose.position.z ); }
		void GetMatrix( XrMatrix4x4f *matrix ) override;
	};

	struct RenderSector : RenderScene
	{
		XrSpace xrSpace = XR_NULL_HANDLE;

		RenderSector( std::string filename )
			: RenderScene( filename )
		{
		}
		~RenderSector() {}

		glm::vec3 GetPosition() override { return glm::vec3( currentPose.position.x, currentPose.position.y, currentPose.position.z ); }
		void GetMatrix( XrMatrix4x4f *matrix ) override;
	};

	struct RenderModel : RenderSector
	{
		bool bApplyOffset = false;

		XrVector3f offsetPosition = { 0.0f, 0.0f, 0.0f };
		XrQuaternionf offsetRotation { 0.0f, 0.0f, 0.0f, 1.0f };

		RenderModel( std::string filename )
			: RenderSector( filename )
		{
		}
		~RenderModel() {}

		uint32_t unOffset = 0;
		glm::vec3 GetPosition() override
		{
			XrVector3f newPos;
			XrVector3f_Add( &newPos, &currentPose.position, &offsetPosition );
			return glm::vec3( newPos.x, newPos.y, newPos.z );
		}
		void GetMatrix( XrMatrix4x4f *matrix ) override;
	};
} // namespace xrvk