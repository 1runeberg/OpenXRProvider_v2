# OpenXR Provider v2
# Sample Extension: XR_HTCX_vive_tracker_interaction

cmake_minimum_required(VERSION 3.14.4 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)

set(SAMPLE_PROJECT "htcx_vive_tracker_interaction")
project("${SAMPLE_PROJECT}" VERSION 0.1.0)

# Set project directories
set(SAMPLES_FOLDER "Samples/Extensions")
set(APP_DIRECTORY "${CMAKE_SOURCE_DIR}/sample_extensions/${SAMPLE_PROJECT}")

set(APP_SOURCE_DIRECTORY "${APP_DIRECTORY}/src")
set(APP_BINARY_DIRECTORY "${APP_DIRECTORY}/bin")
set(APP_LIBRARY_DIRECTORY "${APP_DIRECTORY}/lib")

set(APP_ASSETS_DIRECTORY "${APP_DIRECTORY}/assets")
set(APP_SHADERS_DIRECTORY "${APP_ASSETS_DIRECTORY}/shaders")
set(APP_MODELS_DIRECTORY "${APP_ASSETS_DIRECTORY}/models")
set(APP_TEXTURES_DIRECTORY "${APP_ASSETS_DIRECTORY}/textures")

set(PROVIDER_DIRECTORY "${CMAKE_SOURCE_DIR}/openxr_provider")
set(PROVIDER_INCLUDE_DIRECTORY "${PROVIDER_DIRECTORY}/include")

set(THIRD_PARTY_DIRECTORY "${APP_DIRECTORY}/thirdparty")

# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "[${SAMPLE_PROJECT}] Project language set to C++17")

# Check platform architecture
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
	    set(PLATFORM 64)
	else()
        message(FATAL_ERROR "[${SAMPLE_PROJECT}] ERROR: Only 64-bit platforms are supported.")
	endif()
endif()

# Find all project header files
file(GLOB APP_HEADER_FILES
        "${APP_SOURCE_DIRECTORY}/*.h*"
        "${APP_SOURCE_DIRECTORY}/xrvk/*.h*"
        "${APP_SOURCE_DIRECTORY}/xrvk/vulkanpbr/*.h*"
	)

# Find all project source files
file(GLOB APP_SOURCE_FILES
        "${APP_DIRECTORY}/README.md"
        "${APP_DIRECTORY}/LICENSE" 
        "${APP_DIRECTORY}/CMakeLists.txt" 
        "${APP_DIRECTORY}/*.h.in" 
        "${APP_SOURCE_DIRECTORY}/*.cpp"
        "${APP_SOURCE_DIRECTORY}/xrvk/*.cpp"
        "${APP_SOURCE_DIRECTORY}/xrvk/vulkanpbr/*.cpp"
        "${APP_SHADERS_DIRECTORY}/*.*sl"
        "${APP_SHADERS_DIRECTORY}/*.vert"
        "${APP_SHADERS_DIRECTORY}/*.frag"
	)

add_executable(${SAMPLE_PROJECT} 
    ${APP_HEADER_FILES} 
    ${APP_SOURCE_FILES})

target_link_libraries(${SAMPLE_PROJECT} 
					${OPENXR_PROVIDER})

message(STATUS "[${SAMPLE_PROJECT}] Project executable/module defined.")

# Set project config header which contains this project's current version number
configure_file(project_config.h.in ${APP_SOURCE_DIRECTORY}/project_config.h)
message(STATUS "[${SAMPLE_PROJECT}] Project version is ${CMAKE_PROJECT_VERSION}")

# Add this project to the samples folder (defined in main CMakeLists file)
set_target_properties(${SAMPLE_PROJECT} PROPERTIES FOLDER ${SAMPLES_FOLDER})

# Set project public include headers
# Subdirectories are optional just for convenience/readability (e.g. #include <gli/...> vs #include<gli/gli/...>)
target_include_directories(${SAMPLE_PROJECT} PUBLIC ${APP_SOURCE_DIRECTORY}
                                               ${PROVIDER_INCLUDE_DIRECTORY}
                                               ${OPENXR_PROVIDER}
					                           ${THIRD_PARTY_DIRECTORY})

message(STATUS "[${SAMPLE_PROJECT}] Public include directories defined.")

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${APP_DIRECTORY} FILES ${APP_HEADER_FILES} ${APP_SOURCE_FILES})

# Link third party libraries

message(STATUS "[${SAMPLE_PROJECT}] Third party libraries linked.")

# Set output directories
set_target_properties(${SAMPLE_PROJECT} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${APP_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${APP_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${APP_BINARY_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${APP_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${APP_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${APP_BINARY_DIRECTORY}"
)

message(STATUS "[${SAMPLE_PROJECT}] Project archives will be built in: ${APP_LIBRARY_DIRECTORY}")
message(STATUS "[${SAMPLE_PROJECT}] Project libraries will be built in: ${APP_LIBRARY_DIRECTORY}")
message(STATUS "[${SAMPLE_PROJECT}] Project binaries will be built in: ${APP_BINARY_DIRECTORY}")

# Post-Build
add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_BINARY_DIRECTORY}"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROVIDER_DIRECTORY}/bin"
        "${APP_BINARY_DIRECTORY}")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROVIDER_DIRECTORY}/bin"
    "${CMAKE_SOURCE_DIR}/build/sample_extensions/${SAMPLE_PROJECT}")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_SHADERS_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/shaders")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_MODELS_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/models")

 add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_TEXTURES_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/textures")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_SHADERS_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/sample_extensions/${SAMPLE_PROJECT}/shaders")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_MODELS_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/sample_extensions/${SAMPLE_PROJECT}/models")

add_custom_command(TARGET ${SAMPLE_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_TEXTURES_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/sample_extensions/${SAMPLE_PROJECT}/textures")
