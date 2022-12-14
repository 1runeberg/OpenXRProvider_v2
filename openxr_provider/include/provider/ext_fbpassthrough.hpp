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

	enum class EPassthroughMode
	{
		EPassthroughMode_Basic = 0,
		EPassthroughMode_DynamicRamp = 1,
		EPassthroughMode_GreenRampYellowEdges = 2,
		EPassthroughMode_Masked = 3,
		EPassthroughMode_ProjQuad = 4,
		EPassthroughMode_Stopped = 5,
		EPassthroughMode_Max
	};

		ExtFBPassthrough(XrInstance xrInstance, XrSession xrSession);
	
		XrResult Init(XrSpace appSpace);

		XrResult SetPassThroughStyle(EPassthroughMode eMode);
		XrResult StartPassThrough();

		XrCompositionLayerPassthroughFB* GetCompositionLayer() { return &m_FBPassthroughCompositionLayer; }

	private:
		XrInstance m_xrInstance = XR_NULL_HANDLE;
		XrSession m_xrSession = XR_NULL_HANDLE;

		// function pointers for this spec
		PFN_xrCreatePassthroughFB  xrCreatePassthroughFB = nullptr;
		PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB = nullptr;
		PFN_xrPassthroughStartFB  xrPassthroughStartFB = nullptr;
		PFN_xrPassthroughPauseFB xrPassthroughPauseFB = nullptr;
		PFN_xrCreatePassthroughLayerFB  xrCreatePassthroughLayerFB = nullptr;
		PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB = nullptr;
		PFN_xrPassthroughLayerSetStyleFB xrPassthroughLayerSetStyleFB = nullptr;
		PFN_xrPassthroughLayerPauseFB xrPassthroughLayerPauseFB = nullptr;
		PFN_xrPassthroughLayerResumeFB  xrPassthroughLayerResumeFB = nullptr;
		PFN_xrCreateTriangleMeshFB xrCreateTriangleMeshFB = nullptr;
		PFN_xrDestroyTriangleMeshFB xrDestroyTriangleMeshFB = nullptr;
		PFN_xrTriangleMeshGetVertexBufferFB  xrTriangleMeshGetVertexBufferFB = nullptr;
		PFN_xrTriangleMeshGetIndexBufferFB xrTriangleMeshGetIndexBufferFB = nullptr;
		PFN_xrTriangleMeshBeginUpdateFB xrTriangleMeshBeginUpdateFB = nullptr;
		PFN_xrTriangleMeshEndUpdateFB xrTriangleMeshEndUpdateFB = nullptr;
		PFN_xrCreateGeometryInstanceFB xrCreateGeometryInstanceFB = nullptr;
		PFN_xrDestroyGeometryInstanceFB xrDestroyGeometryInstanceFB = nullptr;
		PFN_xrGeometryInstanceSetTransformFB xrGeometryInstanceSetTransformFB = nullptr;

		// objects for this spec
		XrPassthroughFB passthrough = XR_NULL_HANDLE;
		XrPassthroughLayerFB passthroughLayer = XR_NULL_HANDLE;
		XrPassthroughLayerFB reconPassthroughLayer = XR_NULL_HANDLE;
		XrPassthroughLayerFB geomPassthroughLayer = XR_NULL_HANDLE;
		//XrGeometryInstanceFB geomInstance = XR_NULL_HANDLE;

		// passthrough style
		XrPassthroughStyleFB style{XR_TYPE_PASSTHROUGH_STYLE_FB};
	    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.2f};

		// layers
		XrCompositionLayerPassthroughFB m_FBPassthroughCompositionLayer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
	};

} // namespace oxr
