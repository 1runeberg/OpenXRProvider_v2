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

	XrResult ExtFBPassthrough::Init( XrSpace appSpace )
	{
		// Create passthrough objects
		XrPassthroughCreateInfoFB ptci = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };
		XrResult result = xrCreatePassthroughFB( m_xrSession, &ptci, &passthrough );


		if ( XR_SUCCEEDED( result ) )
		{
			XrPassthroughLayerCreateInfoFB plci = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB };
			plci.passthrough = passthrough;
			plci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
			result = xrCreatePassthroughLayerFB( m_xrSession, &plci, &reconPassthroughLayer );

			if (!XR_SUCCEEDED(result))
			{
				LogError(LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough - xrCreatePassthroughLayerFB: %s", XrEnumToString(result));
				return result;
			}
		}
		else
		{
			LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough - xrCreatePassthroughFB: %s", XrEnumToString( result ) );
			return result;
		}

		if ( XR_SUCCEEDED( result ) )
		{
			XrPassthroughLayerCreateInfoFB plci = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB };
			plci.passthrough = passthrough;
			plci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_PROJECTED_FB;
			result = xrCreatePassthroughLayerFB( m_xrSession, &plci, &geomPassthroughLayer );
			if (!XR_SUCCEEDED(result))
			{
				LogError(LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough - xrCreatePassthroughLayerFB: %s", XrEnumToString(result));
				return result;
			}
		}
		else
		{
			LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough - xrCreateGeometryInstanceFB: %s", XrEnumToString( result ) );
			return result;
		}

		if ( XR_UNQUALIFIED_SUCCESS( result ) )
		{
			LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough initialized: %s", XrEnumToString( result ) );
			return XR_SUCCESS;
		}

		LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Unable to initialize FB Passthrough: %s", XrEnumToString( result ) );
		return result;
	}

	XrResult ExtFBPassthrough::SetPassThroughStyle( EPassthroughMode eMode )
	{
		XrResult result = XR_SUCCESS;

		switch ( eMode )
		{
			case EPassthroughMode::EPassthroughMode_Basic:
				passthroughLayer = reconPassthroughLayer;
				result = xrPassthroughLayerResumeFB( passthroughLayer );
				style.textureOpacityFactor = 0.5f;
				style.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
				result = xrPassthroughLayerSetStyleFB( passthroughLayer, &style );
				break;

			case EPassthroughMode::EPassthroughMode_DynamicRamp:
				passthroughLayer = reconPassthroughLayer;
				xrPassthroughLayerResumeFB( passthroughLayer );
				style.textureOpacityFactor = 0.5f;
				style.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
				xrPassthroughLayerSetStyleFB( passthroughLayer, &style );
				break;

			case EPassthroughMode::EPassthroughMode_GreenRampYellowEdges:
			{
				passthroughLayer = reconPassthroughLayer;
				result = xrPassthroughLayerResumeFB( passthroughLayer );

				// Create a color map which maps each input value to a green ramp
				//XrPassthroughColorMapMonoToRgbaFB colorMap = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_RGBA_FB };
				//for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
				//{
				//	float colorValue = (i / 255.0f) * .5;
				//	colorMap.textureColorMap[ i ] = { colorValue, colorValue, colorValue, 1.0f };
				//}

				//style.textureOpacityFactor = 0.5f;
				//style.edgeColor = { 1.0f, 1.0f, 0.0f, 0.5f };
				//style.next = &colorMap;
				//result = xrPassthroughLayerSetStyleFB( passthroughLayer, &style );


				//XrPassthroughColorMapMonoToMonoFB colorMap = { XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_MONO_FB }; 
				//for ( int i = 0; i < XR_PASSTHROUGH_COLOR_MAP_MONO_SIZE_FB; ++i )
				//{
				//	uint8_t colorValue = i;
				//	colorMap.textureColorMap[ i ] = colorValue;
				//}

				XrPassthroughBrightnessContrastSaturationFB colorMap = { XR_TYPE_PASSTHROUGH_BRIGHTNESS_CONTRAST_SATURATION_FB };
				colorMap.brightness = 0.0f;
				colorMap.contrast = 1.0f;
				colorMap.saturation = 1.5f;

				style.textureOpacityFactor = 1.0f;
				//style.edgeColor = { 1.0f, 1.0f, 1.0f, 0.5f };
				style.next = &colorMap;
				result = xrPassthroughLayerSetStyleFB( passthroughLayer, &style );
			}
			break;

			case EPassthroughMode::EPassthroughMode_Masked:
				passthroughLayer = reconPassthroughLayer;
				result = xrPassthroughLayerResumeFB( passthroughLayer );

				clearColor[ 3 ] = 1.0f;
				style.textureOpacityFactor = 0.5f;
				style.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };

				result = xrPassthroughLayerSetStyleFB( passthroughLayer, &style );
				break;

			case EPassthroughMode::EPassthroughMode_ProjQuad:
				passthroughLayer = geomPassthroughLayer;
				result = xrPassthroughLayerResumeFB( passthroughLayer );
				break;

			case EPassthroughMode::EPassthroughMode_Stopped:
				result = xrPassthroughPauseFB( passthrough );
				break;

			default:
				break;
		}

		LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough - xrPassthroughLayerSetStyleFB: %s", XrEnumToString( result ) );

		// Initialize passthrough composition layer
		m_FBPassthroughCompositionLayer.layerHandle = passthroughLayer;
		m_FBPassthroughCompositionLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		m_FBPassthroughCompositionLayer.space = XR_NULL_HANDLE;

		StartPassThrough();

		return result;
	}

	XrResult ExtFBPassthrough::StartPassThrough() 
	{
		XrResult result = xrPassthroughStartFB(passthrough);

		if ( XR_UNQUALIFIED_SUCCESS( result ) )
			LogInfo( LOG_CATEGORY_EXTFBPASSTHROUGH, "FB Passthrough started: %s", XrEnumToString( result ) );
		else
			LogError( LOG_CATEGORY_EXTFBPASSTHROUGH, "Error - Unable to start passthrough: %s", XrEnumToString( result ) );

		return result;
	}

	void ExtFBPassthrough::SetPassThroughParams( float fTextureOpacityFactor, XrColor4f xrEdgeColor ) 
	{
		SetPassThroughOpacityFactor(fTextureOpacityFactor);
		SetPassThroughEdgeColor(xrEdgeColor);
	}

	XrResult ExtFBPassthrough::SetModeToDefault()
	{
		return XR_SUCCESS;
	}


	XrResult ExtFBPassthrough::SetModeToMono() 
	{
		return XR_SUCCESS;
	}

	XrResult ExtFBPassthrough::SetModeToColorMap( bool bRed, bool bGreen, bool bBlue, float fAlpha /*= 1.0f*/ ) 
	{
		return XR_SUCCESS;
	}

	

	XrResult ExtFBPassthrough::SetModeToBCS( float fBrightness /*= 0.0f*/, float fContrast /*= 1.0f*/, float fSaturation /*= 1.0f*/ ) 
	{
		return XR_SUCCESS;
	}

} // namespace oxr
