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

// Add common headers - this includes openxr headers
#include "provider/common.hpp"
#include "provider/input.hpp"
#include "provider/session.hpp"

#define LOG_CATEGORY_PROVIDER "OpenXRProvider"

namespace oxr
{
	/// <summary>
	/// Information needed to create an openxr instance
	/// </summary>
	struct AppInstanceInfo
	{
		// The name of the application using the OpenXR Provider library
		std::string sAppName;

		// The version of the application using the OpenXR Provider library
		OxrVersion32 unAppVersion;

		// The name of the engine using the OpenXR Provider library
		std::string sEngineName;

		// The version of the engine using the OpenXR Provider library
		OxrVersion32 unEngineVersion;

		// List of extension names that the app wants to activate depending on runtime support
		std::vector< const char * > vecInstanceExtensions;

		// List of api layer names that the app wants to activate depending on runtime support
		std::vector< const char * > vecAPILayers;

		// Additional create info
		void *pvAdditionalCreateInfo = nullptr; // e.g. Android

		// OpenXr instance creation flags
		XrInstanceCreateFlags flgAdditionalCreateInfo = 0;
	};

	class Provider
	{
	  public:
		/// <summary>
		/// Main class to access this library's functions
		/// </summary>
		/// <param name="eMinLogLevel">Minimum severity level to log</param>
		Provider( ELogLevel eMinLogLevel = ELogLevel::LogWarning );
		~Provider();

#ifdef XR_USE_PLATFORM_ANDROID
		/// <summary>
		/// Initializes the openxr android loader, required call to get app info from the ndk/device
		/// </summary>
		/// <param name="app">Android app info from ndk/device - get this from android_main()</param>
		/// <returns></returns>
		XrResult InitAndroidLoader( struct android_app *app );
#endif

		/// <summary>
		/// Creates and openxr instance
		/// </summary>
		/// <param name="pAppInstanceInfo">Application information needed to create the openxr instance</param>
		/// <returns></returns>
		XrResult Init( const AppInstanceInfo *pAppInstanceInfo );

		/// <summary>
		/// Retrieve the openxr instance handle
		/// </summary>
		/// <returns>The openxr instance handle</returns>
		XrInstance GetOpenXrInstance() { return m_instance.xrInstance; }

		/// <summary>
		/// Retrieve the openxr instance properties
		/// </summary>
		/// <returns>The openxr isntance properties</returns>
		XrInstanceProperties GetOpenXrInstanceProperties() { return m_instance.xrInstanceProperties; }

		/// <summary>
		/// Retrieve the openxr system id
		/// </summary>
		/// <returns>The openxr system id</returns>
		XrSystemId GetOpenXrSystemId() { return m_instance.xrSystemId; }

		/// <summary>
		/// Retrieve the openxr system properties
		/// </summary>
		/// <returns>The openxr system properties</returns>
		XrSystemProperties GetOpenXrSystemProperties() { return m_instance.xrSystemProperties; }

		/// <summary>
		/// Checks a boolean result from a function call that doesn't return an XrResult ( e.g. bool foo(XrResult xrResult) {} ).
		/// Logs the XrResult if the boolean return value is false
		/// </summary>
		/// <param name="bTest">The boolean result from a fucntion call with XrResult as an out parameter</param>
		/// <param name="xrErrorIfFail">The XrError to log if failed (filled in by the function call as an out parameter)</param>
		/// <param name="pccErrorMsg">The error message to log</param>
		/// <returns></returns>
		XrResult CheckIfXrError( bool bTest, XrResult xrErrorIfFail, const char *pccErrorMsg );

		/// <summary>
		/// Check if the Init() function for this class has been called.
		/// </summary>
		/// <returns>Return an error as an XrResult and inform the app that it needs to call init first</returns>
		XrResult CheckIfInitCalled();

		/// <summary>
		/// Polls the openxr runtime for events, should be called once per frame
		/// </summary>
		/// <returns>Event packet from the openxr runtime (if any). E.g. change of ctrollers, session state changes, etc</returns>
		XrEventDataBaseHeader *PollXrEvents();

		/// <summary>
		/// Check if an api layer is enabled for the active openxr instance
		/// </summary>
		/// <param name="sApiLayerName"></param>
		/// <returns>True if the api layer is active in the instance, false otherwise</returns>
		bool IsApiLayerEnabled( std::string sApiLayerName ) { return FindStringInVector( m_vecEnabledApiLayers, sApiLayerName ); }

		/// <summary>
		/// Retrieves the api layers currently available in the user's system
		/// </summary>
		/// <param name="vecApiLayers">Vector of api layer property structs that will hold the result of the call</param>
		/// <returns>Whether the query operation was successful</returns>
		XrResult GetSupportedApiLayers( std::vector< XrApiLayerProperties > &vecApiLayers );

		/// <summary>
		/// Retrieves the api layers currently available in the user's system
		/// </summary>
		/// <param name="vecApiLayers">Vector of api layer names that will hold the result of the call</param>
		/// <returns>Whether the query operation was successful</returns>
		XrResult GetSupportedApiLayers( std::vector< std::string > &vecApiLayers );

		/// <summary>
		/// Checks if an extension is enabled for the active openxr instance
		/// </summary>
		/// <param name="sExtensionName">The name of the extension to check</param>
		/// <returns>True if the extension is enabled for the active openxr instance, false otherwise</returns>
		bool IsExtensionEnabled( std::string sExtensionName ) { return FindStringInVector( m_instance.vecEnabledExtensions, sExtensionName ); }

		/// <summary>
		/// Retrieves all the supported extensions by the currently active openxr runtime (e.g. SteamVR, Meta, etc)
		/// If an api layer name is provided, the extensions returned will be the extensions supported by the api layer
		/// </summary>
		/// <param name="vecExtensions">Vector of extension properties that will hold the supported by the currently active openxr runtime or api layer</param>
		/// <param name="pcharApiLayerName">Optionally, the name of the api layer if you want to check the extensinos supported by the specific api layer</param>
		/// <returns>Whether the query operation was successful</returns>
		XrResult GetSupportedExtensions( std::vector< XrExtensionProperties > &vecExtensions, const char *pcharApiLayerName = nullptr );

		/// <summary>
		/// Retrieves all the supported extensions by the currently active openxr runtime (e.g. SteamVR, Meta, etc)
		/// If an api layer name is provided, the extensions returned will be the extensions supported by the api layer
		/// </summary>
		/// <param name="vecExtensions">Vector of extension names that will hold the supported by the currently active openxr runtime or api layer</param>
		/// <param name="pcharApiLayerName">Optionally, the name of the api layer if you want to check the extensinos supported by the specific api layer</param>
		/// <returns>Whether the query operation was successful</returns>
		XrResult GetSupportedExtensions( std::vector< std::string > &vecExtensions, const char *pcharApiLayerName = nullptr );

		/// <summary>
		/// Checks the provided vector of extension properties and removes any that aren't supported
		/// </summary>
		/// <param name="vecRequestedExtensionProperties">The vector of extension properties to check</param>
		/// <returns>The result of the filter operation</returns>
		XrResult FilterForSupportedExtensions( std::vector< XrExtensionProperties > &vecRequestedExtensionProperties );

		/// <summary>
		/// Checks the provided vector of extension names and removes any that aren't supported
		/// </summary>
		/// <param name="vecRequestedExtensionProperties">The vector of extension names to check</param>
		/// <returns>The result of the filter operation</returns>
		XrResult FilterForSupportedExtensions( std::vector< std::string > &vecRequestedExtensionNames );

		/// <summary>
		/// Checks the provided vector of extension names and removes any that aren't supported
		/// </summary>
		/// <param name="vecRequestedExtensionProperties">The vector of extension names to check</param>
		/// <returns>The result of the filter operation</returns>
		XrResult FilterOutUnsupportedExtensions( std::vector< const char * > &vecExtensionNames );

		/// <summary>
		/// Checks the provided vector of api layer names and removes any that aren't supported
		/// </summary>
		/// <param name="vecRequestedExtensionProperties">The vector of api layer names to check</param>
		/// <returns>The result of the filter operation</returns>
		XrResult FilterOutUnsupportedApiLayers( std::vector< const char * > &vecApiLayerNames );

		/// <summary>
		/// Check what view configurations (e.g. stereo for vr) is supported by the currently active runtime
		/// </summary>
		/// <returns>A vector of view configuration types supported by the runtime</returns>
		std::vector< XrViewConfigurationType > GetSupportedViewConfigurations();

		/// <summary>
		/// Retrieves the currently active openxr vulkan extension version
		/// </summary>
		/// <returns>The currently active openxr vulkan extension version</returns>
		EVulkanExt GetCurrentVulkanExt() { return m_instance.eCurrentVulkanExt; };

		/// <summary>
		/// Required call to fill in the graphics requirement needed to start a session
		/// </summary>
		/// <param name="xrGraphicsRequirements">The vulkan graphics requirements struct to be fille in with hte ofno by the runtime</param>
		/// <returns>The result of this required operation/call</returns>
		XrResult GetVulkanGraphicsRequirements( XrGraphicsRequirementsVulkan2KHR *xrGraphicsRequirements );

		/// <summary>
		/// Create the vulkan instance. This is the vulkan instance that the application *must* use forits render operations
		/// </summary>
		/// <param name="xrVulkanInstanceCreateInfo">Appplication information required to create a vulkan instance</param>
		/// <param name="vkInstance">An output parameter that will hold the vulkan instance that hte application *must* use for its render operations</param>
		/// <param name="vkResult">The result of the vulkan instance creation by the runtime</param>
		/// <returns></returns>
		XrResult CreateVulkanInstance( const XrVulkanInstanceCreateInfoKHR *xrVulkanInstanceCreateInfo, VkInstance *vkInstance, VkResult *vkResult );

		/// <summary>
		/// Retrieve the vulkan physical device that the runtime has chose/is using. This *must* be the physical device that the app uses for its render operations.
		/// </summary>
		/// <param name="vkPhysicalDevice">Output parameter that will hold the vulkan physical device handle from this call</param>
		/// <param name="vkInstance">The vulkan instance from CreateVulkanInstance()</param>
		/// <returns>The result of retrieving the physical device used by the active openxr runtime</returns>
		XrResult GetVulkanGraphicsPhysicalDevice( VkPhysicalDevice *vkPhysicalDevice, VkInstance vkInstance );

		/// <summary>
		/// Create the vulkan logical device. This *must* be the logical device that the app uses for its renderign operations
		/// </summary>
		/// <param name="xrVulkanDeviceCreateInfo">App information required to create a vulkan logical device. Should include extensions that the app *requests*</param>
		/// <param name="vkPhysicalDevice">The vulkan physical device from GetVulkanGraphicsPhysicalDevice()</param>
		/// <param name="vkInstance">The vulkan instance from CreateVulkanInstance()</param>
		/// <param name="vkDevice">Output parameter that will hold the vulkan logical device</param>
		/// <param name="vkResult">The result of this create vulkan logical device operation</param>
		/// <returns></returns>
		XrResult CreateVulkanDevice( const XrVulkanDeviceCreateInfoKHR *xrVulkanDeviceCreateInfo, VkPhysicalDevice *vkPhysicalDevice, VkInstance *vkInstance, VkDevice *vkDevice, VkResult *vkResult );

		/// <summary>
		/// Creates an openxr session using graphics binding information provided by the app.
		/// Most information for in the graphics binding *must* have been returned by the openxr runtime (use convenience calls in this library)
		/// </summary>
		/// <param name="pGraphicsBindingVulkan">Graphics binding information required by the openxr runtime to create a session</param>
		/// <param name="flgAdditionalCreateInfo">Additional information for creating a session (optional)</param>
		/// <returns>The result of the openxr attempting to create a session</returns>
		XrResult CreateSession( XrGraphicsBindingVulkanKHR *pGraphicsBindingVulkan, XrSessionCreateFlags flgAdditionalCreateInfo = 0 );

		/// <summary>
		/// Creates an openxr session using full session create info provided by the app.
		/// Note that you need to put a XrGraphicsBindingVulkanKHR pointer in the *next* struct member
		///
		/// Most information for in the graphics binding *must* have been returned by the openxr runtime (use convenience calls in this library)
		/// Alternatively, use CreateSession(XrGraphicsBindingVulkanKHR* pGraphicsBindingVulkan, XrSessionCreateFlags flgAdditionalCreateInfo = 0)
		/// for convenience.
		/// </summary>
		/// <param name="pSessionCreateInfo">Openxr session create info struct. Add a ptr to XrGraphicsBindingVulkanKHR in the "next" member</param>
		/// <returns>The result of the openxr attempting to create a session</returns>
		XrResult CreateSession( XrSessionCreateInfo *pSessionCreateInfo );

		/// <summary>
		/// Retrieves a pointer to the shared openxr instance and and info
		/// </summary>
		/// <returns>The pointer to a native Instance struct that holds shared openxr instance state</returns>
		oxr::Instance *Instance() { return &m_instance; }

		/// <summary>
		/// Retrieves a pointer to the active Session class. Contains pertinent functions and state for openxr session handling.
		/// </summary>
		/// <returns>The pointer to the active Session class native to this library</returns>
		oxr::Session *Session();

		/// <summary>
		/// Retrieves a pointer to the active Input class. Contains pertinent functions and state for openxr input handling.
		/// </summary>
		/// <returns>The pointer to the active Input class native to this library</returns>
		oxr::Input *Input();

		/// <summary>
		/// Retrieves the extensions currently enabled in the active openxr instance
		/// </summary>
		/// <returns>Vector of enabled extension names in the openxr instance</returns>
		std::vector< std::string > GetEnabledExtensions() { return m_instance.vecEnabledExtensions; }

		/// <summary>
		/// Retrieves the api layers currently enabled in the active openxr instance
		/// </summary>
		/// <returns>Vector of enabled api layer names in the openxr instance</returns>
		std::vector< std::string > GetEnabledApiLayers() { return m_vecEnabledApiLayers; }

	  private:
		// Holds openxr instance state, including required android information
		oxr::Instance m_instance {};

		// Pointer to a native Session class that contains functions and state handling for an openxr session
		oxr::Session *m_pSession = nullptr;

		// Pointer to a native Input class that contains functions and state for openxr input handling
		oxr::Input *m_pInput = nullptr;

		// Latest event buffer for the last call to PollXrEvents()
		XrEventDataBuffer m_xrEventDataBuffer;

		// Cache of enabled api layers for the currently active openxr instance
		std::vector< std::string > m_vecEnabledApiLayers;

		// Current log severity level
		ELogLevel m_eMinLogLevel = ELogLevel::LogInfo;

		// Log category that will appear in log messages/console
		std::string m_sLogCategory = LOG_CATEGORY_PROVIDER;

#ifdef XR_USE_PLATFORM_ANDROID
		/// <summary>
		/// Internal function to initialize the android loader.
		/// This call actually initializes the android loader and caches pertinent information in the shared instance state
		/// </summary>
		/// <param name="app">The android app struct from android_main()</param>
		/// <returns>The result of the attempt to initialize the android loader for this openxr instance</returns>
		XrResult InitAndroidLoader_Internal( struct android_app *app );
#endif

		/// <summary>
		/// From a provided vector of api layer property structs, retrieves the names only of the api layers (i.e. sans the version info and other metadata)
		/// </summary>
		/// <param name="vecApilayerProperties">Vector of api layer properties to process</param>
		/// <param name="vecOutApiLayerNames">Output vector that will hold the api layer names</param>
		void GetApilayerNames( std::vector< XrApiLayerProperties > &vecApilayerProperties, std::vector< std::string > &vecOutApiLayerNames );

		/// <summary>
		/// From a provided vector of extension property structs, retrieves the names only of the extensions (i.e. sans the version info and other metadata)
		/// </summary>
		/// <param name="vecInExtensionProperties">Vector of extension property structs to process</param>
		/// <param name="vecOutExtensionNames">Output vector that will hold the names of the extensions</param>
		void GetExtensionNames( std::vector< XrExtensionProperties > &vecInExtensionProperties, std::vector< std::string > &vecOutExtensionNames );

		/// <summary>
		/// Retrieves the latest vulkan extension  from a list of supported graphics api extensions by the runtime
		/// </summary>
		/// <param name="vecSupportedExtensionNames">The extensions names supported by the currently active openxr runtime</param>
		/// <param name="sBestVulkanExt">Output parameter string that will hold the name of hte latest ulkan extension supported by the runtime</param>
		/// <returns></returns>
		XrResult GetBestVulkanExt( std::vector< std::string > &vecSupportedExtensionNames, std::string &sBestVulkanExt );

		/// <summary>
		/// Requests a openxr vulkan extension to use
		/// </summary>
		/// <param name="vecExtensionNames">Vulkan extension names to be checked in FIFO order</param>
		/// <returns></returns>
		bool SetVulkanExt( std::vector< const char * > &vecExtensionNames );

		/// <summary>
		/// From a given vector of extension names, remove any graphics api extensions
		/// This is used to ensure only vulkan is finally sent to the runtime
		/// </summary>
		/// <param name="vecExtensionNames">Vector of extensions names to process</param>
		void FilterOutUnsupportedGraphicsApis( std::vector< const char * > &vecExtensionNames );

		/// <summary>
		/// Utility function to find a specific string in a given vector of strings
		/// </summary>
		/// <param name="vecStrings">Vector of strings to check</param>
		/// <param name="s">The string to find in the provided string vector</param>
		/// <returns>True if the string is found, false otherwise</returns>
		bool FindStringInVector( std::vector< std::string > &vecStrings, std::string s );
	};

} // namespace oxr
