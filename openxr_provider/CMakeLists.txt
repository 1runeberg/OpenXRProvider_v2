# OPENXR PROVIDER v2

cmake_minimum_required(VERSION 3.14.4 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)
project("${OPENXR_PROVIDER}" VERSION 0.1.0)

# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "[${OPENXR_PROVIDER}] Project language set to C++17")

# Check platform architecture
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
	    set(PLATFORM 64)
	else()
        message(FATAL_ERROR "[${OPENXR_PROVIDER}] ERROR: Only 64-bit platforms are supported.")
	endif()
endif()

# OS specific
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_definitions(-D_WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)
endif()

# Set project source directory
set(PROVIDER_DIRECTORY "${CMAKE_SOURCE_DIR}/openxr_provider")
set(PROVIDER_SOURCE_DIRECTORY "${PROVIDER_DIRECTORY}/src")
set(PROVIDER_INCLUDE_DIRECTORY "${PROVIDER_DIRECTORY}/include")    # this is the public include directory for application use

# Find all project header files
file(GLOB PROVIDER_HEADER_FILES
        "${PROVIDER_INCLUDE_DIRECTORY}/*.h*"
        "${PROVIDER_SOURCE_DIRECTORY}/*.h*"
        "${PROVIDER_INCLUDE_DIRECTORY}/provider/*.h*"
        "${PROVIDER_INCLUDE_DIRECTORY}/openxr/*.h*"
	)

# Find all project source files
file(GLOB PROVIDER_SOURCE_FILES
        "${PROVIDER_DIRECTORY}/README.md"
        "${PROVIDER_DIRECTORY}/LICENSE" 
        "${PROVIDER_DIRECTORY}/CMakeLists.txt" 
        "${PROVIDER_DIRECTORY}/*.h.in" 
        "${PROVIDER_SOURCE_DIRECTORY}/*.cpp"
        "${PROVIDER_SOURCE_DIRECTORY}/common/*.cpp" 
	)

# Add project header and source files to project
add_library(${OPENXR_PROVIDER} SHARED ${PROVIDER_HEADER_FILES} ${PROVIDER_SOURCE_FILES})

message(STATUS "[${OPENXR_PROVIDER}] Project library defined.")

if(ANDROID)
    target_compile_definitions(${OPENXR_PROVIDER} PRIVATE XR_USE_PLATFORM_ANDROID=1)
endif()

# Set project config header which contains this project's current version number
configure_file(${PROVIDER_DIRECTORY}/project_config.h.in ${PROVIDER_INCLUDE_DIRECTORY}/provider/project_config.h)
message(STATUS "[${OPENXR_PROVIDER}] Project version is ${CMAKE_PROJECT_VERSION}")

# Set project public include headers
# Subdirectories are optional just for convenience/readability (e.g. #include <gli/...> vs #include<gli/gli/...>)
set(PROVIDER_THIRD_PARTY_DIRECTORY "${PROVIDER_DIRECTORY}/third_party")
set(OPENXR_DIRECTORY "${PROVIDER_THIRD_PARTY_DIRECTORY}/openxr")
set(OPENXR_INCLUDE_DIRECTORY "${OPENXR_DIRECTORY}/include")
target_include_directories(${OPENXR_PROVIDER} PUBLIC "${PROVIDER_SOURCE_DIRECTORY}"
                                                     "${Vulkan_INCLUDE_DIRS}"
                                                     "${PROVIDER_INCLUDE_DIRECTORY}"
                                                     "${PROVIDER_THIRD_PARTY_DIRECTORY}"
                                                     "${PROVIDER_THIRD_PARTY_DIRECTORY}/glm"
                                                     "${PROVIDER_THIRD_PARTY_DIRECTORY}/gli"
                                                     "${OPENXR_INCLUDE_DIRECTORY}/openxr"
                                                     "${PROVIDER_THIRD_PARTY_DIRECTORY}/spdlog/include")

#For Android, add the native app glue NDK directory
if(ANDROID)
   # Add native app glue
    find_path(ANDROID_NATIVE_APP_GLUE NAMES android_native_app_glue.h PATHS ${ANDROID_NDK}/sources/android/native_app_glue)

    if(NOT ANDROID_NATIVE_APP_GLUE)
        message(FATAL_ERROR "[${SAMPLE_PROJECT}] ERROR: Unable to find native_app_glue in:  ${ANDROID_APP_GLUE}")
    endif()

    target_include_directories(${OPENXR_PROVIDER} PUBLIC ${ANDROID_NATIVE_APP_GLUE})
endif()

message(STATUS "[${OPENXR_PROVIDER}] Public include directories defined.")

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${PROVIDER_DIRECTORY} FILES ${PROVIDER_HEADER_FILES} ${PROVIDER_SOURCE_FILES})

#Generate export headers
include(GenerateExportHeader)
generate_export_header(${OPENXR_PROVIDER})

message(STATUS "[${OPENXR_PROVIDER}] Export headers generated.")

# Link third party libraries
if(ANDROID)
    # Use Meta's custom openxr android loader
    add_library(openxr_loader SHARED IMPORTED)
    set_property(
        TARGET openxr_loader
        PROPERTY IMPORTED_LOCATION
        "${CMAKE_SOURCE_DIR}/openxr_provider/third_party/openxr_meta/OpenXR/Libs/Android/arm64-v8a/Debug/libopenxr_loader.so")
else()
    # For other platforms, we'll build the loader from source
    add_subdirectory(third_party/openxr)
endif()

# Link the openxr loader and vulkan libraries
if(ANDROID)
    # For Android, also link the android log libraries
    find_library(ANDROID_LIBRARY NAMES android)
    find_library(ANDROID_LOG_LIBRARY NAMES log)
    target_link_libraries(${OPENXR_PROVIDER}  PUBLIC openxr_loader ${Vulkan_LIBRARY}
                          ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY})
else()
    target_link_libraries(${OPENXR_PROVIDER} PUBLIC openxr_loader ${Vulkan_LIBRARY})
endif()

message(STATUS "[${OPENXR_PROVIDER}] Third party libraries linked.")

# Set output directories
set(PROVIDER_BINARY_DIRECTORY "${PROVIDER_DIRECTORY}/bin")
set(PROVIDER_LIBRARY_DIRECTORY "${PROVIDER_DIRECTORY}/lib")
set_target_properties(${OPENXR_PROVIDER} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PROVIDER_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PROVIDER_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROVIDER_BINARY_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PROVIDER_LIBRARY_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PROVIDER_LIBRARY_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROVIDER_BINARY_DIRECTORY}"
)

message(STATUS "[${OPENXR_PROVIDER}] Project archives will be built in: ${PROVIDER_ARCHIVE_DIRECTORY}")
message(STATUS "[${OPENXR_PROVIDER}] Project libraries will be built in: ${PROVIDER_LIBRARY_DIRECTORY}")
message(STATUS "[${OPENXR_PROVIDER}] Project binaries will be built in: ${PROVIDER_BINARY_DIRECTORY}")

# Post-Build
add_custom_command(TARGET ${OPENXR_PROVIDER} POST_BUILD

		COMMAND ${CMAKE_COMMAND} -E make_directory "${PROVIDER_INCLUDE_DIRECTORY}/openxr"

        #COMMAND ${CMAKE_COMMAND} -E copy_directory
        #"${OPENXR_INCLUDE_DIRECTORY}/openxr"
        #"${PROVIDER_INCLUDE_DIRECTORY}/openxr"
            
        COMMAND ${CMAKE_COMMAND} -E copy
            ${OPENXR_DIRECTORY}/LICENSE
            ${PROVIDER_INCLUDE_DIRECTORY}/openxr/LICENSE
        )

if(ANDROID)
add_custom_command(TARGET ${OPENXR_PROVIDER} POST_BUILD
        # Copy Meta's custom openxr android loader
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/openxr_provider/third_party/openxr_meta/OpenXR/Libs/Android/arm64-v8a/Debug"
        "${PROVIDER_BINARY_DIRECTORY}")
endif()
