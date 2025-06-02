cmake_minimum_required(VERSION 3.5)

# The values of REQUIRES and PRIV_REQUIRES should not depend on any configuration choices (CONFIG_xxx macros).
# This is because requirements are expanded before configuration is loaded. Other component variables (like include paths or source files) can depend on configuration choices.
set(reqs
    esp_ringbuf
    # esp_http_client
)

set(component_srcs
    "src/net_logging.c"
    # "src/builtin_client/udp_client.c"
    "src/builtin_client/multicast_log_sender.c"
    # "src/builtin_client/tcp_client.c"
    # "src/builtin_client/http_client.c"
    "src/builtin_sse_server/sse_server.c"
)

idf_component_register(
    SRCS
        "${component_srcs}"
    INCLUDE_DIRS
        "include"
    PRIV_REQUIRES
        ${reqs}
)

# Include the index.html from this library if it is not customized by the project.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/include.cmake)
if (NOT CONFIG_NETLOGGING_CUSTOM_SSE_ASSETS)
    message (STATUS "Using built-in static assets for built-in HTTP SSE Logging Server")
    set (asset_src "${COMPONENT_DIR}/src/builtin_sse_server/www/index.html")
    target_add_netlogging_asset(${COMPONENT_LIB} ${asset_src})
endif()
