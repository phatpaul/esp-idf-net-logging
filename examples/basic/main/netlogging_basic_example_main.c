/* The example of ESP-IDF net-logging
 *
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "protocol_examples_common.h"

#if CONFIG_EXAMPLE_USE_NETLOGGING
#include "net_logging.h"
#endif

static const char *TAG = "EXAMPLE_MAIN";

void app_main()
{
    ESP_LOGI(TAG, "Starting Net-Logging SSE Server Example");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_USE_NETLOGGING
    // init netlogging module
    ESP_LOGI(TAG, "Init netlogging module");
    int16_t write_to_stdout = 0;

#if CONFIG_EXAMPLE_NETLOGGING_WRITE_TO_STDOUT
    ESP_LOGI(TAG, "Enable write Logging to STDOUT");
    write_to_stdout = 1;
#endif

    ESP_ERROR_CHECK(netlogging_init(write_to_stdout));

#if CONFIG_EXAMPLE_USE_NETLOGGING_SSE_SERVER
// Setup SSE logging
    sse_logging_param_t sse_logging_params = NETLOGGING_SSE_DEFAULT_CONFIG();
    sse_logging_params.port = CONFIG_EXAMPLE_NETLOGGING_SSE_LISTEN_PORT;
    ESP_ERROR_CHECK(netlogging_sse_server_init(&sse_logging_params));
    ESP_ERROR_CHECK(netlogging_sse_server_run());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_SSE_SERVER

#if CONFIG_EXAMPLE_USE_NETLOGGING_MULTICAST
    multicast_logging_param_t multicast_logging_params = NETLOGGING_MULTICAST_DEFAULT_CONFIG();
    multicast_logging_params.ipv4addr = CONFIG_EXAMPLE_NETLOGGING_MULTICAST_IP;
    multicast_logging_params.port = CONFIG_EXAMPLE_NETLOGGING_MULTICAST_PORT;
    ESP_ERROR_CHECK(netlogging_multicast_sender_init(&multicast_logging_params));
    ESP_ERROR_CHECK(netlogging_multicast_sender_run());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_MULTICAST

#if CONFIG_EXAMPLE_USE_NETLOGGING_UDP
    udp_logging_param_t udp_logging_params = NETLOGGING_UDP_DEFAULT_CONFIG();
    udp_logging_params.ipv4addr = CONFIG_EXAMPLE_NETLOGGING_UDP_IP;
    udp_logging_params.port = CONFIG_EXAMPLE_NETLOGGING_UDP_PORT;
    ESP_ERROR_CHECK(netlogging_udp_client_init(&udp_logging_params));
    ESP_ERROR_CHECK(netlogging_udp_client_run());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_UDP

#if CONFIG_EXAMPLE_USE_NETLOGGING_TCP
    tcp_logging_param_t tcp_logging_params = {};
    tcp_logging_params.ipv4addr = CONFIG_EXAMPLE_NETLOGGING_TCP_IP;
    tcp_logging_params.port = CONFIG_EXAMPLE_NETLOGGING_TCP_PORT;
    ESP_ERROR_CHECK(netlogging_tcp_client_init(&tcp_logging_params));
    ESP_ERROR_CHECK(netlogging_tcp_client_run());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_TCP

#if CONFIG_EXAMPLE_USE_NETLOGGING_HTTP_CLIENT
    http_logging_param_t http_logging_params = {};
    http_logging_params.url = CONFIG_EXAMPLE_NETLOGGING_HTTP_CLIENT_CONNECT_URL;
    ESP_ERROR_CHECK(netlogging_http_client_init(&http_logging_params));
    ESP_ERROR_CHECK(netlogging_http_client_run());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_HTTP_CLIENT
#endif // CONFIG_EXAMPLE_USE_NETLOGGING


    // Let it run for a while...
    for (int i = 0; i < 30; i++) {
        ESP_LOGI(TAG, "Log message %d", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

#if CONFIG_EXAMPLE_USE_NETLOGGING
// Deinit netlogging module
    ESP_LOGI(TAG, "Stopping netlogging modules");
    vTaskDelay(100 / portTICK_PERIOD_MS); // wait for a while to let the last messages be sent

#if CONFIG_EXAMPLE_USE_NETLOGGING_SSE_SERVER
    netlogging_sse_server_stop();
    netlogging_sse_server_deinit();
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_SSE_SERVER

#if CONFIG_EXAMPLE_USE_NETLOGGING_MULTICAST
    ESP_ERROR_CHECK(netlogging_multicast_sender_stop());
    ESP_ERROR_CHECK(netlogging_multicast_sender_deinit());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_MULTICAST

#if CONFIG_EXAMPLE_USE_NETLOGGING_UDP
    ESP_ERROR_CHECK(netlogging_udp_client_stop());
    ESP_ERROR_CHECK(netlogging_udp_client_deinit());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_UDP

#if CONFIG_EXAMPLE_USE_NETLOGGING_TCP
    ESP_ERROR_CHECK(netlogging_tcp_client_stop());
    ESP_ERROR_CHECK(netlogging_tcp_client_deinit());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_TCP

#if CONFIG_EXAMPLE_USE_NETLOGGING_HTTP_CLIENT
    ESP_ERROR_CHECK(netlogging_http_client_stop());
    ESP_ERROR_CHECK(netlogging_http_client_deinit());
#endif // CONFIG_EXAMPLE_USE_NETLOGGING_HTTP_CLIENT

    netlogging_deinit();
#endif // CONFIG_EXAMPLE_USE_NETLOGGING

    ESP_LOGI(TAG, "Example Done");
}
