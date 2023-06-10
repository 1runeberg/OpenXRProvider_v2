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

#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_platform_defines.h"
#include "openxr/openxr_reflection.h"

// extension implementations
#include "ext_base.hpp"

// multi-vendor
#include "ext_handtracking.hpp"
#include "ext_eyegaze.hpp"

// vendor
#include "ext_fbpassthrough.hpp"	// meta only
#include "ext_fbrefreshrate.hpp"	// has wide runtime support (steamvr, meta, monado, wmr)
#include "ext_htcxtracker.hpp"		// steamvr, varjo, monado(?)

#define LOG_CATEGORY_EXT "OpenXRProvider-Ext"

namespace oxr
{
	class ExtVisMask : public ExtBase
	{
	  public:
		/// <summary>
		/// VisMask extension constructor - requires a valid XrInstance and XrSession
		/// </summary>
		/// <param name="xrInstance">Current running openxr instance</param>
		/// <param name="xrSession">Current openxr session - may or may not have started</param>
		ExtVisMask( XrInstance xrInstance, XrSession xrSession )
			: ExtBase( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME )
		{
			assert( xrInstance != XR_NULL_HANDLE );
			assert( xrSession != XR_NULL_HANDLE );

			m_xrInstance = xrInstance;
			m_xrSession = xrSession;
		}

		~ExtVisMask() {}

		/// <summary>
		/// Retrieves the visibility mask for a given view (e.g. one per eye)
		/// </summary>
		/// <param name="outVertices">Output parameter that will hold the vismask vertices</param>
		/// <param name="outIndices">Output parameter that will hold the vismask indices</param>
		/// <param name="xrViewConfigurationType">The view configuration type (e.g. STEREO for vr)</param>
		/// <param name="unViewIndex">The view index (e.g. 0 for the left eye, 1 for the right eye</param>
		/// <param name="xrVisibilityMaskType">The mask polygon type (e.g. line loop, visible tris, etc)</param>
		/// <returns>Result of retrieving the visibility mask</returns>
		XrResult GetVisMask(
			std::vector< XrVector2f > &outVertices,
			std::vector< uint32_t > &outIndices,
			XrViewConfigurationType xrViewConfigurationType,
			uint32_t unViewIndex,
			XrVisibilityMaskTypeKHR xrVisibilityMaskType );

	  private:
		// The current active openxr instance
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// The current active openxr session
		XrSession m_xrSession = XR_NULL_HANDLE;
	};

	struct ExtHandler
	{
		/// <summary>
		/// Get a pointer to an extension object if it's enabled in the active openxr instance
		/// </summary>
		/// <param name="sName">The name of the extension to look for</param>
		/// <returns>Pointer to the extension object - nullptr if not enabled or unsupport in the active openxr instance</returns>
		ExtBase *GetExtension( std::string sName )
		{
			for ( auto &extension : m_vecExtensions )
			{
				if ( extension->GetName() == sName )
					return extension;
			}

			return nullptr;
		}

		~ExtHandler();

		/// <summary>
		/// Creates a supported extension object and currently active/enabled in the instance
		/// </summary>
		/// <param name="xrInstance">Current active openxr instance</param>
		/// <param name="xrSession">Valid openxr session</param>
		/// <param name="extensionName">The extension name to create</param>
		/// <returns>True if the extension was created - this means the provider library has a native implementation of this extension</returns>
		bool AddExtension( XrInstance xrInstance, XrSession xrSession, const char *extensionName );

		/// <summary>
		/// Creates a supported extension object and currently active/enabled in the instance
		/// </summary>
		/// <param name="xrInstance">Current active openxr instance</param>
		/// <param name="extensionName">The extension name to create</param>
		/// <returns>True if the extension was created - this means the provider library has a native implementation of this extension</returns>
		bool AddExtension( XrInstance xrInstance, const char *extensionName );

	  private:
		// object cache of currently active and supported extensions in the current openxr instance
		std::vector< ExtBase * > m_vecExtensions;
	};

} // namespace oxr
