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

#pragma once

#include <assert.h>
#include <string>
#include <vector>

#include <provider/common.hpp>

#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_platform_defines.h"
#include "openxr/openxr_reflection.h"

#define LOG_CATEGORY_EXTFBPASSTHROUGH "ExtFBPassthrough"

namespace oxr
{
	class ExtFBPassthrough : public ExtBase
	{
	  public:
		// Passthrough styles/modes - they are exclusive to each other
		enum class EPassthroughMode
		{
			// Passthrough is stopped and not running
			EPassthroughMode_Stopped = 0,

			// Passthrough is started but no layers are active
			EPassthroughMode_Started = 1,

			// Full screen passthrough (colored if available)
			EPassthroughMode_Default = 2,

			// Full screen passthrough in mono (greyscale)
			EPassthroughMode_Mono = 3,

			// Full screen passthrough with rgba colors mapped to luminance from the feed (can be used to add tints)
			EPassthroughMode_ColorMapped = 4,

			// Full screen passthrough with controls for brightness, contrast and saturation
			EPassthroughMode_BCS = 5,

			EPassthroughMode_Max
		};

		/// <summary>
		/// Constructor - a valid active openxr instance and session is required. Session may or may not be running
		/// </summary>
		/// <param name="xrInstance">Handle of the active openxr instance</param>
		/// <param name="xrSession">Handle of the active openxr session - may or may not be running</param>
		ExtFBPassthrough( XrInstance xrInstance, XrSession xrSession );
		~ExtFBPassthrough();

		/// <summary>
		/// Initializes this extension. Creates all the required passthrough objects and layers. Call this before any other function in this class.
		/// </summary>
		/// <returns>Result from the openxr runtime</returns>
		XrResult Init();

		/// <summary>
		/// Start the passthrough - this is also automatically called if needed by all SetModeToXXX() functions
		/// </summary>
		/// <param name="bStartDefaultMode">After starting hte passthrough, also start the default mode (fullscreen and full clolour if available)</param>
		/// <returns>Result from the openxr runtime of attempting to start passthrough</returns>
		XrResult StartPassThrough( bool bStartDefaultMode = false );

		/// <summary>
		/// Stop the passthrough session, this will also automatically stop any passthrough layers that may be running
		/// </summary>
		/// <returns>Result from the openxr runtime of attempting to stop the passthrough or any running layers</returns>
		XrResult StopPassThrough();

		/// <summary>
		/// Stops an active passthrough layer - this is also called by StopPassThorough()
		/// </summary>
		/// <returns>Result from the openxr runtime of attempting tostop a passthrough layer</returns>
		XrResult PausePassThroughLayer();

		/// <summary>
		/// Change the opacity factor of the active passthrough layer
		/// </summary>
		/// <param name="fTextureOpacityFactor">Opacity factor 0..1</param>
		/// <returns>The result of changing the opacity factor of the active layer from the runtime</returns>
		XrResult SetPassThroughOpacityFactor( float fTextureOpacityFactor );

		/// <summary>
		/// Change the color of the edges in the passthrough feed. This is rendered on top of the original passthrough feed.
		/// </summary>
		/// <param name="xrEdgeColor">Color to use for the edges</param>
		/// <returns>Result of attempting to change the color of the passthrough edges from the runtime</returns>
		XrResult SetPassThroughEdgeColor( XrColor4f xrEdgeColor );

		/// <summary>
		/// Set the passthrough parameters - opacity factor and edge color of the passthrough feed. This is applied on top of the original passthrough feed.
		/// </summary>
		/// <param name="fTextureOpacityFactor">Opacity factor 0..1</param>
		/// <param name="xrEdgeColor">Color to use for the edges</param>
		/// <returns>The result of changing the passthrough parameters from the runtime</returns>
		XrResult SetPassThroughParams( float fTextureOpacityFactor, XrColor4f xrEdgeColor );

		/// <summary>
		/// Set the passthrough style to the most basic function - full screen and in color if available
		/// </summary>
		/// <returns>Result of changing hte passthrough style from the runtime</returns>
		XrResult SetModeToDefault();

		/// <summary>
		/// Maps the passthrough feed to mono (black & white) color space
		/// </summary>
		/// <returns>Result of attempting to change to mono color</returns>
		XrResult SetModeToMono();

		/// <summary>
		/// Maps luminance values of the original passthrough feed to rgba map, each channel can be defined and mixed as needed
		/// </summary>
		/// <param name="bRed">Whether to apply the map to the Red channel resulting in a red tint</param>
		/// <param name="bGreen">Whether to apply the map to the Green channel resulting in a green tint</param>
		/// <param name="bBlue">Whether to apply the map to the Blue channel resulting in a blue tint</param>
		/// <param name="fAlpha">Opcaity value 0..1</param>
		/// <returns>The result of changing the colormap of the original passthrough feed from the runtime</returns>
		XrResult SetModeToColorMap( bool bRed, bool bGreen, bool bBlue, float fAlpha = 1.0f );

		/// <summary>
		/// Sets the passthrough mode style to BrightnessContrastSaturation,
		/// allowing you change the brightness, contrast and saturation independently of one another.
		/// Note that the default values are the "neutral" values (i.e. no effect against default/basic style)
		/// </summary>
		/// <param name="fBrightness">Brightness adjustment value in the range[ -100, 100 ] Neutral is 0.0f</param>
		/// <param name="fContrast">Contrast adjustment value in the range[ 0, Infinity ] Neutral element is 1.0f</param>
		/// <param name="fSaturation">Saturation adjustment value in the range[ 0, Infinity ] Neutral element is 1.0f</param>
		/// <returns>Result mode change from the runtime</returns>
		XrResult SetModeToBCS( float fBrightness = 0.0f, float fContrast = 1.0f, float fSaturation = 1.0f );

		/// <summary>
		/// Retrieves the composition layer that has the passthrough layer/feed from this extension. Add as the base layer,
		/// your projection layer with your world rendering should normally be on top of this
		/// </summary>
		/// <returns>Pointer to the composition layer for the passthrough feed</returns>
		XrCompositionLayerPassthroughFB *GetCompositionLayer() { return &m_FBPassthroughCompositionLayer; }

	  private:
		// The active openxr instance handle
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// The active openxr session handle
		XrSession m_xrSession = XR_NULL_HANDLE;

		// Current passthrough mode in effect (see definition of EPassthroughMode)
		EPassthroughMode m_eCurrentMode = EPassthroughMode::EPassthroughMode_Stopped;

		// The main passthrough object which represents the passthrough feature
		XrPassthroughFB m_fbPassthrough = XR_NULL_HANDLE;

		// A passthrough layer (full screen)
		// todo support other types as projection on geometry, etc
		XrPassthroughLayerFB m_fbPassthroughLayer_FullScreen = XR_NULL_HANDLE;

		// The style for the passthrough. Holds opacity and edge color. Next ptt of the struct holds more advanced styling options
		// see EPassthroughMode
		XrPassthroughStyleFB m_fbPassthroughStyle { XR_TYPE_PASSTHROUGH_STYLE_FB };
		float clearColor[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.2f };

		// Pointer to the composition layer for the passthrough feed
		XrCompositionLayerPassthroughFB m_FBPassthroughCompositionLayer { XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB };

		// Below are all the new functions (pointers to them from the runtime) for this spec
		// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_FB_passthrough
		PFN_xrCreatePassthroughFB xrCreatePassthroughFB = nullptr;
		PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB = nullptr;
		PFN_xrPassthroughStartFB xrPassthroughStartFB = nullptr;
		PFN_xrPassthroughPauseFB xrPassthroughPauseFB = nullptr;
		PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB = nullptr;
		PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB = nullptr;
		PFN_xrPassthroughLayerSetStyleFB xrPassthroughLayerSetStyleFB = nullptr;
		PFN_xrPassthroughLayerPauseFB xrPassthroughLayerPauseFB = nullptr;
		PFN_xrPassthroughLayerResumeFB xrPassthroughLayerResumeFB = nullptr;
		PFN_xrCreateTriangleMeshFB xrCreateTriangleMeshFB = nullptr;
		PFN_xrDestroyTriangleMeshFB xrDestroyTriangleMeshFB = nullptr;
		PFN_xrTriangleMeshGetVertexBufferFB xrTriangleMeshGetVertexBufferFB = nullptr;
		PFN_xrTriangleMeshGetIndexBufferFB xrTriangleMeshGetIndexBufferFB = nullptr;
		PFN_xrTriangleMeshBeginUpdateFB xrTriangleMeshBeginUpdateFB = nullptr;
		PFN_xrTriangleMeshEndUpdateFB xrTriangleMeshEndUpdateFB = nullptr;
		PFN_xrCreateGeometryInstanceFB xrCreateGeometryInstanceFB = nullptr;
		PFN_xrDestroyGeometryInstanceFB xrDestroyGeometryInstanceFB = nullptr;
		PFN_xrGeometryInstanceSetTransformFB xrGeometryInstanceSetTransformFB = nullptr;
	};

} // namespace oxr
