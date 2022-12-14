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
 * Portions of this code are Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "data_types.hpp"
#include <future>

namespace xrvk
{
	static const uint32_t k_unCommandBufferNum = 2; // (double buffer)

	class Render
	{
	  public:
		// custom data structs
		enum PBRWorkflows
		{
			PBR_WORKFLOW_METALLIC_ROUGHNESS = 0,
			PBR_WORKFLOW_SPECULAR_GLOSINESS = 1
		};

		struct Textures
		{
			vks::TextureCubeMap environmentCube;
			vks::Texture2D empty;
			vks::Texture2D lutBrdf;
			vks::TextureCubeMap irradianceCube;
			vks::TextureCubeMap prefilteredCube;
		} textures;

		struct ShaderValuesParams
		{
			glm::vec4 lightDir;
			float exposure = 2.5f;
			float gamma = 2.2f;
			float prefilteredCubeMipLevels;
			float scaleIBLAmbient = 1.0f;
			float debugViewInputs = 0;
			float debugViewEquation = 0;
		} shaderValuesPbrParams;

		struct UniformBufferSet
		{
			Buffer scene;
			Buffer params;
		};
		Buffer skyboxUniformBuffer;
		std::vector< UniformBufferSet > vecUniformBuffers;

		struct VertexBufferSet
		{
			Buffer vertexBuffer;
			Buffer indexBuffer;
		};

		struct UBOMatrices
		{
			XrMatrix4x4f vp;
			XrMatrix4x4f model;
			XrVector3f eyePos;
		} uboMatricesScene, uboMatricesSkybox;

		struct DescriptorSetLayouts
		{
			VkDescriptorSetLayout scene;
			VkDescriptorSetLayout material;
			VkDescriptorSetLayout node;
		} descriptorSetLayouts;

		struct DescriptorSets
		{
			VkDescriptorSet scene;
			VkDescriptorSet skybox;
		};
		std::vector< DescriptorSets > vecDescriptorSets;

		struct PushConstBlockMaterial
		{
			glm::vec4 baseColorFactor;
			glm::vec4 emissiveFactor;
			glm::vec4 diffuseFactor;
			glm::vec4 specularFactor;
			float workflow;
			int colorTextureSet;
			int PhysicalDescriptorTextureSet;
			int normalTextureSet;
			int occlusionTextureSet;
			int emissiveTextureSet;
			float metallicFactor;
			float roughnessFactor;
			float alphaMask;
			float alphaMaskCutoff;
		} pushConstBlockMaterial;

		struct Pipelines
		{
			VkPipeline vismask;
			VkPipeline skybox;
			VkPipeline pbr;
			VkPipeline pbrDoubleSided;
			VkPipeline pbrAlphaBlend;
		} pipelines;

		struct VisMask
		{
			std::vector< XrVector2f > vertices;
			std::vector< uint32_t > indices;
		};

		// data
		int32_t nAnimationIndex = 0;
		float fAnimationTimer = 0.0f;
		bool bAnimate = true;

		VkExtent2D vkExtent {};
		VkDeviceSize vkDeviceSizeOffsets[ 1 ] = { 0 };
		VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
		VkPipeline vkBoundPipeline = VK_NULL_HANDLE;

		// pipeline layouts
		VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout vkPipelineLayoutVisMask = VK_NULL_HANDLE;

		// renderables
		RenderModel *skybox = nullptr;
		std::string skyboxTexture;

		std::vector< RenderScene * > vecRenderScenes;
		std::vector< RenderSector * > vecRenderSectors;
		std::vector< RenderModel * > vecRenderModels;

		// Constructors
#ifdef XR_USE_PLATFORM_ANDROID
		Render(
			AAssetManager *pAndroidAssetManager,
			ELogLevel eLogLevel = ELogLevel::LogDebug,
			bool bShowSkybox = true,
			std::string filenameSkyboxTex = "textures/papermill.ktx",
			std::string filenameSkyboxModel = "models/Box.glb",
			VkClearColorValue vkClearColorValue = {} );
#else
		Render(
			ELogLevel eLogLevel = ELogLevel::LogDebug,
			bool bShowSkybox = true,
			std::string filenameSkyboxTex = "textures/papermill.ktx",
			std::string filenameSkyboxModel = "models/Box.glb",
			VkClearColorValue vkClearColorValue = {} );
#endif
		~Render();

		// Initialize rendering
		XrResult Init( oxr::Provider *pProvider, const char *pccAppName, uint32_t unAppVersion, const char *pccEngineName, uint32_t unEngineVersion );

		// Initialize vulkan resources
		void CreateRenderResources( oxr::Session *pSession, int64_t nColorFormat, int64_t nDepthFormat, VkExtent2D vkExtent );

		// Rendering
		void BeginRender(
			oxr::Session *pSession,
			std::vector< XrCompositionLayerProjectionView > &vecFrameLayerProjectionViews,
			XrFrameState *pFrameState,
			uint32_t unSwapchainIndex,
			uint32_t unImageIndex,
			float fNearZ = 0.1f,
			float fFarZ = 100.0f,
			XrVector3f v3fScaleEyeView = { 1.0f, 1.0f, 1.0f } );

		void EndRender();
		void RenderNode( RenderSceneBase *renderable, vkglTF::Node *gltfNode, uint32_t unCmdBufIndex, vkglTF::Material::AlphaMode gltfAlphaMode );

		// Asset handling
		void LoadAssets();
		void PrepareAllPipelines();

		void LoadEnvironment( std::string sFilename );
		void GenerateCubemaps();
		void GenerateBRDFLUT();

		void PrepareUniformBuffers();
		void UpdateUniformBuffers();

		void SetupDescriptors();
		void SetupNodeDescriptorSet( vkglTF::Node *node );
		void PreparePipelines();

		// Renderables handling
		uint32_t AddRenderScene( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f } );
		uint32_t AddRenderSector( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f }, XrSpace xrSpace = XR_NULL_HANDLE );
		uint32_t AddRenderModel( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f }, XrSpace xrSpace = XR_NULL_HANDLE );

		void CreateVisMasks( uint32_t unNum );

		// getters and setters
		void SetCurrentLogLevel( ELogLevel eLogLevel ) { m_eMinLogLevel = eLogLevel; }
		void SetSkyboxVisibility( bool bNewVisibility );

		bool GetSkyboxVisibility();
		ELogLevel GetCurrentLogLevel() { return m_eMinLogLevel; }
		SharedState *GetSharedState() { return &m_SharedState; }
		std::vector< VisMask > &GetVisMasks() { return m_vecVisMasks; }

		XrGraphicsBindingVulkan2KHR *GetVulkanGraphicsBinding() { return &m_SharedState.xrGraphicsBinding; }

		// Vulkan debug logging
		static VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
			void *pUserData )
		{
			if ( messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
			{
				LogDebug( "[Vulkan Validation] %s", pCallbackData->pMessage );
			}

			return VK_FALSE;
		}

	  private:
		// general
		bool m_bShowSkybox = true;
		ELogLevel m_eMinLogLevel = ELogLevel::LogDebug;
		SharedState m_SharedState {};

		// internal
		std::vector< std::vector< RenderTarget > > m_vec2RenderTargets;
		std::vector< FrameData > m_vecFrameData {};
		std::vector< VkRenderPass > m_vecRenderPasses { VK_NULL_HANDLE };

		// openxr
		oxr::Provider *m_pProvider = nullptr;
		bool m_bEnableVismask = true;
		std::vector< VisMask > m_vecVisMasks;
		std::vector< VertexBufferSet > m_vecVisMaskBuffers;

		// vulkan
		const std::vector< const char * > m_vecValidationLayers;	 //= { "VK_LAYER_KHRONOS_validation" };
		const std::vector< const char * > m_vecValidationExtensions; //= { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

		// vks
		vks::VulkanDevice *m_pVulkanDevice = nullptr;
		std::map< std::string, std::string > mapEnvironments;

		// functions - render resources
		void CreateRenderPass( int64_t nColorFormat, int64_t nDepthFormat, uint32_t nIndex = 0 );
		void CreateRenderTargets( oxr::Session *pSession, VkRenderPass vkRenderPass );

		// functions - renderables
		void RenderGltfScene( RenderSceneBase *renderable );
		void RenderGltfScenes();

		void LoadGltfScene( RenderSceneBase *renderable );
		void LoadGltfScenes();

		void UpdateUniformBuffers( UBOMatrices *uboMatrices, Buffer *buffer, RenderSceneBase *renderable, XrMatrix4x4f *matViewProjection, XrPosef *eyePose );
		void UpdateUniformBuffers( UBOMatrices *uboMatrices, Buffer *buffer, XrMatrix4x4f *matViewProjection, XrPosef *eyePose );

		void UpdateRenderablePoses( oxr::Session *pSession, XrFrameState *pFrameState );

		// functions - utility
		void CalculateDescriptorScope( vkglTF::Model *gltfModel, uint32_t *imageSamplerCount, uint32_t *materialCount, uint32_t *meshCount );
		void AllocateDescriptorSet( vkglTF::Model *gltfModel );
	};

} // namespace xrvk