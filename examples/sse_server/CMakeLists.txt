# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add the root of this git repo to the component search path.
set(EXTRA_COMPONENT_DIRS "../../")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(sse_server)

if (CONFIG_NETLOGGING_CUSTOM_SSE_ASSETS)
message (STATUS "Using custom static assets for HTTP SSE Logging Server")
    set (asset_src "${CMAKE_SOURCE_DIR}/www/index.html")
    set (asset_dst_dir "${CMAKE_BINARY_DIR}/www")
    set (asset_dst "${asset_dst_dir}/index.html.gz")
    add_custom_command(
        WORKING_DIRECTORY ${COMPONENT_DIR}
        OUTPUT ${asset_dst}
        COMMAND ${CMAKE_COMMAND} -E echo "Compressing static assets..."
        COMMAND python tools/compress_assets.py --outdir=${asset_dst_dir} ${asset_src}
        DEPENDS ${asset_src}
    )
    add_custom_target(custom_sse_assets DEPENDS "${asset_dst}")
    target_add_binary_data(${PROJECT_NAME}.elf "${asset_dst}" BINARY)
endif()

