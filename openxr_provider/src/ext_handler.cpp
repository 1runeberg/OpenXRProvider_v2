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

#include <provider/common.hpp>
#include <provider/ext_handler.hpp>
#include <provider/log.hpp>

namespace oxr
{

	XrResult ExtVisMask::GetVisMask(
		std::vector< XrVector2f > &outVertices,
		std::vector< uint32_t > &outIndices,
		XrViewConfigurationType xrViewConfigurationType,
		uint32_t unViewIndex,
		XrVisibilityMaskTypeKHR xrVisibilityMaskType )
	{
		// Clear  output vectors
		outIndices.clear();
		outVertices.clear();

		// Get function pointer to retrieve vismask data from the runtime
		PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR = nullptr;
		XrResult xrResult = xrGetInstanceProcAddr( m_xrInstance, "xrGetVisibilityMaskKHR", ( PFN_xrVoidFunction * )&xrGetVisibilityMaskKHR );

		if ( xrResult != XR_SUCCESS )
		{
			LogDebug( LOG_CATEGORY_EXT, "Error retrieving vismask function from system: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Get index and vertex counts
		XrVisibilityMaskKHR xrVisibilityMask { XR_TYPE_VISIBILITY_MASK_KHR };
		xrVisibilityMask.indexCapacityInput = 0;
		xrVisibilityMask.vertexCapacityInput = 0;

		xrResult = xrGetVisibilityMaskKHR( m_xrSession, xrViewConfigurationType, unViewIndex, xrVisibilityMaskType, &xrVisibilityMask );

		if ( xrResult != XR_SUCCESS )
		{
			LogDebug( LOG_CATEGORY_EXT, "Error retrieving vismask counts: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		// Check vismask data
		uint32_t unVertexCount = xrVisibilityMask.vertexCountOutput;
		uint32_t unIndexCount = xrVisibilityMask.indexCountOutput;

		if ( unIndexCount == 0 && unVertexCount == 0 )
		{
			LogWarning( LOG_CATEGORY_EXT, "Warning - runtime doesn't have a visibility mask for this view configuration!" );
			return XR_SUCCESS;
		}

		// Reserve size for output vectors
		outVertices.resize( unVertexCount );
		outIndices.resize( unIndexCount );

		// Get mask vertices and indices from the runtime
		xrVisibilityMask.vertexCapacityInput = unVertexCount;
		xrVisibilityMask.indexCapacityInput = unIndexCount;
		xrVisibilityMask.indexCountOutput = 0;
		xrVisibilityMask.vertexCountOutput = 0;
		xrVisibilityMask.indices = outIndices.data();
		xrVisibilityMask.vertices = outVertices.data();

		xrResult = xrGetVisibilityMaskKHR( m_xrSession, xrViewConfigurationType, unViewIndex, xrVisibilityMaskType, &xrVisibilityMask );

		if ( xrResult != XR_SUCCESS )
		{
			LogDebug( LOG_CATEGORY_EXT, "Error retrieving vismask data from the runtime: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	ExtHandler::~ExtHandler()
	{
		for ( auto &ext : m_vecExtensions )
		{
			delete ext;
		}
	}

	bool ExtHandler::AddExtension( XrInstance xrInstance, XrSession xrSession, const char *extensionName )
	{
		// KHR: Visibility mask
		if ( strcmp( extensionName, XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtVisMask( xrInstance, xrSession ) );
			return true;
		}

		// EXT: Hand tracking
		if ( strcmp( extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtHandTracking( xrInstance, xrSession ) );
			return true;
		}

		// FB: Passthrough
		if ( strcmp( extensionName, XR_FB_PASSTHROUGH_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtFBPassthrough( xrInstance, xrSession ) );
			return true;
		}

		// FB: Display refresh rate
		if ( strcmp( extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtFBRefreshRate( xrInstance, xrSession ) );
			return true;
		}

		// HTCX: Vive tracker
		if ( strcmp( extensionName, XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtHTCXViveTrackerInteraction( xrInstance, xrSession ) );
		}

		return false;
	}

	bool ExtHandler::AddExtension( XrInstance xrInstance, const char *extensionName ) 
	{
		// EXT: Eye gaze tracking
		if ( strcmp( extensionName, XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME ) == 0 )
		{
			m_vecExtensions.push_back( new ExtEyeGaze( xrInstance ) );
			return true;
		}

		return false;
	}

} // namespace oxr
