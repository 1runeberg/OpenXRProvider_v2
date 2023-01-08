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

#include <provider/ext_htcxvivetrackerinteraction.hpp>
#include <assert.h>

namespace oxr
{
	ExtHTCXViveTrackerInteraction::ExtHTCXViveTrackerInteraction( XrInstance xrInstance, XrSession xrSession )
		: ExtBase( XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME )
	{
		assert( xrInstance != XR_NULL_HANDLE );
		assert( xrSession != XR_NULL_HANDLE );

		m_xrInstance = xrInstance;
		m_xrSession = xrSession;
	}

	XrResult ExtHTCXViveTrackerInteraction::Init()
	{
		// Get function pointer to enumerate paths function in active runtime
		XrResult xrResult = xrGetInstanceProcAddr( m_xrInstance, "xrEnumerateViveTrackerPathsHTCX", ( PFN_xrVoidFunction * )&xrEnumerateViveTrackerPathsHTCX );
		return xrResult;
	}

	ExtHTCXViveTrackerInteraction::~ExtHTCXViveTrackerInteraction() { xrEnumerateViveTrackerPathsHTCX = nullptr; }
}


