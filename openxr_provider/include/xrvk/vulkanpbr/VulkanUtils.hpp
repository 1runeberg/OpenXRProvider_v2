/*
* Vulkan utilities
*
* Copyright(C) 2018 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include "vulkan/vulkan.h"
#include "VulkanDevice.hpp"
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/asset_manager.h>
#elif defined(__linux__)
#include <dirent.h>
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	VkPipelineShaderStageCreateInfo loadShader(AAssetManager* androidAssetManager, VkDevice device, std::string filename, VkShaderStageFlagBits stage)
#else
	VkPipelineShaderStageCreateInfo loadShader(VkDevice device, std::string filename, VkShaderStageFlagBits stage)
#endif
{
	VkPipelineShaderStageCreateInfo shaderStage{};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.pName = "main";
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	std::string assetpath = filename;
	AAsset* asset = AAssetManager_open(androidAssetManager, assetpath.c_str(), AASSET_MODE_STREAMING);
	assert(asset);
	size_t size = AAsset_getLength(asset);
	assert(size > 0);
	char *shaderCode = new char[size];
	AAsset_read(asset, shaderCode, size);
	AAsset_close(asset);
	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;
	moduleCreateInfo.codeSize = size;
	moduleCreateInfo.pCode = (uint32_t*)shaderCode;
	moduleCreateInfo.flags = 0;
	VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStage.module));
	delete[] shaderCode;
#else
	std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

	if (is.is_open()) {
		size_t size = is.tellg();
		is.seekg(0, std::ios::beg);
		char* shaderCode = new char[size];
		is.read(shaderCode, size);
		is.close();
		assert(size > 0);
		VkShaderModuleCreateInfo moduleCreateInfo{};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = size;
		moduleCreateInfo.pCode = (uint32_t*)shaderCode;
		vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStage.module);
		delete[] shaderCode;
	}
	else {
		std::cerr << "Error: Could not open shader file \"" << filename << "\"" << std::endl;
		shaderStage.module = VK_NULL_HANDLE;
	}

#endif
	assert(shaderStage.module != VK_NULL_HANDLE);
	return shaderStage;
}

#if defined( VK_USE_PLATFORM_ANDROID_KHR )
	void readDirectory( AAssetManager* androidAssetManager, const std::string& directory, const std::string &pattern, std::map<std::string, std::string> &filelist, bool recursive)
#else
	void readDirectory(const std::string& directory, const std::string &pattern, std::map<std::string, std::string> &filelist, bool recursive)
#endif
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	AAssetDir* assetDir = AAssetManager_openDir(androidAssetManager, directory.c_str());
	AAssetDir_rewind(assetDir);
	const char* assetName;
	while ((assetName = AAssetDir_getNextFileName(assetDir)) != 0) {
		std::string filename(assetName);
		filename.erase(filename.find_last_of("."), std::string::npos);
		filelist[filename] = directory + "/" + assetName;
	}
	AAssetDir_close(assetDir);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	std::string searchpattern(directory + "/" + pattern);
	WIN32_FIND_DATA data;
	HANDLE hFind;
	if ((hFind = FindFirstFile(searchpattern.c_str(), &data)) != INVALID_HANDLE_VALUE) {
		do {
			std::string filename(data.cFileName);
			filename.erase(filename.find_last_of("."), std::string::npos);
			filelist[filename] = directory + "/" + data.cFileName;
		} while (FindNextFile(hFind, &data) != 0);
		FindClose(hFind);
	}
	if (recursive) {
		std::string dirpattern = directory + "/*";
		if ((hFind = FindFirstFile(dirpattern.c_str(), &data)) != INVALID_HANDLE_VALUE) {
			do {
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					char subdir[MAX_PATH];
					strcpy(subdir, directory.c_str());
					strcat(subdir, "/");
					strcat(subdir, data.cFileName);
					if ((strcmp(data.cFileName, ".") != 0) && (strcmp(data.cFileName, "..") != 0)) 
					{
						readDirectory(subdir, pattern, filelist, recursive);
					}
				}
			} while (FindNextFile(hFind, &data) != 0);
			FindClose(hFind);
		}
	}
#elif defined(VK_USE_PLATFORM_LINUX_KHR)
	std::string patternExt = pattern;
	patternExt.erase(0, pattern.find_last_of("."));
	struct dirent *entry;
	DIR *dir = opendir(directory.c_str());
	if (dir == NULL) {
		return;
	}
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG) {
			std::string filename(entry->d_name);
			if (filename.find(patternExt) != std::string::npos) {
				filename.erase(filename.find_last_of("."), std::string::npos);
				filelist[filename] = directory + "/" + entry->d_name;
			}
		}
		if (recursive && (entry->d_type == DT_DIR)) {
			std::string subdir = directory + "/" + entry->d_name;
			if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0)) {
				readDirectory(subdir, pattern, filelist, recursive);
			}
		}
	}
	closedir(dir);
#endif
}