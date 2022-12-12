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
 * 
 */

#pragma once

#include "log.hpp"

#define XR_USE_GRAPHICS_API_VULKAN 1
#include <vulkan/vulkan.h>

#include <openxr.h>
#include <openxr_platform.h>
#include <openxr_platform_defines.h>
#include <openxr_reflection.h>

// Maths
#include "openxr/xr_linear.h"

// glm, gli
#include <gli/gli.hpp>
#include <glm/glm.hpp>

// internal
#include "log.hpp"
#include "vulkanpbr/VulkanglTFModel.h"

namespace xrvk
{
	struct SharedState
	{
		XrGraphicsBindingVulkan2KHR xrGraphicsBinding { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };

		std::array< VkClearValue, 2 > vkClearValues;

		// Android specific
#ifdef XR_USE_PLATFORM_ANDROID
		AAssetManager *androidAssetManager = nullptr;
#endif
	};
} // namespace xrvk