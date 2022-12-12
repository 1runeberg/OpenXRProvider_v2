# OpenXR Provider
# Sample 03 - Hello Session

cmake_minimum_required(VERSION 3.14.4 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)

set(SAMPLE_PROJECT "sample_03_hello_session")
project("${SAMPLE_PROJECT}" VERSION 0.1.0)

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


# Set project source directory
set(APP_DIRECTORY "${CMAKE_SOURCE_DIR}/sample_03_hello_session")
set(APP_SOURCE_DIRECTORY "${APP_DIRECTORY}/src")

# Find all project header files
file(GLOB APP_HEADER_FILES
        "${APP_SOURCE_DIRECTORY}/*.h*"
	)

# Find all project source files
file(GLOB APP_SOURCE_FILES
        "${APP_DIRECTORY}/README.md"
        "${APP_DIRECTORY}/LICENSE" 
        "${APP_DIRECTORY}/CMakeLists.txt" 
        "${APP_DIRECTORY}/*.h.in" 
        "${APP_SOURCE_DIRECTORY}/*.cpp"
	)

# Set directories
set(APP_BINARY_DIRECTORY "${APP_DIRECTORY}/bin")
set(APP_LIBRARY_DIRECTORY "${APP_DIRECTORY}/lib")
set(PROVIDER_DIRECTORY "${CMAKE_SOURCE_DIR}/openxr_provider")
set(PROVIDER_INCLUDE_DIRECTORY "${PROVIDER_DIRECTORY}/include")

# Define application executable/module
if(ANDROID)
    # Android log library
    find_library(ANDROID_LIBRARY NAMES android)
    find_library(ANDROID_LOG_LIBRARY NAMES log)

    # Add native app glue
    find_path(ANDROID_NATIVE_APP_GLUE NAMES android_native_app_glue.h PATHS ${ANDROID_NDK}/sources/android/native_app_glue)

    if(NOT ANDROID_NATIVE_APP_GLUE)
        message(FATAL_ERROR "[${SAMPLE_PROJECT}] ERROR: Unable to find native_app_glue in:  ${ANDROID_APP_GLUE}")
    endif()

    add_library(android_native_app_glue_3 OBJECT "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
    target_include_directories(android_native_app_glue_3 PUBLIC ${ANDROID_NATIVE_APP_GLUE})
    target_compile_options(android_native_app_glue_3 PRIVATE -Wno-unused-parameter)

    add_library(${SAMPLE_PROJECT} MODULE 
        ${APP_HEADER_FILES}
        ${APP_SOURCE_FILES}
        $<TARGET_OBJECTS:android_native_app_glue_3>)

    target_link_libraries(${SAMPLE_PROJECT}  
        ${Vulkan_LIBRARY}
        ${OPENXR_PROVIDER}
        ${ANDROID_LIBRARY} 
        ${ANDROID_LOG_LIBRARY})   

    # Set platform define for openxr and provider
    target_compile_definitions(${SAMPLE_PROJECT} PRIVATE XR_USE_PLATFORM_ANDROID=1)     
else()
    add_executable(${SAMPLE_PROJECT} 
        ${APP_HEADER_FILES} 
        ${APP_SOURCE_FILES})

    target_link_libraries(${SAMPLE_PROJECT} 
    				  ${Vulkan_LIBRARY}
					  ${OPENXR_PROVIDER})
endif()

message(STATUS "[${SAMPLE_PROJECT}] Project executable/module defined.")

# Set project config header which contains this project's current version number
configure_file(project_config.h.in ${APP_SOURCE_DIRECTORY}/project_config.h)
message(STATUS "[${SAMPLE_PROJECT}] Project version is ${CMAKE_PROJECT_VERSION}")

# Add this project to the samples folder (defined in main CMakeLists file)
set_target_properties(${SAMPLE_PROJECT} PROPERTIES FOLDER ${SAMPLES_FOLDER})

# Set project public include headers
# Subdirectories are optional just for convenience/readability (e.g. #include <gli/...> vs #include<gli/gli/...>)
target_include_directories(${SAMPLE_PROJECT} PUBLIC ${APP_SOURCE_DIRECTORY}
                                               ${Vulkan_INCLUDE_DIRS}
                                               ${PROVIDER_INCLUDE_DIRECTORY}
                                               ${OPENXR_PROVIDER})

#For Android, add the native app glue NDK directory
if(ANDROID)
    target_include_directories(${SAMPLE_PROJECT} PUBLIC ${ANDROID_NATIVE_APP_GLUE})
endif()

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
            "${APP_BINARY_DIRECTORY}"
            )
