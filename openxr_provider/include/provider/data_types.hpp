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

// Use Vulkan graphics api (this is the only supported graphics api)
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <inttypes.h>
#include <string>
#include <vector>

// Add android headers - must include before the openxr headers
#ifdef XR_USE_PLATFORM_ANDROID
	#include <android/asset_manager.h>
	#include <android/asset_manager_jni.h>
	#include <android/native_window.h>
	#include <android_native_app_glue.h>
	#include <jni.h>
	#include <sys/system_properties.h>
#endif

// OpenXR headers
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"
#include "openxr/openxr_platform_defines.h"
#include "openxr/openxr_reflection.h"

namespace oxr
{
	typedef uint32_t OxrVersion32;
	typedef uint64_t OxrHandleType;

	/// <summary>
	/// Severity levels for logging
	/// </summary>
	enum class ELogLevel
	{
		LogNone = 1,
		LogDebug = 2,
		LogVerbose = 3,
		LogInfo = 4,
		LogWarning = 5,
		LogError = 6,
		LogEMax
	};

	/// <summary>
	/// Vulkan extension versions supported by this library
	/// </summary>
	enum class EVulkanExt
	{
		VulkanExtNone = 1,
		VulkanExt1 = 2,
		VulkanExt2 = 3,
		VulkanExtEMax
	};

#ifdef XR_USE_PLATFORM_ANDROID
	struct AndroidAppState
	{
		ANativeWindow *NativeWindow = nullptr;
		bool Resumed = false;
	};
#endif

	/// <summary>
	/// Contains the OpenXR instance state
	/// </summary>
	struct Instance
	{
		XrInstance xrInstance = XR_NULL_HANDLE;
		XrSystemProperties xrSystemProperties { XR_TYPE_SYSTEM_PROPERTIES };

		XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
		XrInstanceProperties xrInstanceProperties { XR_TYPE_INSTANCE_PROPERTIES };

		EVulkanExt eCurrentVulkanExt = EVulkanExt::VulkanExtNone;
		bool bDepthHandling = false;

#ifdef XR_USE_PLATFORM_ANDROID
		JNIEnv *jniEnv = nullptr;
		struct android_app *androidApp;
		ANativeActivity *androidActivity = nullptr;
		AndroidAppState androidAppState = {};
#endif
	};
} // namespace oxr
