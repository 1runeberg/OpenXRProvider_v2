# OpenXR Provider v2
# YOUR_LOCALIZED_APP_NAME_HERE

cmake_minimum_required(VERSION 3.14.4 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)

set(XR_PROJECT "YOUR_OUTPUT_DIRECTORY_HERE")
project("${XR_PROJECT}" VERSION 0.1.0)

# Set project directories
set(SAMPLES_FOLDER "Samples")
set(APP_DIRECTORY "${CMAKE_SOURCE_DIR}/${XR_PROJECT}")

set(APP_SOURCE_DIRECTORY "${APP_DIRECTORY}/src")
set(APP_BINARY_DIRECTORY "${APP_DIRECTORY}/bin")
set(APP_LIBRARY_DIRECTORY "${APP_DIRECTORY}/lib")

set(APP_ASSETS_DIRECTORY "${APP_DIRECTORY}/assets")
set(APP_SHADERS_DIRECTORY "${APP_ASSETS_DIRECTORY}/shaders")
set(APP_MODELS_DIRECTORY "${APP_ASSETS_DIRECTORY}/models")
set(APP_TEXTURES_DIRECTORY "${APP_ASSETS_DIRECTORY}/textures")

set(PROVIDER_DIRECTORY "${CMAKE_SOURCE_DIR}/openxr_provider")
set(PROVIDER_INCLUDE_DIRECTORY "${PROVIDER_DIRECTORY}/include")

set(TEMPLATE_DIRECTORY "${APP_DIRECTORY}/openxr_app")
set(THIRD_PARTY_DIRECTORY "${APP_DIRECTORY}/thirdparty")


# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "[${XR_PROJECT}] Project language set to C++17")

# Check platform architecture
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
	    set(PLATFORM 64)
	else()
        message(FATAL_ERROR "[${XR_PROJECT}] ERROR: Only 64-bit platforms are supported.")
	endif()
endif()

# Find all project header files
file(GLOB APP_HEADER_FILES
        "${APP_SOURCE_DIRECTORY}/*.h*"
        "${TEMPLATE_DIRECTORY}/*.h*"
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
        "${TEMPLATE_DIRECTORY}/*.cpp"
	)

# Define application executable/module
if(ANDROID)
    # Android log library
    find_library(ANDROID_LIBRARY NAMES android)
    find_library(ANDROID_LOG_LIBRARY NAMES log)

    # Add native app glue
    find_path(ANDROID_NATIVE_APP_GLUE NAMES android_native_app_glue.h PATHS ${ANDROID_NDK}/sources/android/native_app_glue)

    if(NOT ANDROID_NATIVE_APP_GLUE)
        message(FATAL_ERROR "[${XR_PROJECT}] ERROR: Unable to find native_app_glue in:  ${ANDROID_APP_GLUE}")
    endif()

    add_library(android_native_app_glue_YOUR_APP_NAME_HERE OBJECT "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
    target_include_directories(android_native_app_glue_YOUR_APP_NAME_HERE PUBLIC ${ANDROID_NATIVE_APP_GLUE})
    target_compile_options(android_native_app_glue_YOUR_APP_NAME_HERE PRIVATE -Wno-unused-parameter)

    add_library(${XR_PROJECT} MODULE 
        ${APP_HEADER_FILES}
        ${APP_SOURCE_FILES}
        $<TARGET_OBJECTS:android_native_app_glue_YOUR_APP_NAME_HERE>)

    target_link_libraries(${XR_PROJECT}
        ${Vulkan_LIBRARY}
        ${OPENXR_PROVIDER}
        ${ANDROID_LIBRARY} 
        ${ANDROID_LOG_LIBRARY})

    # Set platform define for openxr and provider
    target_compile_definitions(${XR_PROJECT} PRIVATE XR_USE_PLATFORM_ANDROID=1)     
else()
    add_executable(${XR_PROJECT} 
        ${APP_HEADER_FILES} 
        ${APP_SOURCE_FILES})

    target_link_libraries(${XR_PROJECT} 
    				  ${Vulkan_LIBRARY}
					  ${OPENXR_PROVIDER})
endif()

message(STATUS "[${XR_PROJECT}] Project executable/module defined.")

# Set project config header which contains this project's current version number
configure_file(project_config.h.in ${APP_SOURCE_DIRECTORY}/project_config.h)
message(STATUS "[${XR_PROJECT}] Project version is ${CMAKE_PROJECT_VERSION}")

# Add this project to the samples folder (defined in main CMakeLists file)
set_target_properties(${XR_PROJECT} PROPERTIES FOLDER ${SAMPLES_FOLDER})

# Set project public include headers
# Subdirectories are optional just for convenience/readability (e.g. #include <gli/...> vs #include<gli/gli/...>)
target_include_directories(${XR_PROJECT} PUBLIC ${APP_SOURCE_DIRECTORY}
                                               ${Vulkan_INCLUDE_DIRS}
                                               ${PROVIDER_INCLUDE_DIRECTORY}
                                               ${OPENXR_PROVIDER}
					                           ${THIRD_PARTY_DIRECTORY}
                                               ${TEMPLATE_DIRECTORY})

#For Android, add the native app glue NDK directory
if(ANDROID)
    target_include_directories(${XR_PROJECT} PUBLIC ${ANDROID_NATIVE_APP_GLUE})
endif()

message(STATUS "[${XR_PROJECT}] Public include directories defined.")

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${APP_DIRECTORY} FILES ${APP_HEADER_FILES} ${APP_SOURCE_FILES})

# Link third party libraries


message(STATUS "[${XR_PROJECT}] Third party libraries linked.")

# Set output directories
set_target_properties(${XR_PROJECT} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${APP_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${APP_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${APP_BINARY_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${APP_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${APP_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${APP_BINARY_DIRECTORY}"
)

message(STATUS "[${XR_PROJECT}] Project archives will be built in: ${APP_LIBRARY_DIRECTORY}")
message(STATUS "[${XR_PROJECT}] Project libraries will be built in: ${APP_LIBRARY_DIRECTORY}")
message(STATUS "[${XR_PROJECT}] Project binaries will be built in: ${APP_BINARY_DIRECTORY}")

# Post-Build
add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_BINARY_DIRECTORY}"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROVIDER_DIRECTORY}/bin"
        "${APP_BINARY_DIRECTORY}")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROVIDER_DIRECTORY}/bin"
    "${CMAKE_SOURCE_DIR}/build/${XR_PROJECT}")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_SHADERS_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/shaders")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_MODELS_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/models")

 add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_TEXTURES_DIRECTORY}"
    "${APP_BINARY_DIRECTORY}/textures")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_SHADERS_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/${XR_PROJECT}/shaders")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_MODELS_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/${XR_PROJECT}/models")

add_custom_command(TARGET ${XR_PROJECT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APP_TEXTURES_DIRECTORY}"
    "${CMAKE_SOURCE_DIR}/build/${XR_PROJECT}/textures")
