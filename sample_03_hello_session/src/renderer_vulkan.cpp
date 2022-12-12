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

#include "renderer_vulkan.h"

XrResult VulkanRenderer::Init( oxr::Provider *pProvider, oxr::AppInstanceInfo *pAppInstanceInfo )
{
	assert( pProvider );
	assert( pAppInstanceInfo );

	XrResult xrResult = XR_SUCCESS;

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
	std::vector< const char * > vecExtensions;
	std::vector< const char * > vecLayers;

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

	VkResult vkResult = VK_SUCCESS;
	xrResult = pProvider->CreateVulkanInstance( &xrVulkanInstanceCreateInfo, &m_vkInstance, &vkResult );
	if ( XR_UNQUALIFIED_SUCCESS( xrResult ) && vkResult == VK_SUCCESS )
	{
		oxr::Log( oxr::ELogLevel::LogInfo, m_sLogCategory, "Vulkan instance successfully created." );
	}
	else
	{
		oxr::LogError( m_sLogCategory, "Error creating vulkan instance with openxr result (%s) and vulkan result (%i)", oxr::XrEnumToString( xrResult ), ( int32_t )vkResult );
		return xrResult == XR_SUCCESS ? XR_ERROR_VALIDATION_FAILURE : xrResult;
	}

	// (5) Get the vulkan physical device (gpu) used by the runtime
	xrResult = pProvider->GetVulkanGraphicsPhysicalDevice( &m_vkPhysicalDevice, m_vkInstance );
	if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
	{
		oxr::LogError( m_sLogCategory, "Error getting the runtime's vulkan physical device (gpu) with result (%s) ", oxr::XrEnumToString( xrResult ) );
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
