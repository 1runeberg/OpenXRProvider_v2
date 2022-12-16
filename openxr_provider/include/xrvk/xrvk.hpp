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
 * Portions of this code Copyright (c) 2019-2022, The Khronos Group Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include "data_types.hpp"
#include <future>

namespace Shapes
{
	// Color constants
	constexpr XrVector3f Red { 1, 0, 0 };
	constexpr XrVector3f DarkRed { 0.25f, 0, 0 };
	constexpr XrVector3f Green { 0, 1, 0 };
	constexpr XrVector3f DarkGreen { 0, 0.25f, 0 };
	constexpr XrVector3f Blue { 0, 0, 1 };
	constexpr XrVector3f DarkBlue { 0, 0, 0.25f };

	// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
	constexpr XrVector3f LBB { -0.5f, -0.5f, -0.5f };
	constexpr XrVector3f LBF { -0.5f, -0.5f, 0.5f };
	constexpr XrVector3f LTB { -0.5f, 0.5f, -0.5f };
	constexpr XrVector3f LTF { -0.5f, 0.5f, 0.5f };
	constexpr XrVector3f RBB { 0.5f, -0.5f, -0.5f };
	constexpr XrVector3f RBF { 0.5f, -0.5f, 0.5f };
	constexpr XrVector3f RTB { 0.5f, 0.5f, -0.5f };
	constexpr XrVector3f RTF { 0.5f, 0.5f, 0.5f };

#define CUBE_SIDE( V1, V2, V3, V4, V5, V6, COLOR ) { V1, COLOR }, { V2, COLOR }, { V3, COLOR }, { V4, COLOR }, { V5, COLOR }, { V6, COLOR },

	struct Vertex
	{
		XrVector3f Position;
		XrVector3f Color;
	};

	// Cube indices
	std::vector< unsigned short > vecCubeIndices = {
		0,	1,	2,	3,	4,	5,	// -X
		6,	7,	8,	9,	10, 11, // +X
		12, 13, 14, 15, 16, 17, // -Y
		18, 19, 20, 21, 22, 23, // +Y
		24, 25, 26, 27, 28, 29, // -Z
		30, 31, 32, 33, 34, 35, // +Z
	};

	// Cube vertices
	std::vector< Shapes::Vertex > vecCubeVertices = {
		CUBE_SIDE( LTB, LBF, LBB, LTB, LTF, LBF, DarkRed )	 // -X
		CUBE_SIDE( RTB, RBB, RBF, RTB, RBF, RTF, Red )		 // +X
		CUBE_SIDE( LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen ) // -Y
		CUBE_SIDE( LTB, RTB, RTF, LTB, RTF, LTF, Green )	 // +Y
		CUBE_SIDE( LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue )	 // -Z
		CUBE_SIDE( LBF, LTF, RTF, LBF, RTF, RBF, Blue )		 // +Z
	};

	struct Shape
	{
		XrPosef pose;
		XrVector3f scale { 1.0f, 1.0f, 1.0f };

		xrvk::Buffer indexBuffer {};
		xrvk::Buffer vertexBuffer {};

		VkPipeline pipeline = VK_NULL_HANDLE;

		std::vector< unsigned short >* vecIndices = nullptr;
		std::vector< Shapes::Vertex >* vecVertices = nullptr;

		Shape* Duplicate()
		{
			Shape* shape = new Shape;
			shape->pose = pose;
			shape->scale = scale;
			shape->indexBuffer = indexBuffer;
			shape->vertexBuffer = vertexBuffer;
			shape->pipeline = pipeline;
			shape->vecIndices = vecIndices;
			shape->vecVertices = vecVertices;

			return shape;
		}
	};

	struct Shape_Cube : Shape
	{
		Shape_Cube() 
		{ 
			XrPosef_Identity( &pose ); 
			this->vecIndices = &vecCubeIndices;
			this->vecVertices = &vecCubeVertices;
		}
	};

} // namespace Shapes

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

		std::vector< std::vector< Buffer > > vecvecUniformBuffers_Shapes;

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

		// pipelines
		VkPipeline vkBoundPipeline = VK_NULL_HANDLE;

		// pipeline layouts
		VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
		VkPipelineLayout vkPipelineLayoutVisMask = VK_NULL_HANDLE;
		VkPipelineLayout vkPipelineLayoutShapes = VK_NULL_HANDLE;

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

		void SetupDescriptors();
		void SetupNodeDescriptorSet( vkglTF::Node *node );

		// Pipelines
		void PrepareShapesPipeline( Shapes::Shape *shape, std::string sVertexShader, std::string sFragmentShader,
									VkPolygonMode vkPolygonMode = VK_POLYGON_MODE_FILL ); // basic geometry
		void PreparePipelines();														  // pbr

		// Shaders
		void PrepareUniformBuffers();
		VkShaderModule CreateShaderModule( const std::string &sFilename );
		VkPipelineShaderStageCreateInfo CreateShaderStage( VkShaderStageFlagBits flagShaderStage, VkShaderModule *pShaderModule, const std::string &sEntrypoint );

		// Renderables handling
		uint32_t AddRenderScene( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f } );
		uint32_t AddRenderSector( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f }, XrSpace xrSpace = XR_NULL_HANDLE );
		uint32_t AddRenderModel( std::string sFilename, XrVector3f scale = { 1.0f, 1.0f, 1.0f }, XrSpace xrSpace = XR_NULL_HANDLE );

		std::vector< Shapes::Shape * > vecShapes;

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

		// Shader loading (bytecode only - spirv)
#ifdef XR_USE_PLATFORM_ANDROID
		std::vector< char > readFile( const std::string &filename )
		{
			AAsset *file = AAssetManager_open( m_pProvider->Instance()->androidActivity->assetManager, filename.c_str(), AASSET_MODE_BUFFER );
			size_t fileLength = AAsset_getLength( file );
			char *fileContent = new char[ fileLength ];
			AAsset_read( file, fileContent, fileLength );
			AAsset_close( file );

			std::vector< char > vec( fileContent, fileContent + fileLength );
			return vec;
		}
#else
		static std::vector< char > readFile( const std::string &filename )
		{
			std::ifstream file( filename, std::ios::ate | std::ios::binary );

			if ( !file.is_open() )
			{
				std::filesystem::path cwd = std::filesystem::current_path();
				LogError( "Unable to read file: %s (%s)", filename.c_str(), cwd.generic_string().c_str() );
				throw std::runtime_error( "failed to open file!" );
			}

			size_t fileSize = ( size_t )file.tellg();
			std::vector< char > buffer( fileSize );

			file.seekg( 0 );
			file.read( buffer.data(), fileSize );

			file.close();
			return buffer;
		}
#endif

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