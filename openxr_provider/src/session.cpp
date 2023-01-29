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

#include <provider/session.hpp>

namespace oxr
{
	XrResult Session::CheckIfXrError( bool bTest, XrResult xrResult, const char *pccErrorMsg )
	{
		if ( bTest )
		{
			oxr::LogError( m_sLogCategory, pccErrorMsg );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult Session::CheckIfInitCalled()
	{
		// Check if there's a valid openxr instance
		return CheckIfXrError( m_xrSession == XR_NULL_HANDLE, XR_ERROR_CALL_ORDER_INVALID, "Error - This session has not been initialized properly. Have you called Session.Init?" );
	}

	Session::Session( oxr::Instance *pInstance, ELogLevel eLogLevel, bool bDepthhandling )
		: m_pInstance( pInstance )
		, m_eMinLogLevel( eLogLevel )
		, m_bDepthHandling( bDepthhandling )
	{
	}

	XrResult Session::Init( XrSessionCreateInfo *pSessionCreateInfo, XrReferenceSpaceType xrRefSpaceType, XrPosef xrReferencePose )
	{
		assert( m_pInstance->xrInstance != XR_NULL_HANDLE );

		// Create session
		XrResult xrResult = xrCreateSession( m_pInstance->xrInstance, pSessionCreateInfo, &m_xrSession );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			// Add extensions that require a session
			for ( auto &extensionName : m_pInstance->vecEnabledExtensions )
			{
				if ( extensionName == XR_KHR_VISIBILITY_MASK_EXTENSION_NAME )
					m_pInstance->extHandler.AddExtension( m_pInstance->xrInstance, m_xrSession, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME );

				if ( extensionName == XR_EXT_HAND_TRACKING_EXTENSION_NAME )
					m_pInstance->extHandler.AddExtension( m_pInstance->xrInstance, m_xrSession, XR_EXT_HAND_TRACKING_EXTENSION_NAME );

				if ( extensionName == XR_FB_PASSTHROUGH_EXTENSION_NAME )
					m_pInstance->extHandler.AddExtension( m_pInstance->xrInstance, m_xrSession, XR_FB_PASSTHROUGH_EXTENSION_NAME );

				if ( extensionName == XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME )
					m_pInstance->extHandler.AddExtension( m_pInstance->xrInstance, m_xrSession, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME );

				if ( extensionName == XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME )
					m_pInstance->extHandler.AddExtension( m_pInstance->xrInstance, m_xrSession, XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);
			}

			// Log session supported reference space types (debug only)
			if ( oxr::CheckLogLevelDebug( m_eMinLogLevel ) )
			{
				auto vecSupportedReferenceSpaceTypes = GetSupportedReferenceSpaceTypes();

				oxr::LogDebug( m_sLogCategory, "This session supports %i reference space type(s):", vecSupportedReferenceSpaceTypes.size() );
				for ( auto &xrReferenceSpaceType : vecSupportedReferenceSpaceTypes )
				{
					oxr::LogDebug( m_sLogCategory, "\t%s", XrEnumToString( xrReferenceSpaceType ) );
				}
			}

			// Create reference space
			XrReferenceSpaceCreateInfo xrReferenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
			xrReferenceSpaceCreateInfo.poseInReferenceSpace = xrReferencePose;
			xrReferenceSpaceCreateInfo.referenceSpaceType = xrRefSpaceType;
			xrResult = xrCreateReferenceSpace( m_xrSession, &xrReferenceSpaceCreateInfo, &m_xrReferenceSpace );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;

			oxr::LogDebug( m_sLogCategory, "Reference space of type (%s) created with handle (%" PRIu64 ").", XrEnumToString( xrRefSpaceType ), ( uint64_t )m_xrReferenceSpace );

			// Create app space
			xrReferenceSpaceCreateInfo.poseInReferenceSpace = xrReferencePose;
			xrReferenceSpaceCreateInfo.referenceSpaceType = xrRefSpaceType;
			xrResult = xrCreateReferenceSpace( m_xrSession, &xrReferenceSpaceCreateInfo, &m_xrAppSpace );

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;

			oxr::LogDebug( m_sLogCategory, "App Reference space of type (%s) created with handle (%" PRIu64 ").", XrEnumToString( xrRefSpaceType ), ( uint64_t )m_xrReferenceSpace );
		}

		return xrResult;
	}

	XrResult Session::Begin( XrViewConfigurationType xrViewConfigurationType, void *pvOtherBeginInfo, void *pvOtherReferenceSpaceInfo )
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Begin session
		XrSessionBeginInfo xrSessionBeginInfo { XR_TYPE_SESSION_BEGIN_INFO };
		xrSessionBeginInfo.next = pvOtherBeginInfo;
		xrSessionBeginInfo.primaryViewConfigurationType = xrViewConfigurationType;

		xrResult = xrBeginSession( m_xrSession, &xrSessionBeginInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogError( m_sLogCategory, "Unable to begin session (%s)", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Cache view configuration type
		m_xrViewConfigurationType = xrViewConfigurationType;

		oxr::LogInfo( m_sLogCategory, "OpenXR session started." );
		return XR_SUCCESS;
	}

	XrResult Session::End()
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		xrResult = xrEndSession( m_xrSession );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogError( m_sLogCategory, "Unable to end session (%s)", XrEnumToString( xrResult ) );
			return xrResult;
		}

		oxr::LogInfo( m_sLogCategory, "OpenXR session ended." );
		return XR_SUCCESS;
	}

	XrResult Session::RequestExit()
	{
		// Check if session was initialized correctly
		if ( !XR_UNQUALIFIED_SUCCESS( CheckIfInitCalled() ) )
			return XR_ERROR_CALL_ORDER_INVALID;

		return xrRequestExitSession( m_xrSession );
	}

	std::vector< XrReferenceSpaceType > Session::GetSupportedReferenceSpaceTypes()
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return {};

		// Get number of reference space types supported by the runtime
		uint32_t unReferenceSpaceTypesNum = 0;

		xrResult = xrEnumerateReferenceSpaces( m_xrSession, unReferenceSpaceTypesNum, &unReferenceSpaceTypesNum, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			std::vector< XrReferenceSpaceType > vecViewReferenceSpaceTypes( unReferenceSpaceTypesNum );
			xrResult = xrEnumerateReferenceSpaces( m_xrSession, unReferenceSpaceTypesNum, &unReferenceSpaceTypesNum, vecViewReferenceSpaceTypes.data() );

			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				return vecViewReferenceSpaceTypes;
			}
		}

		oxr::LogError( m_sLogCategory, "Error getting supported reference space types from the runtime (%s)", XrEnumToString( xrResult ) );
		return {};
	}

	XrResult Session::CreateReferenceSpace( XrSpace *outSpace, XrReferenceSpaceType xrReferenceSpaceType, XrPosef xrReferencePose, void *pvAdditionalCreateInfo )
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		XrReferenceSpaceCreateInfo xrReferenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		xrReferenceSpaceCreateInfo.next = pvAdditionalCreateInfo;
		xrReferenceSpaceCreateInfo.referenceSpaceType = xrReferenceSpaceType;
		xrReferenceSpaceCreateInfo.poseInReferenceSpace = xrReferencePose;

		xrResult = xrCreateReferenceSpace( m_xrSession, &xrReferenceSpaceCreateInfo, outSpace );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			oxr::LogDebug( m_sLogCategory, "Reference space created of type (%s) Handle (%" PRIu64 ")", XrEnumToString( xrReferenceSpaceType ), ( uint64_t )*outSpace );
		}

		return xrResult;
	}

	XrResult Session::LocateSpace( XrSpace baseSpace, XrSpace targetSpace, XrTime predictedDisplayTime, XrSpaceLocation *outSpaceLocation )
	{
		return xrLocateSpace( targetSpace, baseSpace, predictedDisplayTime, outSpaceLocation );
	}

	XrResult Session::LocateReferenceSpace( XrTime predictedDisplayTime, XrSpaceLocation *outSpaceLocation )
	{
		return LocateSpace( m_xrReferenceSpace, m_xrReferenceSpace, predictedDisplayTime, outSpaceLocation );
	}

	XrResult Session::LocateAppSpace( XrTime predictedDisplayTime, XrSpaceLocation *outSpaceLocation )
	{
		return LocateSpace( m_xrReferenceSpace, m_xrAppSpace, predictedDisplayTime, outSpaceLocation );
	}

	const std::vector< XrViewConfigurationView > &Session::UpdateConfigurationViews( XrResult *outResult, XrViewConfigurationType xrViewConfigType )
	{
		uint32_t unViewConfigNum = 0;
		m_vecConfigViews.clear();

		// Check if session was initialized correctly
		*outResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( *outResult ) )
			return m_vecConfigViews;

		// Get number of configuration views for this session
		*outResult = xrEnumerateViewConfigurationViews( m_pInstance->xrInstance, m_pInstance->xrSystemId, xrViewConfigType, unViewConfigNum, &unViewConfigNum, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( *outResult ) )
		{
			// Update view config cache
			m_vecConfigViews.resize( unViewConfigNum, { XR_TYPE_VIEW_CONFIGURATION_VIEW } );
			*outResult = xrEnumerateViewConfigurationViews( m_pInstance->xrInstance, m_pInstance->xrSystemId, xrViewConfigType, unViewConfigNum, &unViewConfigNum, m_vecConfigViews.data() );
		}

		return m_vecConfigViews;
	}

	bool Session::IsDepthFormat( VkFormat vkFormat )
	{
		if ( vkFormat == VK_FORMAT_D16_UNORM || vkFormat == VK_FORMAT_X8_D24_UNORM_PACK32 || vkFormat == VK_FORMAT_D32_SFLOAT || vkFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
			 vkFormat == VK_FORMAT_D24_UNORM_S8_UINT || vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT )
		{
			return true;
		}

		return false;
	}

	XrResult Session::GetSupportedTextureFormats( std::vector< int64_t > &outSupportedFormats )
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Clear out vector
		outSupportedFormats.clear();

		// Select a swapchain format.
		uint32_t unSwapchainFormatNum = 0;
		xrResult = xrEnumerateSwapchainFormats( m_xrSession, unSwapchainFormatNum, &unSwapchainFormatNum, nullptr );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			outSupportedFormats.resize( unSwapchainFormatNum, 0 );
			xrResult = xrEnumerateSwapchainFormats( m_xrSession, unSwapchainFormatNum, &unSwapchainFormatNum, outSupportedFormats.data() );

			// Log supported swapchain formats
			if ( m_eMinLogLevel == ELogLevel::LogDebug && XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				oxr::LogDebug( m_sLogCategory, "Runtime supports the following color formats: " );
				for ( auto supportedTextureFormat : outSupportedFormats )
				{
					if ( !IsDepthFormat( ( VkFormat )supportedTextureFormat ) )
					{
						oxr::LogDebug( m_sLogCategory, "\t%i", supportedTextureFormat );
					}
				}

				oxr::LogDebug( m_sLogCategory, "Runtime supports the following depth formats: " );
				for ( auto supportedTextureFormat : outSupportedFormats )
				{
					if ( IsDepthFormat( ( VkFormat )supportedTextureFormat ) )
					{
						oxr::LogDebug( m_sLogCategory, "\t%i", supportedTextureFormat );
					}
				}
			}
		}

		return xrResult;
	}

	VkFormat Session::SelectTextureFormat( std::vector< int64_t > &vecSupportedFormats, const std::vector< int64_t > &vecRequestedFormats, bool bIsDepth )
	{
		// If there are no requested texture formats, choose the first one from the runtime
		if ( vecRequestedFormats.empty() )
		{
			// Get first format
			for ( auto textureFormat : vecSupportedFormats )
			{
				VkFormat vkFormat = ( VkFormat )textureFormat;
				if ( IsDepthFormat( vkFormat ) == bIsDepth )
				{
					return vkFormat;
				}
			}
		}
		else
		{
			// Find matching texture format with runtime's supported ones
			for ( auto supportedFormat : vecSupportedFormats )
			{
				for ( auto &requestedFormat : vecRequestedFormats )
				{
					// Return selected format if a match is found
					if ( requestedFormat == supportedFormat )
						return ( VkFormat )supportedFormat;
				}
			}
		}

		return VK_FORMAT_UNDEFINED;
	}

	XrResult Session::CreateSwapchainImages( bool bIsDepth )
	{
		assert( !m_vecSwapchains.empty() );

		// Get appropriate openxr swapchain (color vs depth)
		XrSwapchain xrSwapchain = bIsDepth ? m_vecSwapchains.back().xrDepthSwapchain : m_vecSwapchains.back().xrColorSwapchain;

		// Determine number of swapchain images generated by the runtime
		uint32_t unNumOfSwapchainImages = 0;
		XrResult xrResult = xrEnumerateSwapchainImages( xrSwapchain, unNumOfSwapchainImages, &unNumOfSwapchainImages, nullptr );

		if ( xrResult != XR_SUCCESS )
			return xrResult;

		// Create swapchain images
		if ( bIsDepth )
		{
			m_vecSwapchains.back().vecDepthTextures.clear();
			m_vecSwapchains.back().vecDepthTextures.resize( unNumOfSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR } );
		}
		else
		{
			m_vecSwapchains.back().vecColorTextures.clear();
			m_vecSwapchains.back().vecColorTextures.resize( unNumOfSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR } );
		}

		xrResult = xrEnumerateSwapchainImages(
			xrSwapchain,
			unNumOfSwapchainImages,
			&unNumOfSwapchainImages,
			reinterpret_cast< XrSwapchainImageBaseHeader * >( bIsDepth ? m_vecSwapchains.back().vecDepthTextures.data() : m_vecSwapchains.back().vecColorTextures.data() ) );

		if ( xrResult != XR_SUCCESS )
			return xrResult;

		// Log swapchain creation (debug only)
		if ( CheckLogLevelDebug( m_eMinLogLevel ) )
		{
			oxr::LogDebug( m_sLogCategory, "%s swapchain created with %d images/textures.", bIsDepth ? "Depth" : "Color", unNumOfSwapchainImages );
		}

		return XR_SUCCESS;
	}

	XrResult Session::CreateSwapchains(
		TextureFormats *outSelectedTextureFormats,
		const std::vector< int64_t > &vecRequestedColorFormats,
		const std::vector< int64_t > &vecRequestedDepthFormats,
		uint32_t unWidth /** 0: will use recommended */,
		uint32_t unHeight /** 0: will use recommended */,
		XrViewConfigurationType xrViewConfigType /*= XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO*/,
		uint32_t unSwapchainCount /**  0: will match num of views. can use 1 for single pass*/,
		uint32_t unArraysize /** 1: e.g. you can have one swapchain for both eyes in vr using a value of 2 here */,
		uint32_t unSampleCount /** 0: will use recommended */,
		uint32_t unFaceCount /** 1: min */,
		uint32_t unMipCount /** 1: min */ )
	{
		// Check if session was initialized correctly
		XrResult xrResult = CheckIfInitCalled();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		// Reset output texture formats
		outSelectedTextureFormats->vkColorTextureFormat = VK_FORMAT_UNDEFINED;
		outSelectedTextureFormats->vkDepthTextureFormat = VK_FORMAT_UNDEFINED;

		// (1) Get runtime's supported texture formats to check against the app's requested texture formats
		std::vector< int64_t > vecSupportedTextureFormats;
		xrResult = GetSupportedTextureFormats( vecSupportedTextureFormats );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			return xrResult;

		if ( vecSupportedTextureFormats.empty() )
			return XR_ERROR_RUNTIME_FAILURE;

		// Select color format
		outSelectedTextureFormats->vkColorTextureFormat = SelectTextureFormat( vecSupportedTextureFormats, vecRequestedColorFormats, false );
		if ( outSelectedTextureFormats->vkColorTextureFormat == VK_FORMAT_UNDEFINED )
		{
			oxr::LogError( m_sLogCategory, "Unable to negotiate a requested color texture format with the runtime." );
			return XR_ERROR_RUNTIME_FAILURE;
		}

		if ( CheckLogLevelDebug( m_eMinLogLevel ) )
		{
			oxr::LogDebug( m_sLogCategory, "Color format (%" PRIu64 ") selected.", ( int64_t )outSelectedTextureFormats->vkColorTextureFormat );
		}

		// Select depth format
		outSelectedTextureFormats->vkDepthTextureFormat = SelectTextureFormat( vecSupportedTextureFormats, vecRequestedDepthFormats, true );
		if ( outSelectedTextureFormats->vkDepthTextureFormat == VK_FORMAT_UNDEFINED )
		{
			oxr::LogError( m_sLogCategory, "Unable to negotiate a requested depth texture format with the runtime." );
			return XR_ERROR_RUNTIME_FAILURE;
		}

		if ( CheckLogLevelDebug( m_eMinLogLevel ) )
		{
			oxr::LogDebug( m_sLogCategory, "Depth format (%" PRIu64 ") selected.", ( int64_t )outSelectedTextureFormats->vkDepthTextureFormat );
		}

		// (2) Update the configuration view properties which holds the number of views
		//     for this session and the recommended and max texture rects
		uint32_t unConfigViewsNum = ( uint32_t )( UpdateConfigurationViews( &xrResult, xrViewConfigType ) ).size();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) || unConfigViewsNum < 1 )
		{
			oxr::LogError( m_sLogCategory, "Fatal error. No configuration views for selected configuration type (%s) is supported by the active runtime.", XrEnumToString( xrViewConfigType ) );
			return xrResult;
		}

		// (3) Create views - e.g. for vr, there's one for each eye
		m_vecViews.resize( unConfigViewsNum, { XR_TYPE_VIEW } );

		// Check if app specified number of swapchains to generate, otherwise match with views
		uint32_t unSwapchainsNum = ( unSwapchainCount == 0 || unSwapchainCount > unConfigViewsNum ) ? unConfigViewsNum : unSwapchainCount;

		m_vecSwapchains.clear();
		for ( uint32_t i = 0; i < unSwapchainsNum; i++ )
		{
			// Determine number of textures to use for this swapchain
			uint32_t unSamplesNum = ( unSampleCount == 0 || unSampleCount > m_vecConfigViews[ i ].maxSwapchainSampleCount ) ? m_vecConfigViews[ i ].recommendedSwapchainSampleCount : unSampleCount;

			// (4) Create internal provider swapchain - this holds both openxr color and if supported, depth swapchains
			Swapchain providerSwapchain;
			providerSwapchain.vulkanTextureFormats.vkColorTextureFormat = outSelectedTextureFormats->vkColorTextureFormat;
			providerSwapchain.vulkanTextureFormats.vkDepthTextureFormat = outSelectedTextureFormats->vkDepthTextureFormat;

			providerSwapchain.unWidth = unWidth == 0 ? m_vecConfigViews[ i ].recommendedImageRectWidth : unWidth;
			providerSwapchain.unHeight = unHeight == 0 ? m_vecConfigViews[ i ].recommendedImageRectHeight : unHeight;

			// (5) Set general openxr swapchain info
			XrSwapchainCreateInfo xrSwapchainCreateInfo { XR_TYPE_SWAPCHAIN_CREATE_INFO };
			xrSwapchainCreateInfo.arraySize = unArraysize;
			xrSwapchainCreateInfo.width = providerSwapchain.unWidth;
			xrSwapchainCreateInfo.height = providerSwapchain.unHeight;
			xrSwapchainCreateInfo.mipCount = unMipCount;
			xrSwapchainCreateInfo.faceCount = unFaceCount;
			xrSwapchainCreateInfo.sampleCount = unSamplesNum;

			// (6) Create openxr swapchain - color
			xrSwapchainCreateInfo.format = providerSwapchain.vulkanTextureFormats.vkColorTextureFormat;
			xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

			xrResult = xrCreateSwapchain( m_xrSession, &xrSwapchainCreateInfo, &providerSwapchain.xrColorSwapchain );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;

			// (7) Create openxr swapchain - depth
			xrSwapchainCreateInfo.format = providerSwapchain.vulkanTextureFormats.vkDepthTextureFormat;
			xrSwapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			xrResult = xrCreateSwapchain( m_xrSession, &xrSwapchainCreateInfo, &providerSwapchain.xrDepthSwapchain );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;

			// (8) Add internal provider swapchain to cache
			m_vecSwapchains.push_back( providerSwapchain );

			if ( CheckLogLevelDebug( m_eMinLogLevel ) )
			{
				oxr::LogDebug(
					m_sLogCategory,
					"Color swapchain[%d] created: format (%" PRIu64 "), array size (%d), width (%d), height (%d)",
					i,
					( int64_t )outSelectedTextureFormats->vkColorTextureFormat,
					unArraysize,
					xrSwapchainCreateInfo.width,
					xrSwapchainCreateInfo.height );

				oxr::LogDebug(
					m_sLogCategory,
					"Depth swapchain[%d] created: format (%" PRIu64 "), array size (%d), width (%d), height (%d)",
					i,
					( int64_t )outSelectedTextureFormats->vkDepthTextureFormat,
					unArraysize,
					xrSwapchainCreateInfo.width,
					xrSwapchainCreateInfo.height );
			}

			// (9) Create swapchain images/textures - color
			xrResult = CreateSwapchainImages( false );
			if ( xrResult != XR_SUCCESS )
			{
				oxr::LogError( m_sLogCategory, "Unable to create color swapchain images (%s)", XrEnumToString( xrResult ) );
				return xrResult;
			}

			// (10) Create swapchain images/textures - depth
			xrResult = CreateSwapchainImages( true );
			if ( xrResult != XR_SUCCESS )
			{
				oxr::LogError( m_sLogCategory, "Unable to create depth swapchain images (%s)", XrEnumToString( xrResult ) );
				return xrResult;
			}
		}

		return xrResult;
	}

	void Session::RenderFrame(
		std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews,
		XrFrameState *pFrameState,
		XrCompositionLayerFlags xrCompositionLayerFlags,
		XrEnvironmentBlendMode xrEnvironmentBlendMode,
		XrOffset2Di xrRectOffset,
		XrExtent2Di xrRectExtent,
		bool bIsarray,
		uint32_t unArrayIndex )
	{
		std::vector< XrCompositionLayerBaseHeader * > xrFrameLayers;
		RenderFrameWithLayers( vecFrameLayerProjectionViews, xrFrameLayers, pFrameState, xrCompositionLayerFlags, xrEnvironmentBlendMode, xrRectOffset, xrRectExtent, bIsarray, unArrayIndex );
	}

	void Session::RenderFrameWithLayers(
		std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews,
		std::vector< XrCompositionLayerBaseHeader * > &vecFrameLayers,
		XrFrameState *pFrameState,
		XrCompositionLayerFlags xrCompositionLayerFlags /*= 0 */,
		XrEnvironmentBlendMode xrEnvironmentBlendMode /*= XR_ENVIRONMENT_BLEND_MODE_OPAQUE*/,
		/* vr */ XrOffset2Di xrRectOffset /*= { 0, 0 }*/,
		XrExtent2Di xrRectExtent /*= { 0, 0 }*/,
		bool bIsarray /*= false*/,
		uint32_t unArrayIndex /*= 0 */ )
	{
		// Check if there's a valid session and swapchains to work with
		if ( m_xrSession == XR_NULL_HANDLE || m_vecSwapchains.empty() )
			return;

		// (1) Wait for a new frame
		XrFrameWaitInfo xrWaitFrameInfo { XR_TYPE_FRAME_WAIT_INFO };

		if ( xrWaitFrame( m_xrSession, &xrWaitFrameInfo, pFrameState ) != XR_SUCCESS )
			return;

		// Cache predicted time and period
		m_xrPredictedDisplayTime = pFrameState->predictedDisplayTime;
		m_xrPredictedDisplayPeriod = pFrameState->predictedDisplayPeriod;

		// (2) Begin frame before doing any GPU work
		XrFrameBeginInfo xrBeginFrameInfo { XR_TYPE_FRAME_BEGIN_INFO };
		if ( xrBeginFrame( m_xrSession, &xrBeginFrameInfo ) != XR_SUCCESS )
			return;

		XrCompositionLayerProjection xrFrameLayerProjection { XR_TYPE_COMPOSITION_LAYER_PROJECTION };

		if ( pFrameState->shouldRender )
		{
			XrResult xrResult = XR_SUCCESS;

			// (3) Get space and time information for this frame
			XrViewLocateInfo xrFrameSpaceTimeInfo { XR_TYPE_VIEW_LOCATE_INFO };
			xrFrameSpaceTimeInfo.displayTime = pFrameState->predictedDisplayTime;
			xrFrameSpaceTimeInfo.space = m_xrReferenceSpace;
			xrFrameSpaceTimeInfo.viewConfigurationType = m_xrViewConfigurationType;

			XrViewState xrFrameViewState { XR_TYPE_VIEW_STATE };
			uint32_t nFoundViewsCount;
			xrResult = xrLocateViews( m_xrSession, &xrFrameSpaceTimeInfo, &xrFrameViewState, ( uint32_t )m_vecViews.size(), &nFoundViewsCount, m_vecViews.data() );
			if ( xrResult != XR_SUCCESS )
			{
				return;
			}

			// (4) Grab images from swapchain and render - must at least have orientation tracking
			if ( xrFrameViewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT )
			{
				for ( uint32_t i = 0; i < m_vecSwapchains.size(); i++ )
				{
					// (4.1) Acquire swapchain image
					const XrSwapchain xrSwapchain = m_vecSwapchains[ i ].xrColorSwapchain;
					XrSwapchainImageAcquireInfo xrAcquireInfo { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
					uint32_t unImageIndex;
					if ( xrAcquireSwapchainImage( xrSwapchain, &xrAcquireInfo, &unImageIndex ) != XR_SUCCESS )
						return;

					// (4.2) Let apps build command buffers via their registered callbacks
					ExecuteRenderImageCallbacks( m_vecAcquireSwapchainImageCallbacks, i, unImageIndex );

					// (4.3) Wait for swapchain image
					XrSwapchainImageWaitInfo xrWaitInfo { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
					xrWaitInfo.timeout = XR_INFINITE_DURATION;
					if ( xrWaitSwapchainImage( xrSwapchain, &xrWaitInfo ) != XR_SUCCESS )
						return;

					// (4.4) Add projection view to swapchain image
					vecFrameLayerProjectionViews[ i ] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
					vecFrameLayerProjectionViews[ i ].pose = m_vecViews[ i ].pose;
					vecFrameLayerProjectionViews[ i ].fov = m_vecViews[ i ].fov;
					vecFrameLayerProjectionViews[ i ].subImage.swapchain = xrSwapchain;
					vecFrameLayerProjectionViews[ i ].subImage.imageArrayIndex = bIsarray ? unArrayIndex : 0;
					vecFrameLayerProjectionViews[ i ].subImage.imageRect.offset = xrRectOffset;
					vecFrameLayerProjectionViews[ i ].subImage.imageRect.extent = {
						xrRectExtent.width == 0 ? m_vecSwapchains[ i ].unWidth : xrRectExtent.width, xrRectExtent.height == 0 ? m_vecSwapchains[ i ].unHeight : xrRectExtent.height };

					// (4.5) Depth handling
					if ( m_bDepthHandling )
					{
						XrCompositionLayerDepthInfoKHR xrDepthInfo { XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR };
						xrDepthInfo.subImage.swapchain = m_vecSwapchains[ i ].xrDepthSwapchain;
						xrDepthInfo.subImage.imageArrayIndex = 0;
						xrDepthInfo.subImage.imageRect.offset = { 0, 0 };
						xrDepthInfo.subImage.imageRect.extent = vecFrameLayerProjectionViews[ i ].subImage.imageRect.extent;
						xrDepthInfo.minDepth = 0.0f;
						xrDepthInfo.maxDepth = 1.0f;
						xrDepthInfo.nearZ = 0.1f;
						xrDepthInfo.farZ = FLT_MAX;

						vecFrameLayerProjectionViews[ i ].next = &xrDepthInfo;
					}

					// (4.6) Let apps render to textures via their registered callbacks
					ExecuteRenderImageCallbacks( m_vecWaitSwapchainImageCallbacks, i, unImageIndex );

					// (4.7) Release swapchain image
					XrSwapchainImageReleaseInfo xrSwapChainRleaseInfo { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
					if ( xrReleaseSwapchainImage( xrSwapchain, &xrSwapChainRleaseInfo ) != XR_SUCCESS )
						return;

					// (4.8) Let apps do any internal cleanups via their registered callbacks
					ExecuteRenderImageCallbacks( m_vecReleaseSwapchainImageCallbacks, i, unImageIndex );
				}

				// (4.9) Assemble projection layer
				xrFrameLayerProjection.space = m_xrAppSpace;
				xrFrameLayerProjection.layerFlags = xrCompositionLayerFlags;
				xrFrameLayerProjection.viewCount = ( uint32_t )vecFrameLayerProjectionViews.size();
				xrFrameLayerProjection.views = vecFrameLayerProjectionViews.data();

				vecFrameLayers.push_back( reinterpret_cast< XrCompositionLayerBaseHeader * >( &xrFrameLayerProjection ) );
			}
		}

		// (5) End current frame

		XrFrameEndInfo xrEndFrameInfo { XR_TYPE_FRAME_END_INFO };
		xrEndFrameInfo.displayTime = pFrameState->predictedDisplayTime;
		xrEndFrameInfo.environmentBlendMode = xrEnvironmentBlendMode;
		xrEndFrameInfo.layerCount = ( uint32_t )vecFrameLayers.size();
		xrEndFrameInfo.layers = vecFrameLayers.data();

		xrEndFrame( m_xrSession, &xrEndFrameInfo );
	}

	void Session::RenderHeadlessFrame( XrFrameState *pFrameState )
	{
		// Check if there's a valid session and swapchains to work with
		if ( m_xrSession == XR_NULL_HANDLE )
			return;

		// (1) Wait for a new frame
		XrFrameWaitInfo xrWaitFrameInfo { XR_TYPE_FRAME_WAIT_INFO };

		if ( xrWaitFrame( m_xrSession, &xrWaitFrameInfo, pFrameState ) != XR_SUCCESS )
			return;

		// Cache predicted time and period
		m_xrPredictedDisplayTime = pFrameState->predictedDisplayTime;
		m_xrPredictedDisplayPeriod = pFrameState->predictedDisplayPeriod;

		// (2) Begin frame before doing any GPU work
		XrFrameBeginInfo xrBeginFrameInfo { XR_TYPE_FRAME_BEGIN_INFO };
		if ( xrBeginFrame( m_xrSession, &xrBeginFrameInfo ) != XR_SUCCESS )
			return;

		// (3) End current frame

		XrFrameEndInfo xrEndFrameInfo { XR_TYPE_FRAME_END_INFO };
		xrEndFrameInfo.displayTime = pFrameState->predictedDisplayTime;
		xrEndFrameInfo.layerCount = 0;

		xrEndFrame( m_xrSession, &xrEndFrameInfo );
	}

} // namespace oxr
