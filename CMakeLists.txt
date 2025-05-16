cmake_minimum_required(VERSION 3.5)

set(component_srcs
    "src/net_logging.c"
    # "src/builtin_client/udp_client.c"
    "src/builtin_client/multicast_log_sender.c"
    # "src/builtin_client/tcp_client.c"
    # "src/builtin_client/http_client.c"
    "src/builtin_sse_server/sse_server.c"
)

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS
        "include"
    PRIV_REQUIRES
        esp_http_client
        esp_ringbuf
        esp_hw_support
)

# Include the index.html from this library if it is not customized by the project.
if (NOT CONFIG_NETLOGGING_CUSTOM_SSE_ASSETS)
    message (STATUS "Using built-in static assets for built-in HTTP SSE Logging Server")
    set (asset_src "${COMPONENT_DIR}/src/builtin_sse_server/www/index.html")
    set (asset_dst_dir "${CMAKE_BINARY_DIR}/www")
    set (asset_dst "${asset_dst_dir}/index.html.gz")
    add_custom_command(
        WORKING_DIRECTORY ${COMPONENT_DIR}
        OUTPUT ${asset_dst}
        COMMAND ${CMAKE_COMMAND} -E echo "Compressing static assets for built-in HTTP SSE Logging Server..."
        COMMAND python tools/compress_assets.py --outdir=${asset_dst_dir} ${asset_src}
        DEPENDS ${asset_src}
    )
    add_custom_target(builtin_sse_assets DEPENDS "${asset_dst}")
    target_add_binary_data(${COMPONENT_LIB} "${asset_dst}" BINARY)
endif()
