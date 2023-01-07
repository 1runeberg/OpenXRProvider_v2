/* Copyright 2023 Charlotte Gore
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

#include <provider/ext_base.hpp>
#include <provider/interaction_profiles.hpp>
#include <provider/common.hpp>

#define LOG_CATEGORY_HTCXVIVETRACKERINTERACTION "HTCXViveTrackerInteraction"

namespace oxr
{
	class ExtHTCXViveTrackerInteraction : public oxr::ExtBase
	{
	  public:
		/// <summary>
		/// HTC Vive Tracker interaction extension - requires a valid openxr instance and session (may or may not be running)
		/// </summary>
		/// <param name="xrInstance">Active openxr instance</param>
		/// <param name="xrSession">Active openxr session (may or may not be running)</param>
		ExtHTCXViveTrackerInteraction( XrInstance xrInstance, XrSession xrSession );

		~ExtHTCXViveTrackerInteraction();

		/// <summary>
		/// Caches the main EnumerateViveTrackerPathsHTCX() function call from the runtime
		/// </summary>
		/// <returns>Result from the runtime</returns>
		XrResult Init();

	  private:
		// Active openxr instance
		XrInstance m_xrInstance = XR_NULL_HANDLE;

		// Active openxr session
		XrSession m_xrSession = XR_NULL_HANDLE;

		// Cached function pointer to the main EnumberViveTrackerPaths call from the active openxr runtime
		PFN_xrEnumerateViveTrackerPathsHTCX xrEnumerateViveTrackerPathsHTCX = nullptr;
	};

}
