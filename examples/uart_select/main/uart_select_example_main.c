/* UART Select Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_system.h"


#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "protocol_examples_common.h"

#if CONFIG_EXAMPLE_USE_NETLOGGING
#include "net_logging.h"
#endif

static const char *TAG = "EXAMPLE_MAIN";

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

static void uart_select_task(void *arg)
{
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB, // UART_SCLK_DEFAULT in idf 5+
	};
	uart_driver_install(UART_NUM_0, 2*1024, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_0, &uart_config);
	uart_set_pin(UART_NUM_0, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	while (1) {
		int fd;

		if ((fd = open("/dev/uart/0", O_RDWR)) == -1) {
			ESP_LOGE(TAG, "Cannot open UART");
			vTaskDelay(5000 / portTICK_PERIOD_MS);
			continue;
		}

		// We have a driver now installed so set up the read/write functions to use driver also.
		esp_vfs_dev_uart_use_driver(0);
		//uart_vfs_dev_use_driver(UART_NUM_0);

		while (1) {
			int s;
			fd_set rfds;
			struct timeval tv = {
				.tv_sec = 5,
				.tv_usec = 0,
			};

			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);

			s = select(fd + 1, &rfds, NULL, NULL, &tv);

			if (s < 0) {
				ESP_LOGE(TAG, "Select failed: errno %d", errno);
				break;
			} else if (s == 0) {
				ESP_LOGI(TAG, "Timeout has been reached and nothing has been received");
			} else {
				if (FD_ISSET(fd, &rfds)) {
					char buf;
					if (read(fd, &buf, 1) > 0) {
						ESP_LOGI(TAG, "Received: %c", buf);
						// Note: Only one character was read even the buffer contains more. The other characters will
						// be read one-by-one by subsequent calls to select() which will then return immediately
						// without timeout.
					} else {
						ESP_LOGE(TAG, "UART read error");
						break;
					}
				} else {
					ESP_LOGE(TAG, "No FD has been set in select()");
					break;
				}
			}
		}

		close(fd);
	}

	vTaskDelete(NULL);
}

void app_main(void)
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

	xTaskCreate(uart_select_task, "uart_select_task", 4*1024, NULL, 5, NULL);
}
