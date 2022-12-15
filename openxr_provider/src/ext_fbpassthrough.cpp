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

#include <provider/ext_fbpassthrough.hpp>

namespace oxr
{

	ExtFBPassthrough::ExtFBPassthrough( XrInstance xrInstance, XrSession xrSession )
		: ExtBase( XR_FB_PASSTHROUGH_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		assert( xrSession != XR_NULL_HANDLE );

		m_xrInstance = xrInstance;
		m_xrSession = xrSession;

		// Initialize all function pointers
		INIT_PFN( m_xrInstance, xrCreatePassthroughFB );
		INIT_PFN( m_xrInstance, xrDestroyPassthroughFB );
		INIT_PFN( m_xrInstance, xrPassthroughStartFB );
		INIT_PFN( m_xrInstance, xrPassthroughPauseFB );
		INIT_PFN( m_xrInstance, xrCreatePassthroughLayerFB );
		INIT_PFN( m_xrInstance, xrDestroyPassthroughLayerFB );
		INIT_PFN( m_xrInstance, xrPassthroughLayerSetStyleFB );
		INIT_PFN( m_xrInstance, xrPassthroughLayerPauseFB );
		INIT_PFN( m_xrInstance, xrPassthroughLayerResumeFB );
		INIT_PFN( m_xrInstance, xrCreateTriangleMeshFB );
		INIT_PFN( m_xrInstance, xrDestroyTriangleMeshFB );
		INIT_PFN( m_xrInstance, xrTriangleMeshGetVertexBufferFB );
		INIT_PFN( m_xrInstance, xrTriangleMeshGetIndexBufferFB );
		INIT_PFN( m_xrInstance, xrTriangleMeshBeginUpdateFB );
		INIT_PFN( m_xrInstance, xrTriangleMeshEndUpdateFB );
		INIT_PFN( m_xrInstance, xrCreateGeometryInstanceFB );
		INIT_PFN( m_xrInstance, xrDestroyGeometryInstanceFB );
		INIT_PFN( m_xrInstance, xrGeometryInstanceSetTransformFB );
	}

	XrResult ExtFBPassthrough::Init()
	{
		// Create passthrough objects
		XrPassthroughCreateInfoFB xrPassthroughCI = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };
		XrResult xrResult = xrCreatePassthroughFB( m_xrSession, &xrPassthroughCI, &m_fbPassthrough );

		if ( XR_SUCCEEDED( xrResult ) )
		{
			// todo: support other passthrough types
			XrPassthroughLayerCreateInfoFB xrPassthroughLayerCI = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB };
			xrPassthroughLayerCI.passthrough = m_fbPassthrough;
			xrPassthroughLayerCI.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
			xrResult = xrCreatePassthroughLayerFB( m_xrSession, &xrPassthroughLayerCI, &m_fbPassthroughLayer_FullScreen );

			if ( !XR_SUCCEEDED( xrResult ) )
			{
				LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - unable to create a full screen passthrough layer: %s", XrEnumToString( xrResult ) );
				return xrResult;
			}
		}
		else
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to create fb passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Initialize passthrough composition layer
		m_FBPassthroughCompositionLayer.layerHandle = m_fbPassthroughLayer_FullScreen;
		m_FBPassthroughCompositionLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		m_FBPassthroughCompositionLayer.space = XR_NULL_HANDLE;

		return xrResult;
	}

	XrResult ExtFBPassthrough::StartPassThrough( bool bStartDefaultMode )
	{
		if ( m_eCurrentMode != EPassthroughMode::EPassthroughMode_Stopped )
			return XR_SUCCESS;

		// Start passthrough
		XrResult xrResult = xrPassthroughStartFB( m_fbPassthrough );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to start passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		m_eCurrentMode = EPassthroughMode::EPassthroughMode_Started;
		if ( !bStartDefaultMode )
			return xrResult;

		// Start default mode
		xrResult = SetModeToDefault();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to set mode to default while starting the passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// change mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_Default;
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::StopPassThrough()
	{
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
			return XR_SUCCESS;

		// pause passthrough layer
		XrResult xrResult = PausePassThroughLayer();
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to pause passthrough layer while stoppign passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// stop passthrough
		xrResult = xrPassthroughPauseFB( m_fbPassthrough );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to stop passthrough: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// change mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_Stopped;
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::PausePassThroughLayer()
	{
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
			return XR_SUCCESS;

		XrResult xrResult = xrPassthroughLayerPauseFB( m_fbPassthroughLayer_FullScreen );

		if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
			LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "Passthrough layer paused: %s", XrEnumToString( xrResult ) );
		else
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to pause passthrough layer: %s", XrEnumToString( xrResult ) );

		return xrResult;
	}

	XrResult ExtFBPassthrough::SetPassThroughOpacityFactor( float fTextureOpacityFactor )
	{
		m_fbPassthroughStyle.textureOpacityFactor = fTextureOpacityFactor;

		// Start passthrough layer
		XrResult xrResult = xrPassthroughLayerResumeFB( m_fbPassthroughLayer_FullScreen );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough parameter - opacity factor: %s", XrEnumToString( xrResult ) );
		}

		return xrResult;
	}

	XrResult ExtFBPassthrough::SetPassThroughEdgeColor( XrColor4f xrEdgeColor )
	{
		m_fbPassthroughStyle.edgeColor = xrEdgeColor;

		// Start passthrough layer
		XrResult xrResult = xrPassthroughLayerResumeFB( m_fbPassthroughLayer_FullScreen );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough parameter - edge color: %s", XrEnumToString( xrResult ) );
		}

		return xrResult;
	}

	XrResult ExtFBPassthrough::SetPassThroughParams( float fTextureOpacityFactor, XrColor4f xrEdgeColor )
	{
		m_fbPassthroughStyle.textureOpacityFactor = fTextureOpacityFactor;
		m_fbPassthroughStyle.edgeColor = xrEdgeColor;

		XrResult xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passthrough parameters: %s", XrEnumToString( xrResult ) );
		}

		return xrResult;
	}

	XrResult ExtFBPassthrough::SetModeToDefault()
	{
		XrResult xrResult = XR_SUCCESS;

		// Check if we're already in requested mode
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Default )
			return xrResult;

		// Start passthrough if it's not started
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
		{
			xrResult = StartPassThrough();

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Start passthrough layer
		xrResult = xrPassthroughLayerResumeFB(m_fbPassthroughLayer_FullScreen);
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Default
		m_fbPassthroughStyle.next = nullptr;
		m_fbPassthroughStyle.textureOpacityFactor = 1.0f;
		m_fbPassthroughStyle.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough mode to Default: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Change current mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_Default;
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::SetModeToMono()
	{
		XrResult xrResult = XR_SUCCESS;

		// Start passthrough if it's not started
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
		{
			xrResult = StartPassThrough();

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Start passthrough layer
		xrResult = xrPassthroughLayerResumeFB( m_fbPassthroughLayer_FullScreen );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Mono
		XrPassthroughColorMapMonoToMonoFB colorMap_Mono = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_MONO_FB };
		for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
		{
			uint8_t colorMono = i;
			colorMap_Mono.textureColorMap[ i ] = colorMono;
		}

		m_fbPassthroughStyle.textureOpacityFactor = 1.0f;
		m_fbPassthroughStyle.next = &colorMap_Mono;

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough mode to Mono: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Change current mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_Mono;
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::SetModeToColorMap( bool bRed, bool bGreen, bool bBlue, float fAlpha /*= 1.0f*/ )
	{
		XrResult xrResult = XR_SUCCESS;

		// Start passthrough if it's not started
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
		{
			xrResult = StartPassThrough();

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Start passthrough layer
		xrResult = xrPassthroughLayerResumeFB( m_fbPassthroughLayer_FullScreen );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Color Mapped
		XrPassthroughColorMapMonoToRgbaFB colorMap_RGBA = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_RGBA_FB };
		for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
		{
			float color_rgb = i / 255.0f;
			colorMap_RGBA.textureColorMap[ i ] = { bRed ? color_rgb : 0.0f, bGreen ? color_rgb : 0.0f, bBlue ? color_rgb : 0.0f, fAlpha };
		}

		m_fbPassthroughStyle.textureOpacityFactor = 1.0f;
		m_fbPassthroughStyle.next = &colorMap_RGBA;

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough mode to ColorMapped: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Change current mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_ColorMapped;
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::SetModeToBCS( float fBrightness /*= 0.0f*/, float fContrast /*= 1.0f*/, float fSaturation /*= 1.0f*/ )
	{
		XrResult xrResult = XR_SUCCESS;

		// Start passthrough if it's not started
		if ( m_eCurrentMode == EPassthroughMode::EPassthroughMode_Stopped )
		{
			xrResult = StartPassThrough();

			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
				return xrResult;
		}

		// Start passthrough layer
		xrResult = xrPassthroughLayerResumeFB( m_fbPassthroughLayer_FullScreen );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error starting passthrough layer: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// BCS - Brightness, Contrast and Saturation channels
		XrPassthroughBrightnessContrastSaturationFB bcs = { XR_TYPE_PASSTHROUGH_BRIGHTNESS_CONTRAST_SATURATION_FB };
		bcs.brightness = fBrightness;
		bcs.contrast = fContrast;
		bcs.saturation = fSaturation;

		m_fbPassthroughStyle.textureOpacityFactor = 1.0f;
		m_fbPassthroughStyle.next = &bcs;

		xrResult = xrPassthroughLayerSetStyleFB( m_fbPassthroughLayer_FullScreen, &m_fbPassthroughStyle );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error changing passhtrough mode to BCS: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Change current mode
		m_eCurrentMode = EPassthroughMode::EPassthroughMode_BCS;
		return XR_SUCCESS;
	}

} // namespace oxr
