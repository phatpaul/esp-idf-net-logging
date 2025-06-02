# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add the root of this git repo to the component search path. Only needed in this example project, since it uses the component from the root of the repo.
set(EXTRA_COMPONENT_DIRS 
    "../../"
    # This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
    "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sse_server)

if (CONFIG_NETLOGGING_CUSTOM_SSE_ASSETS)
    message (STATUS "Using custom static assets for HTTP SSE Logging Server")
    include(../../cmake/include.cmake) # Include the helper function to add assets. You may need to adjust the path based on your project structure.
    set (asset_src "${CMAKE_SOURCE_DIR}/www/index.html")
    target_add_netlogging_asset(${PROJECT_NAME}.elf ${asset_src})
endif()
