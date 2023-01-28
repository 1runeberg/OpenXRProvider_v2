cmake_minimum_required(VERSION 3.14.4 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)
set(MAIN_PROJECT "OPENXR_PROVIDER")
project("${MAIN_PROJECT}")

# Compiler specific stuff
IF(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc") # for perf
ELSEIF(APPLE)
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -ObjC++")
	ELSE()
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -xobjective-c++")
	ENDIF()
ENDIF(MSVC)

# Add Vulkan - this is the only graphics api supported
find_package(Vulkan)

IF (NOT Vulkan_FOUND)
	message(FATAL_ERROR "[${MAIN_PROJECT}] ERROR: Unable to find Vulkan library - install the Vulkan SDK from https://vulkan.lunarg.com/")
ELSE()
	message(STATUS "[${MAIN_PROJECT}] Vulkan library loaded: ${Vulkan_LIBRARY}")
ENDIF()

# Add OpenXR Provider Library
set(OPENXR_PROVIDER "openxr_provider")
add_subdirectory(${OPENXR_PROVIDER})

# Samples
set (SAMPLES_FOLDER "Samples")

# Sample 01 - Hello
add_subdirectory("sample_01_hello")

# Sample 02 - Hello Extensions
add_subdirectory("sample_02_hello_exts")

# Sample 03 - Hello Extensions
add_subdirectory("sample_03_hello_session")

# Sample 04 - Hello Cube
add_subdirectory("sample_04_hello_cube")

# Sample 05 - Hello PBR
add_subdirectory("sample_05_hello_pbr")

# Sample 06 - Finger Painting
add_subdirectory("sample_06_finger_painting")

# Sample 07 - Finger Painting 2
add_subdirectory("sample_07_finger_painting2")

# Sample 08 - Finger Painting 3
add_subdirectory("sample_08_finger_painting3")

# Sample 09 - Hidden Worlds
add_subdirectory("sample_09_hidden_worlds")

# Sample Extensions (mini demos for extensions)
add_subdirectory("sample_extensions/htcx_vive_tracker_interaction")

# Set a sample app as the startup project in VS
set_property (DIRECTORY PROPERTY VS_STARTUP_PROJECT "sample_09_hidden_worlds")
message("[${MAIN_PROJECT}] Startup project for VS set to: sample_09_hidden_worlds")
