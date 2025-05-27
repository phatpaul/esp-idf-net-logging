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
include(${CMAKE_CURRENT_LIST_DIR}/cmake/include.cmake)
if (NOT CONFIG_NETLOGGING_CUSTOM_SSE_ASSETS)
    message (STATUS "Using built-in static assets for built-in HTTP SSE Logging Server")
    set (asset_src "${COMPONENT_DIR}/src/builtin_sse_server/www/index.html")
    netlogging_add_asset(${COMPONENT_LIB} ${asset_src})
endif()
