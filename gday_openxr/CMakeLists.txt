# Set project name and version
project(GDAY_OPENXR VERSION 0.1.0)

# Must use C++17 minimum
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define executable
add_executable(GDAY_OPENXR "main.cpp")
target_link_libraries(GDAY_OPENXR ${OPENXR_PROVIDER}  ${Vulkan_LIBRARY})
target_include_directories(GDAY_OPENXR PUBLIC "${CMAKE_SOURCE_DIR}/openxr_provider/include" ${Vulkan_INCLUDE_DIRS})

# Copy openxr provider dll to build directory
add_custom_command(TARGET GDAY_OPENXR POST_BUILD

	    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/openxr_provider/bin"
            "${CMAKE_CURRENT_BINARY_DIR}"
            )
