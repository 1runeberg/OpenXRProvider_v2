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

#include <cassert>
#include <provider/provider.hpp>

namespace oxr
{
	std::vector< const char * > ConvertDelimitedCharArray( char *arrChars, char cDelimiter )
	{
		std::vector< const char * > vecOutput;

		// Loop 'til end of array
		while ( *arrChars != 0 )
		{
			vecOutput.push_back( arrChars );
			while ( *( ++arrChars ) != 0 )
			{
				if ( *arrChars == cDelimiter )
				{
					*arrChars++ = '\0';
					break;
				}
			}
		}
		return vecOutput;
	}

	Provider::Provider( ELogLevel eMinLogLevel )
		: m_eMinLogLevel( eMinLogLevel )
	{
		// Add greeting
		oxr::LogInfo( LOG_CATEGORY_PROVIDER, "G'Day! OPENXR PROVIDER version %i.%i.%i", PROVIDER_VERSION_MAJOR, PROVIDER_VERSION_MINOR, PROVIDER_VERSION_PATCH );
	}

	Provider::~Provider()
	{
		// Release session - this will gracefully tear down the openxr session
		if ( m_pSession )
		{
			delete m_pSession;
		}

		// Destroy instance
		if ( m_instance.xrInstance != XR_NULL_HANDLE )
		{
			xrDestroyInstance( m_instance.xrInstance );
		}

#ifdef XR_USE_PLATFORM_ANDROID
		m_instance.androidActivity->vm->DetachCurrentThread();
#endif
	}

#ifdef XR_USE_PLATFORM_ANDROID
	XrResult Provider::InitAndroidLoader( struct android_app *app )
	{
		XrResult xrResult = InitAndroidLoader_Internal( app );
		if ( XR_SUCCEEDED( xrResult ) )
		{
			oxr::LogInfo( m_sLogCategory, "Android loader initialized" );
		}

		return xrResult;
	}
#endif

	XrResult Provider::Init( const AppInstanceInfo *pAppInstanceInfo )
	{
		assert( pAppInstanceInfo );

		XrResult xrResult = XR_ERROR_RUNTIME_UNAVAILABLE;

		XrInstanceCreateInfo xrInstanceCreateInfo { XR_TYPE_INSTANCE_CREATE_INFO };
		xrInstanceCreateInfo.next = pAppInstanceInfo->pvAdditionalCreateInfo;
		xrInstanceCreateInfo.applicationInfo = {};

		strncpy( xrInstanceCreateInfo.applicationInfo.applicationName, pAppInstanceInfo->sAppName.c_str(), pAppInstanceInfo->sAppName.size() );
		strncpy( xrInstanceCreateInfo.applicationInfo.engineName, pAppInstanceInfo->sEngineName.c_str(), pAppInstanceInfo->sEngineName.size() );

		xrInstanceCreateInfo.applicationInfo.applicationVersion = pAppInstanceInfo->unAppVersion;
		xrInstanceCreateInfo.applicationInfo.engineVersion = pAppInstanceInfo->unEngineVersion;
		xrInstanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

		// Retrieve the supported extensions by the runtime
		std::vector< XrExtensionProperties > vecExtensionProperties;
		if ( !XR_SUCCEEDED( GetSupportedExtensions( vecExtensionProperties ) ) )
		{
			return XR_ERROR_RUNTIME_UNAVAILABLE;
		}
		oxr::LogInfo( m_sLogCategory, "This runtime supports %i available extensions:", vecExtensionProperties.size() );

		// Prepare app requested extensions
		std::vector< const char * > vecRequestedExtensionNames;
		if ( pAppInstanceInfo->vecInstanceExtensions.size() > 0 )
		{
			vecRequestedExtensionNames = pAppInstanceInfo->vecInstanceExtensions; // copy
		}

		// Remove any unsupported graphics api extensions from requested extensions (only Vulkan is supported)
		FilterOutUnsupportedGraphicsApis( vecRequestedExtensionNames );

		// Get the supported extension names by the runtime so we can check for vulkan support
		std::vector< std::string > vecSupportedExtensionNames;
		GetExtensionNames( vecExtensionProperties, vecSupportedExtensionNames );

		// Set the vulkan ext requested
		std::string sBestVulkanExt;
		if ( !SetVulkanExt( vecRequestedExtensionNames ) )
		{
			// if none was provided by the app, the best available vulkan ext will be enabled instead
			xrResult = GetBestVulkanExt( vecSupportedExtensionNames, sBestVulkanExt );
			if ( !XR_SUCCEEDED( xrResult ) )
			{
				return xrResult;
			}
			else
			{
				vecRequestedExtensionNames.push_back( sBestVulkanExt.c_str() );
			}
		}

		// Add extensions to create instance info
		xrInstanceCreateInfo.enabledExtensionCount = ( uint32_t )vecRequestedExtensionNames.size();
		xrInstanceCreateInfo.enabledExtensionNames = vecRequestedExtensionNames.data();

		// Cache enabled extensions
		for ( auto &xrExtensionProperty : vecExtensionProperties )
		{
			std::string sExtensionName = std::string( xrExtensionProperty.extensionName );
			bool bFound = std::find( vecRequestedExtensionNames.begin(), vecRequestedExtensionNames.end(), sExtensionName ) != vecRequestedExtensionNames.end();

			if ( bFound )
			{
				m_instance.vecEnabledExtensions.push_back( sExtensionName );
			}

			if ( oxr::CheckLogLevelDebug( m_eMinLogLevel ) )
			{
				std::string sEnableTag = bFound ? "[WILL ENABLE]" : "";
				oxr::LogDebug( m_sLogCategory, "\t%s (ver. %i) %s", &xrExtensionProperty.extensionName, xrExtensionProperty.extensionVersion, sEnableTag.c_str() );
			}
		}

		// Retrieve the supported api layers by the runtime
		std::vector< XrApiLayerProperties > vecApiLayerProperties;
		if ( !XR_SUCCEEDED( GetSupportedApiLayers( vecApiLayerProperties ) ) )
		{
			return XR_ERROR_RUNTIME_UNAVAILABLE;
		}

		oxr::LogInfo( m_sLogCategory, "There are %i openxr api layers available:", vecApiLayerProperties.size() );

		// Prepare app requested api layers
		std::vector< const char * > vecRequestedApiLayerNames;
		if ( pAppInstanceInfo->vecAPILayers.size() > 0 )
		{
			vecRequestedApiLayerNames = pAppInstanceInfo->vecAPILayers; // copy
		}

		// Cache enabled api layers
		for ( auto &xrApiLayerProperty : vecApiLayerProperties )
		{
			std::string sApiLayerName = std::string( xrApiLayerProperty.layerName );
			bool bFound = std::find( vecRequestedApiLayerNames.begin(), vecRequestedApiLayerNames.end(), sApiLayerName ) != vecRequestedApiLayerNames.end();

			if ( bFound )
			{
				m_vecEnabledApiLayers.push_back( sApiLayerName );
			}

			if ( oxr::CheckLogLevelDebug( m_eMinLogLevel ) )
			{
				std::string sEnableTag = bFound ? "[WILL ENABLE]" : "";

				oxr::LogDebug( m_sLogCategory, "\t%s (ver. %i) %s", xrApiLayerProperty.layerName, xrApiLayerProperty.specVersion, sEnableTag.c_str() );
				oxr::LogDebug( m_sLogCategory, "\t\t%s\n", xrApiLayerProperty.description );
			}
		}

		// Add api layers to create instance info
		xrInstanceCreateInfo.enabledApiLayerCount = ( uint32_t )vecRequestedApiLayerNames.size();
		xrInstanceCreateInfo.enabledApiLayerNames = vecRequestedApiLayerNames.data();

		// Create openxr instance
		xrResult = xrCreateInstance( &xrInstanceCreateInfo, &m_instance.xrInstance );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogInfo( m_sLogCategory, "OpenXr instance created. Handle (%" PRIu64 ")", ( uint64_t )m_instance.xrInstance );
		}
		else
		{
			oxr::LogError( m_sLogCategory, "Error creating openxr instance (%i)", xrResult );
			return xrResult;
		}

		// Get instance properties
		xrResult = xrGetInstanceProperties( m_instance.xrInstance, &m_instance.xrInstanceProperties );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogInfo(
				m_sLogCategory,
				"OpenXr runtime %s version %i.%i.%i is now active for this instance.",
				m_instance.xrInstanceProperties.runtimeName,
				XR_VERSION_MAJOR( m_instance.xrInstanceProperties.runtimeVersion ),
				XR_VERSION_MINOR( m_instance.xrInstanceProperties.runtimeVersion ),
				XR_VERSION_PATCH( m_instance.xrInstanceProperties.runtimeVersion ) );
		}
		else
		{
			oxr::LogError( m_sLogCategory, "Error getting active openxr instance properties (%i)", xrResult );
			return xrResult;
		}

		// Get user's system info
		XrSystemGetInfo xrSystemGetInfo { XR_TYPE_SYSTEM_GET_INFO };
		xrSystemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

		xrResult = xrGetSystem( m_instance.xrInstance, &xrSystemGetInfo, &m_instance.xrSystemId );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			xrResult = xrGetSystemProperties( m_instance.xrInstance, m_instance.xrSystemId, &m_instance.xrSystemProperties );
			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				xrGetSystemProperties( m_instance.xrInstance, m_instance.xrSystemId, &m_instance.xrSystemProperties );
				oxr::LogInfo( m_sLogCategory, "Active tracking system is %s (Vendor Id %i)", m_instance.xrSystemProperties.systemName, m_instance.xrSystemProperties.vendorId );
			}
			else
			{
				oxr::LogError( m_sLogCategory, "Error getting user's system info (%s)", XrEnumToString( xrResult ) );
			}
		}
		else
		{
			oxr::LogError( m_sLogCategory, "Error getting user's system id (%s)", XrEnumToString( xrResult ) );
		}

		// Show supported view configurations (debug only)
		if ( oxr::CheckLogLevelDebug( m_eMinLogLevel ) )
		{
			auto vecSupportedViewConfigurations = GetSupportedViewConfigurations();

			oxr::LogDebug( m_sLogCategory, "This runtime supports %i view configuration(s):", vecSupportedViewConfigurations.size() );
			for ( auto &xrViewConfigurationType : vecSupportedViewConfigurations )
			{
				oxr::LogDebug( m_sLogCategory, "\t%s", XrEnumToString( xrViewConfigurationType ) );
			}
		}

		return xrResult;
	}

	XrResult Provider::CheckIfXrError( bool bTest, XrResult xrResult, const char *pccErrorMsg )
	{
		if ( bTest )
		{
			oxr::LogError( m_sLogCategory, pccErrorMsg );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult Provider::CheckIfInitCalled()
	{
		// Check if there's a valid openxr instance
		return CheckIfXrError( m_instance.xrInstance == XR_NULL_HANDLE, XR_ERROR_CALL_ORDER_INVALID, "Error - No openxr instance established with the runtime. Have you called Provider.Init?" );
	}

	XrEventDataBaseHeader *Provider::PollXrEvents()
	{
		XrEventDataBaseHeader *xrEventDataBaseHeader = reinterpret_cast< XrEventDataBaseHeader * >( &m_xrEventDataBuffer );
		*xrEventDataBaseHeader = { XR_TYPE_EVENT_DATA_BUFFER };

		const XrResult xrResult = xrPollEvent( m_instance.xrInstance, &m_xrEventDataBuffer );

		if ( xrResult == XR_SUCCESS )
		{
			if ( xrEventDataBaseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST )
			{
				// Report any lost events
				const XrEventDataEventsLost *const eventsLost = reinterpret_cast< const XrEventDataEventsLost * >( xrEventDataBaseHeader );
				oxr::LogWarning( m_sLogCategory, "Poll events warning - there are %i events lost", eventsLost->lostEventCount );
			}
			else if ( xrEventDataBaseHeader->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
			{
				// Update session state
				auto xrSessionStateChangedEvent = *reinterpret_cast< const XrEventDataSessionStateChanged * >( xrEventDataBaseHeader );

				const XrSessionState xrCurrentSessionState = Session()->GetState();
				Session()->SetState( xrSessionStateChangedEvent.state );

				oxr::LogDebug( m_sLogCategory, "Session state changed from %s to %s", XrEnumToString( xrCurrentSessionState ), XrEnumToString( xrSessionStateChangedEvent.state ) );
			}
		}
		else
		{
			return nullptr;
		}

		return xrEventDataBaseHeader;
	}

	XrResult Provider::GetSupportedApiLayers( std::vector< XrApiLayerProperties > &vecApiLayers )
	{
		uint32_t unCount = 0;
		XrResult xrResult = xrEnumerateApiLayerProperties( 0, &unCount, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			if ( unCount == 0 )
				return XR_SUCCESS;

			vecApiLayers.clear();
			for ( uint32_t i = 0; i < unCount; ++i )
			{
				vecApiLayers.push_back( XrApiLayerProperties { XR_TYPE_API_LAYER_PROPERTIES, nullptr } );
			}

			return xrEnumerateApiLayerProperties( unCount, &unCount, vecApiLayers.data() );
		}

		return xrResult;
	}

	XrResult Provider::GetSupportedApiLayers( std::vector< std::string > &vecApiLayers )
	{
		std::vector< XrApiLayerProperties > vecApiLayerProperties;
		XrResult xrResult = GetSupportedApiLayers( vecApiLayerProperties );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			GetApilayerNames( vecApiLayerProperties, vecApiLayers );
		}

		return xrResult;
	}

	XrResult Provider::GetSupportedExtensions( std::vector< XrExtensionProperties > &vecExtensions, const char *pcharApiLayerName )
	{
		uint32_t unCount = 0;
		XrResult xrResult = xrEnumerateInstanceExtensionProperties( pcharApiLayerName, 0, &unCount, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			if ( unCount == 0 )
				return XR_SUCCESS;

			vecExtensions.clear();
			for ( uint32_t i = 0; i < unCount; ++i )
			{
				vecExtensions.push_back( XrExtensionProperties { XR_TYPE_EXTENSION_PROPERTIES, nullptr } );
			}

			return xrEnumerateInstanceExtensionProperties( pcharApiLayerName, unCount, &unCount, vecExtensions.data() );
		}

		return xrResult;
	}

	XrResult Provider::GetSupportedExtensions( std::vector< std::string > &vecExtensions, const char *pcharApiLayerName )
	{
		std::vector< XrExtensionProperties > vecExtensionProperties;
		XrResult xrResult = GetSupportedExtensions( vecExtensionProperties );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			GetExtensionNames( vecExtensionProperties, vecExtensions );
		}

		return xrResult;
	}

	XrResult Provider::FilterForSupportedExtensions( std::vector< std::string > &vecRequestedExtensionNames )
	{
		// Get supported extensions from active runtime
		std::vector< std::string > vecSupportedExtensions;
		XrResult xrResult = GetSupportedExtensions( vecSupportedExtensions );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			for ( std::vector< std::string >::iterator it = vecRequestedExtensionNames.begin(); it != vecRequestedExtensionNames.end(); )
			{
				// Find requested extension is the runtime's list of supported extensions
				bool bFound = false;
				for ( auto &sExtensionName : vecSupportedExtensions )
				{
					if ( sExtensionName == *it )
					{
						bFound = true;
						break;
					}
				}

				// If the requested extension isn't found, delete it
				if ( !bFound )
				{
					vecRequestedExtensionNames.erase( it );
				}
			}

			// shrink the requested extension vector in case we needed to delete values from it
			vecRequestedExtensionNames.shrink_to_fit();
		}

		return xrResult;
	}

	XrResult Provider::FilterOutUnsupportedExtensions( std::vector< const char * > &vecExtensionNames )
	{
		std::vector< XrExtensionProperties > vecSupportedExtensions;
		XrResult xrResult = GetSupportedExtensions( vecSupportedExtensions );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		for ( auto it = vecExtensionNames.begin(); it != vecExtensionNames.end(); it++ )
		{
			// Check if our requested extension exists
			bool bFound = false;
			for ( auto &supportedExt : vecSupportedExtensions )
			{
				if ( std::strcmp( supportedExt.extensionName, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If not supported by the current openxr runtime, then remove it from our list
			if ( !bFound )
			{
				vecExtensionNames.erase( it-- );
			}
		}

		return XR_SUCCESS;
	}

	XrResult Provider::FilterOutUnsupportedApiLayers( std::vector< const char * > &vecApiLayerNames )
	{
		std::vector< XrApiLayerProperties > vecSupportedApiLayers;
		XrResult xrResult = GetSupportedApiLayers( vecSupportedApiLayers );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		for ( auto it = vecApiLayerNames.begin(); it != vecApiLayerNames.end(); it++ )
		{
			// Check if our requested extension exists
			bool bFound = false;
			for ( auto &supportedApiLayer : vecSupportedApiLayers )
			{
				if ( std::strcmp( supportedApiLayer.layerName, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If not supported by the current openxr runtime, then remove it from our list
			if ( !bFound )
			{
				vecApiLayerNames.erase( it-- );
			}
		}

		return XR_SUCCESS;
	}

	std::vector< XrViewConfigurationType > Provider::GetSupportedViewConfigurations()
	{

		// Check if there's a valid openxr instance
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return {};

		// Get number of view configurations supported by the runtime
		uint32_t unViewConfigurationsNum = 0;

		xrResult = xrEnumerateViewConfigurations( m_instance.xrInstance, m_instance.xrSystemId, unViewConfigurationsNum, &unViewConfigurationsNum, nullptr );
		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			std::vector< XrViewConfigurationType > vecViewConfigurations( unViewConfigurationsNum );
			xrResult = xrEnumerateViewConfigurations( m_instance.xrInstance, m_instance.xrSystemId, unViewConfigurationsNum, &unViewConfigurationsNum, vecViewConfigurations.data() );

			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				return vecViewConfigurations;
			}
		}

		oxr::LogError( m_sLogCategory, "Error getting supported view configuration types from the runtime (%s)", XrEnumToString( xrResult ) );
		return {};
	}

	XrResult Provider::GetVulkanGraphicsRequirements( XrGraphicsRequirementsVulkan2KHR *xrGraphicsRequirements )
	{
		// Check if there's a valid openxr instance
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Get the vulkan graphics requirements (min/max vulkan api version, etc) of the runtime
		PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
		xrResult = xrGetInstanceProcAddr( m_instance.xrInstance, "xrGetVulkanGraphicsRequirementsKHR", reinterpret_cast< PFN_xrVoidFunction * >( &pfnGetVulkanGraphicsRequirementsKHR ) );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			// We'll retrieve the Vulkan1 requirements and use it as the min/max vulkan api
			XrGraphicsRequirementsVulkanKHR vulkan1Requirements { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
			xrResult = pfnGetVulkanGraphicsRequirementsKHR( m_instance.xrInstance, m_instance.xrSystemId, &vulkan1Requirements );

			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				xrGraphicsRequirements->maxApiVersionSupported = vulkan1Requirements.maxApiVersionSupported;
				xrGraphicsRequirements->minApiVersionSupported = vulkan1Requirements.minApiVersionSupported;
			}
		}

		return xrResult;
	}

	XrResult Provider::CreateVulkanInstance( const XrVulkanInstanceCreateInfoKHR *xrVulkanInstanceCreateInfo, VkInstance *vkInstance, VkResult *vkResult )
	{
		// Check if there's a valid openxr instance
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Check vulkan extensions required by the runtime
		PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
		xrResult = xrGetInstanceProcAddr( m_instance.xrInstance, "xrGetVulkanInstanceExtensionsKHR", reinterpret_cast< PFN_xrVoidFunction * >( &pfnGetVulkanInstanceExtensionsKHR ) );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			uint32_t unExtNum = 0;
			xrResult = pfnGetVulkanInstanceExtensionsKHR( m_instance.xrInstance, xrVulkanInstanceCreateInfo->systemId, 0, &unExtNum, nullptr );

			// Check number of extensions required by the runtime
			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				std::vector< char > vecExtensionNames( unExtNum );
				xrResult = pfnGetVulkanInstanceExtensionsKHR( m_instance.xrInstance, xrVulkanInstanceCreateInfo->systemId, unExtNum, &unExtNum, vecExtensionNames.data() );

				// Combine runtime and app extensions
				if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
				{
					std::vector< const char * > vecExtensions = ConvertDelimitedCharArray( vecExtensionNames.data(), ' ' );
					for ( uint32_t i = 0; i < xrVulkanInstanceCreateInfo->vulkanCreateInfo->enabledExtensionCount; ++i )
					{
						vecExtensions.push_back( xrVulkanInstanceCreateInfo->vulkanCreateInfo->ppEnabledExtensionNames[ i ] );
					}

					VkInstanceCreateInfo vkInstanceCreateInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
					memcpy( &vkInstanceCreateInfo, xrVulkanInstanceCreateInfo->vulkanCreateInfo, sizeof( vkInstanceCreateInfo ) );
					vkInstanceCreateInfo.enabledExtensionCount = ( uint32_t )vecExtensions.size();
					vkInstanceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

					auto pfnCreateInstance = ( PFN_vkCreateInstance )xrVulkanInstanceCreateInfo->pfnGetInstanceProcAddr( nullptr, "vkCreateInstance" );
					*vkResult = pfnCreateInstance( &vkInstanceCreateInfo, xrVulkanInstanceCreateInfo->vulkanAllocator, vkInstance );
				}
			}
		}

		return xrResult;
	}

	XrResult Provider::GetVulkanGraphicsPhysicalDevice( VkPhysicalDevice *vkPhysicalDevice, VkInstance vkInstance )
	{
		// Check if there's a valid openxr instance
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
		xrResult = xrGetInstanceProcAddr( m_instance.xrInstance, "xrGetVulkanGraphicsDeviceKHR", reinterpret_cast< PFN_xrVoidFunction * >( &pfnGetVulkanGraphicsDeviceKHR ) );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			xrResult = pfnGetVulkanGraphicsDeviceKHR( m_instance.xrInstance, m_instance.xrSystemId, vkInstance, vkPhysicalDevice );
		}

		return XR_SUCCESS;
	}

	XrResult
		Provider::CreateVulkanDevice( const XrVulkanDeviceCreateInfoKHR *xrVulkanDeviceCreateInfo, VkPhysicalDevice *vkPhysicalDevice, VkInstance *vkInstance, VkDevice *vkDevice, VkResult *vkResult )
	{
		// Check if there's a valid openxr instance
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
		xrResult = xrGetInstanceProcAddr( m_instance.xrInstance, "xrGetVulkanDeviceExtensionsKHR", reinterpret_cast< PFN_xrVoidFunction * >( &pfnGetVulkanDeviceExtensionsKHR ) );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			// Get number of device extensions needed by the runtime
			uint32_t unDeviceExtensionsNum = 0;
			xrResult = pfnGetVulkanDeviceExtensionsKHR( m_instance.xrInstance, xrVulkanDeviceCreateInfo->systemId, 0, &unDeviceExtensionsNum, nullptr );

			std::vector< char > vecDeviceExtensionNames( unDeviceExtensionsNum );
			xrResult = ( pfnGetVulkanDeviceExtensionsKHR( m_instance.xrInstance, xrVulkanDeviceCreateInfo->systemId, unDeviceExtensionsNum, &unDeviceExtensionsNum, vecDeviceExtensionNames.data() ) );

			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				// Merge runtime and app's required extensions
				std::vector< const char * > vecExtensions = ConvertDelimitedCharArray( vecDeviceExtensionNames.data(), ' ' );
				for ( uint32_t i = 0; i < xrVulkanDeviceCreateInfo->vulkanCreateInfo->enabledExtensionCount; ++i )
				{
					vecExtensions.push_back( xrVulkanDeviceCreateInfo->vulkanCreateInfo->ppEnabledExtensionNames[ i ] );
				}

				VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures {};
				memcpy( &vkPhysicalDeviceFeatures, xrVulkanDeviceCreateInfo->vulkanCreateInfo->pEnabledFeatures, sizeof( vkPhysicalDeviceFeatures ) );

#if !defined( XR_USE_PLATFORM_ANDROID )
				// Mute validation error with Meta PC runtime
				VkPhysicalDeviceFeatures metaRequiredFeatures {};
				vkGetPhysicalDeviceFeatures( *vkPhysicalDevice, &metaRequiredFeatures );
				if ( metaRequiredFeatures.shaderStorageImageMultisample == VK_TRUE )
				{
					vkPhysicalDeviceFeatures.shaderStorageImageMultisample = VK_TRUE;
				}
#endif

				VkDeviceCreateInfo vkDeviceCreateInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
				memcpy( &vkDeviceCreateInfo, xrVulkanDeviceCreateInfo->vulkanCreateInfo, sizeof( vkDeviceCreateInfo ) );
				vkDeviceCreateInfo.pEnabledFeatures = &vkPhysicalDeviceFeatures;
				vkDeviceCreateInfo.enabledExtensionCount = ( uint32_t )vecExtensions.size();
				vkDeviceCreateInfo.ppEnabledExtensionNames = vecExtensions.empty() ? nullptr : vecExtensions.data();

				auto pfnCreateDevice = ( PFN_vkCreateDevice )xrVulkanDeviceCreateInfo->pfnGetInstanceProcAddr( *vkInstance, "vkCreateDevice" );
				*vkResult = pfnCreateDevice( *vkPhysicalDevice, &vkDeviceCreateInfo, xrVulkanDeviceCreateInfo->vulkanAllocator, vkDevice );
			}
		}

		return xrResult;
	}

	XrResult Provider::CreateSession( XrGraphicsBindingVulkanKHR *pGraphicsBindingVulkan, XrSessionCreateFlags flgAdditionalCreateInfo )
	{
		// Check for depth support
		bool bEnableDepthSupport = IsExtensionEnabled( XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME );

		// Create session
		XrSessionCreateInfo xrSessionCreateInfo { XR_TYPE_SESSION_CREATE_INFO };
		xrSessionCreateInfo.next = pGraphicsBindingVulkan;
		xrSessionCreateInfo.systemId = m_instance.xrSystemId;
		xrSessionCreateInfo.createFlags = flgAdditionalCreateInfo;

		m_pSession = new oxr::Session( &m_instance, m_eMinLogLevel, bEnableDepthSupport );
		XrResult xrResult = m_pSession->Init( &xrSessionCreateInfo );

		if ( !XR_SUCCEEDED( xrResult ) )
		{
			delete m_pSession;
			oxr::LogError( m_sLogCategory, "Error creating session (%s)", XrEnumToString( xrResult ) );
		}

		return xrResult;
	}

	XrResult Provider::CreateSession( XrSessionCreateInfo *pSessionCreateInfo )
	{
		m_pSession = new oxr::Session( &m_instance, m_eMinLogLevel );
		XrResult xrResult = m_pSession->Init( pSessionCreateInfo );

		if ( !XR_SUCCEEDED( xrResult ) )
		{
			delete m_pSession;
			oxr::LogError( m_sLogCategory, "Error creating session (%s)", XrEnumToString( xrResult ) );
		}

		return xrResult;
	}

	Session *Provider::Session()
	{
		if ( m_pSession == nullptr )
		{
			oxr::LogError( m_sLogCategory, "Error invoking openxr session object. Did you create one?" );
			assert( m_pSession );
		}

		return m_pSession;
	}

#ifdef XR_USE_PLATFORM_ANDROID
	XrResult Provider::InitAndroidLoader_Internal( struct android_app *app )
	{
		m_instance.androidApp = app;
		m_instance.androidActivity = app->activity;
		m_instance.androidActivity->vm->AttachCurrentThread( &m_instance.jniEnv, nullptr );

		app->userData = &m_instance.androidAppState;

		PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
		XrResult xrResult = xrGetInstanceProcAddr( XR_NULL_HANDLE, "xrInitializeLoaderKHR", ( PFN_xrVoidFunction * )( &initializeLoader ) );
		if ( XR_SUCCEEDED( xrResult ) )
		{
			XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
			memset( &loaderInitInfoAndroid, 0, sizeof( loaderInitInfoAndroid ) );
			loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
			loaderInitInfoAndroid.next = NULL;
			loaderInitInfoAndroid.applicationVM = app->activity->vm;
			loaderInitInfoAndroid.applicationContext = app->activity->clazz;
			initializeLoader( ( const XrLoaderInitInfoBaseHeaderKHR * )&loaderInitInfoAndroid );
		}

		return xrResult;
	}
#endif

	void Provider::GetApilayerNames( std::vector< XrApiLayerProperties > &vecApilayerProperties, std::vector< std::string > &vecOutApiLayerNames )
	{
		for ( auto &apiLayerProperty : vecApilayerProperties )
		{
			vecOutApiLayerNames.push_back( apiLayerProperty.layerName );
		}
	}

	void Provider::GetExtensionNames( std::vector< XrExtensionProperties > &vecExtensionProperties, std::vector< std::string > &vecOutExtensionNames )
	{
		for ( auto &extensionProperty : vecExtensionProperties )
		{
			vecOutExtensionNames.push_back( extensionProperty.extensionName );
		}
	}

	XrResult Provider::GetBestVulkanExt( std::vector< std::string > &vecSupportedExtensionNames, std::string &sBestVulkanExt )
	{
		// App didn't provide a preferred vulkan ext, prefer vulkan2
		bool bFound = std::find( vecSupportedExtensionNames.begin(), vecSupportedExtensionNames.end(), std::string( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) ) != vecSupportedExtensionNames.end();

		if ( bFound )
		{
			sBestVulkanExt = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
			m_instance.eCurrentVulkanExt = EVulkanExt::VulkanExt2;
			return XR_SUCCESS;
		}

		// Otherwise, use vulkan1
		bFound = std::find( vecSupportedExtensionNames.begin(), vecSupportedExtensionNames.end(), std::string( XR_KHR_VULKAN_ENABLE_EXTENSION_NAME ) ) != vecSupportedExtensionNames.end();

		// Return an error if the current runtime doesn't support any vulkan extensions
		if ( bFound )
		{
			sBestVulkanExt = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
			m_instance.eCurrentVulkanExt = EVulkanExt::VulkanExt1;
		}
		else
		{
			oxr::LogError( m_sLogCategory, "This runtime does not support any Vulkan extensions!" );
			return XR_ERROR_EXTENSION_NOT_PRESENT;
		}

		return XR_SUCCESS;
	}

	bool Provider::SetVulkanExt( std::vector< const char * > &vecExtensionNames )
	{
		// Prefer vulkan 2
		bool bFound = std::find( vecExtensionNames.begin(), vecExtensionNames.end(), std::string( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) ) != vecExtensionNames.end();

		if ( bFound )
		{
			m_instance.eCurrentVulkanExt = EVulkanExt::VulkanExt2;
			return true;
		}

		// Otherwise, vulkan 1
		bFound = std::find( vecExtensionNames.begin(), vecExtensionNames.end(), std::string( XR_KHR_VULKAN_ENABLE_EXTENSION_NAME ) ) != vecExtensionNames.end();

		if ( bFound )
		{
			m_instance.eCurrentVulkanExt = EVulkanExt::VulkanExt1;
			return true;
		}

		return false;
	}

	void Provider::FilterOutUnsupportedGraphicsApis( std::vector< const char * > &vecExtensionNames )
	{
		// @todo - capture from openxr_platform.h
		std::vector< const char * > vecUnsupportedGraphicsApis { "XR_KHR_opengl_enable", "XR_KHR_opengl_es_enable", "XR_KHR_D3D11_enable", "XR_KHR_D3D12_enable", "XR_MNDX_egl_enable" };

		for ( auto it = vecExtensionNames.begin(); it != vecExtensionNames.end(); it++ )
		{
			// Check if there's an unsupported graphics api in the input vector
			bool bFound = false;
			for ( auto &unsupportedExt : vecUnsupportedGraphicsApis )
			{
				if ( std::strcmp( unsupportedExt, *it ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			// If an unsupported graphics api is found in the list, then remove it
			if ( bFound )
			{
				vecExtensionNames.erase( it-- );
			}
		}
	}

	bool Provider::FindStringInVector( std::vector< std::string > &vecStrings, std::string s )
	{
		for ( auto str : vecStrings )
		{
			if ( str == s )
			{
				return true;
			}
		}

		return false;
	}

} // namespace oxr
