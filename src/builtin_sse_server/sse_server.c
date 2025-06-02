/*
    Simple Stand-alone HTTP Server with SSE (Server-Sent Events) for ESP32 remote logging
*/

#include "net_logging.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE // set log level in this file only
#include "esp_log.h"
#include "esp_netif_types.h" // for IP_EVENT
#include "esp_wifi_types.h" // for WIFI_EVENT
#include "esp_system.h"
#include "esp_event.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/inet.h" // for inet_addr_from_ip4addr
#include "lwip/netdb.h" // for getaddrinfo
#if CONFIG_NETLOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

#define RETRY_TIMEOUT_MS (3000) // retry timeout in ms
#define KEEPALIVE_TIMEOUT_MS (10000) // keepalive timeout in ms

#define STOP_WAITTIME (8000 / portTICK_PERIOD_MS) // wait this long for task to stop
#define MAX_CLIENTS (CONFIG_LWIP_MAX_SOCKETS - 3)
#define INVALID_SOCK (-1) // Indicates that the file descriptor represents an invalid (uninitialized or closed) socket
#define YIELD_TO_ALL_MS (50) // Time in ms to yield to all tasks when a non-blocking socket would block
#define USE_GZIP_ASSETS (1) // Use gzip compression for index.html


#define TAG "sse_log_sender"

// File content buffer for index.html
#if USE_GZIP_ASSETS
extern const char sse_html_start[] asm("_binary_index_html_gz_start");
extern const char sse_html_end[] asm("_binary_index_html_gz_end");
#else
extern const char sse_html_start[] asm("_binary_index_html_start");
extern const char sse_html_end[] asm("_binary_index_html_end");
#endif // USE_GZIP_ASSETS

struct client_handle_s
{
    int sock;
    void *buffer;
    TickType_t last_activity;
};
struct server_handle_s
{
    sse_logging_param_t param;
    struct client_handle_s client[MAX_CLIENTS];
    volatile bool task_run;
    int start_count;
    EventGroupHandle_t state_event;      /*!< Task's state event group */
};
static struct server_handle_s *server = NULL;
#define STOPPED_BIT (1UL << 0) // bit to signal that task has stopped
#define NET_CHANGED_BIT (1UL << 1) // bit to signal that network configuration has changed

/**
 * @brief Tries to receive data from specified sockets in a non-blocking way,
 *        i.e. returns immediately if no data.
 *
 * @param[in] sock Socket for reception
 * @param[out] data Data pointer to write the received data
 * @param[in] max_len Maximum size of the allocated space for receiving data
 * @return
 *          >0 : Size of received data
 *          =0 : No data available
 *          -1 : Error occurred during socket read operation
 *          -2 : Socket is not connected, to distinguish between an actual socket error and active disconnection
 */
static int try_receive(const int sock, char *data, size_t max_len)
{
    int len = recv(sock, data, max_len, 0);
    if (len < 0) {
        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   // Not an error
        }
        if (errno == ENOTCONN) {
            NETLOGGING_LOGD("[sock=%d]: Connection closed", sock);
            return -2;  // Socket has been disconnected
        }
        NETLOGGING_LOGD("Error occurred during receiving. errno: %d", errno);
        return -1;
    }

    return len;
}

/**
 * @brief Sends the specified data to the socket. This function blocks until all bytes got sent.
 *
 * @param[in] sock Socket to write data
 * @param[in] data Data to be written
 * @param[in] len Length of the data
 * @return
 *          >0 : Size the written data
 *          -1 : Error occurred during socket write operation
 */
static int socket_send(const int sock, const char *data, const size_t len)
{
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
            NETLOGGING_LOGD("Error occurred during sending. errno: %d", errno);
            return -1;
        }
        to_write -= written;
        if (to_write > 0) {
            // If not all data has been sent, yield to other tasks before continuing
            vTaskDelay(pdMS_TO_TICKS(YIELD_TO_ALL_MS));
        }
    }
    return len;
}

/**
 * @brief Returns the string representation of client's address (accepted on this server)
 */
static inline char *get_clients_address(struct sockaddr_storage *source_addr)
{
    static char address_str[128];
    char *res = NULL;
    // Convert ip address to string
    if (source_addr->ss_family == PF_INET) {
        res = inet_ntoa_r(((struct sockaddr_in *)source_addr)->sin_addr, address_str, sizeof(address_str) - 1);
    }
#ifdef CONFIG_LWIP_IPV6
    else if (source_addr->ss_family == PF_INET6) {
        res = inet6_ntoa_r(((struct sockaddr_in6 *)source_addr)->sin6_addr, address_str, sizeof(address_str) - 1);
    }
#endif
    if (!res) {
        address_str[0] = '\0'; // Returns empty string if conversion didn't succeed
    }
    return address_str;
}



static void cleanup_client(struct client_handle_s *client)
{
    if (INVALID_SOCK != client->sock) {
        shutdown(client->sock, 0);
        close(client->sock);
        client->sock = INVALID_SOCK;
    }
    if (NULL != client->buffer) {
        netlogging_unregister_recieveBuffer(client->buffer);
#if CONFIG_NETLOGGING_USE_RINGBUFFER
        vRingbufferDelete(client->buffer);
#else
        vMessageBufferDelete(client->buffer);
#endif
        client->buffer = NULL;
    }
}

static int serve_client(struct client_handle_s *client)
{
    assert(client != NULL);
    TickType_t now = xTaskGetTickCount();
    // first time serving this client?
    if (NULL == client->buffer)
    {
        // Receive HTTP request
        char request[1024];
        int req_len = try_receive(client->sock, request, sizeof(request) - 1);
        if (0 == req_len) {
            // No data available -> yield to other tasks
            return 0;
        }
        if (req_len < 0) {
            // Error occurred within this client's socket -> close and mark invalid
            return req_len;
        }

        // Else got HTTP request
        const size_t sse_html_size = sse_html_end - sse_html_start;
        // first time serving this client
        NETLOGGING_LOGD("serve new client");

        // Null-terminate received data
        request[req_len] = 0;

        // Check if the request is for the root path (/)
        if (strstr(request, "GET / HTTP") != NULL) {
            // Prepare HTTP response
            char headers[256];
            snprintf(headers, sizeof(headers),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %d\r\n"
#if USE_GZIP_ASSETS
                "Content-Encoding: gzip\r\n"
#endif // USE_GZIP_ASSETS
                "Connection: close\r\n"
                "\r\n",
                sse_html_size);

            // Send header
            int sent_len = socket_send(client->sock, headers, strlen(headers));
            if (sent_len < 0) {
                NETLOGGING_LOGD("Error sending header: errno %d", errno);
                return -1;
            }

            // Send file content
            sent_len = socket_send(client->sock, sse_html_start, sse_html_size);
            if (sent_len < 0) {
                NETLOGGING_LOGD("Error sending file content: errno %d", errno);
                return -1;
            }

        }
        // Check if the request is for the SSE endpoint
        else if (strstr(request, "GET /log-events HTTP") != NULL) {
            NETLOGGING_LOGD("client connected");
            // Send SSE headers
            const char *headers = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "retry: 1000\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "\r\n";

            socket_send(client->sock, headers, strlen(headers));

#if CONFIG_NETLOGGING_USE_RINGBUFFER
            // Create RingBuffer
            client->buffer = xRingbufferCreate(CONFIG_NETLOGGING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
#else
            // Create MessageBuffer
            client->buffer = xMessageBufferCreate(CONFIG_NETLOGGING_BUFFER_SIZE);
#endif
            if (NULL == client->buffer) {
                NETLOGGING_LOGE("bufferCreate failed");
                return -1;
            }
            esp_err_t err = netlogging_register_recieveBuffer(client->buffer);
            if (err != ESP_OK) {
                NETLOGGING_LOGE("netlogging_register_recieveBuffer failed");
                return -1;
            }
            client->last_activity = 0; // force a keep-alive event on first run
        }
        else {
            // Not found response for other paths
            const char *not_found = "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 9\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Not Found";
            int sent_len = socket_send(client->sock, not_found, strlen(not_found));
            if (sent_len < 0) {
                NETLOGGING_LOGD("Error sending 404: errno %d", errno);
                return -1;
            }
        }
    }
    else {
        // Keep connection open and send SSE events

#if CONFIG_NETLOGGING_USE_RINGBUFFER
        size_t received;
        char *buffer = (char *)xRingbufferReceive(client->buffer, &received, 0); // don't wait
        if (NULL == buffer) {
            received = 0; // no data available
        }
#else
        char buffer[CONFIG_NETLOGGING_MESSAGE_MAX_LENGTH];
        size_t received = xMessageBufferReceive(client->buffer, buffer, sizeof(buffer), 0); // don't wait
#endif

        if ((NULL != buffer) && (received > 0)) {
            // Format the buffer content as an SSE event
            char sse_event[CONFIG_NETLOGGING_MESSAGE_MAX_LENGTH + 64];
            snprintf(sse_event, sizeof(sse_event), "event: log-line\ndata: %.*s\n\n", (int)received, buffer);

#if CONFIG_NETLOGGING_USE_RINGBUFFER
            vRingbufferReturnItem(client->buffer, (void *)buffer);
#endif

            // Send the event
            int ret = socket_send(client->sock, sse_event, strlen(sse_event));
            if (ret < 0) {
                NETLOGGING_LOGD("Error sending SSE event: errno %d", errno);
                return -1;
            }
            client->last_activity = now;
        }
        else if (now > (client->last_activity + pdMS_TO_TICKS(KEEPALIVE_TIMEOUT_MS))) {
            // No data available, send keep-alive event
            NETLOGGING_LOGD("sending keep-alive");
            char sse_event[CONFIG_NETLOGGING_MESSAGE_MAX_LENGTH + 64];
            snprintf(sse_event, sizeof(sse_event), "event: keepalive\ndata: %"PRIu32"\n\n", now);

            int ret = socket_send(client->sock, sse_event, strlen(sse_event));
            if (ret < 0) {
                NETLOGGING_LOGD("Error sending keep-alive: errno %d", errno);
                return -1;
            }
            client->last_activity = now;
        }
    }
    return 0;
}

static void network_changed_handler(void *arg, esp_event_base_t event_base,
    const int32_t event_id, void *event_data)
{
    if (NULL == server) {
        return;
    }
    xEventGroupSetBits(server->state_event, NET_CHANGED_BIT);
}

static void server_task(void *pvParameters)
{
    NETLOGGING_LOGD("Starting HTTP Logging Server");

    while (server->task_run) // Outer while loop to (re)start server
    {
        if (server->start_count > 0) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_TIMEOUT_MS));
            NETLOGGING_LOGD("Restart HTTP Logging Server");
        }
        server->start_count++;

        // Prepare a list to hold client's connection state, mark all of them as invalid, i.e. available
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            server->client[i].sock = INVALID_SOCK;
            server->client[i].buffer = NULL;
        }

        // Creating a listener socket for incoming connections
#if CONFIG_LWIP_IPV6
        int listen_sock = socket(PF_INET6, SOCK_STREAM, 0);
#else
        int listen_sock = socket(PF_INET, SOCK_STREAM, 0);
#endif
        if (listen_sock < 0) {
            NETLOGGING_LOGE("Unable to create socket: errno %d", errno);
            continue; // continue outer while loop to retry
        }
        NETLOGGING_LOGD("Listener socket created");

#if CONFIG_LWIP_IPV6
        struct in6_addr inaddr_any = IN6ADDR_ANY_INIT;
        struct sockaddr_in6 serv_addr = {
            .sin6_family = PF_INET6,
            .sin6_addr = inaddr_any,
            .sin6_port = htons(server->param.port)
        };
#else
        struct sockaddr_in serv_addr = {
            .sin_family = PF_INET,
            .sin_addr = {
                .s_addr = htonl(INADDR_ANY)
            },
            .sin_port = htons(handle->param.port)
        };
#endif

        // Marking the socket as non-blocking
        int flags = fcntl(listen_sock, F_GETFL);
        if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            NETLOGGING_LOGE("Unable to set socket non blocking");
            continue; // continue outer while loop to retry
        }

        /* Enable SO_REUSEADDR to allow binding to the same
         * address and port when restarting the server */
        int opt = 1;
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            NETLOGGING_LOGW("error enabling SO_REUSEADDR %d", errno);
            /* This will fail if CONFIG_LWIP_SO_REUSE is not enabled. But
             * it does not affect the normal working of the HTTP Server */
        }


        // Binding socket to the given address
        int err = bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (err != 0) {
            NETLOGGING_LOGE("Socket unable to bind: errno %d", errno);
            continue; // continue outer while loop to retry
        }

        // Start listening
        // Set queue (backlog) of pending connections to 5 (can be more)
        err = listen(listen_sock, 5);
        if (err != 0) {
            NETLOGGING_LOGE("Error listening on socket: errno %d", errno);
            continue; // continue outer while loop to retry
        }
        NETLOGGING_LOGI("HTTP Logging Server listening on port %d", server->param.port);

        xEventGroupClearBits(server->state_event, NET_CHANGED_BIT);

        while (server->task_run) // Inner loop to accept clients
        {
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t addr_len = sizeof(source_addr);

            // Find a free socket
            int new_client_index = 0;
            for (new_client_index = 0; new_client_index < MAX_CLIENTS; ++new_client_index) {
                if (server->client[new_client_index].sock == INVALID_SOCK) {
                    break;
                }
            }

            // We accept a new connection only if we have a free socket
            if (new_client_index < MAX_CLIENTS) {
                // Try to accept a new connections
                struct client_handle_s *client = &server->client[new_client_index];
                client->sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

                if (client->sock < 0) {
                    if (errno == EWOULDBLOCK) { // The listener socket did not accepts any connection
                        // continue to serve open connections and try to accept again upon the next iteration
                        NETLOGGING_LOGV("No pending connections...");
                    }
                    else {
                        NETLOGGING_LOGE("Error accepting connection: errno %d", errno);
                        break; // break out of the inner while loop, back to the outer while loop to try to create the socket again
                    }
                }
                else {
                    // We have a new client connected -> print it's address
                    NETLOGGING_LOGD("[sock=%d]: Connection accepted from IP:%s", client->sock, get_clients_address(&source_addr));

                    // ...and set the client's socket non-blocking
                    flags = fcntl(client->sock, F_GETFL);
                    if (fcntl(client->sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                        NETLOGGING_LOGE("Unable to set socket non blocking: errno %d", errno);
                        break; // break out of the inner while loop, back to the outer while loop to try to create the socket again
                    }
                    NETLOGGING_LOGV("[sock=%d]: Socket marked as non blocking", client->sock);
                }
            }

            // We serve all the connected clients in this loop
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                struct client_handle_s *client = &server->client[i];
                if (client->sock != INVALID_SOCK) {
                    if (serve_client(client) != 0)
                    {
                        // Error occurred while serving this client -> close and mark invalid
                        NETLOGGING_LOGD("[sock=%d]: Error occurred while serving client -> closing the socket", client->sock);
                        cleanup_client(client);
                        continue; // continue to the next socket
                    }
                } // one client's socket
            } // for all sockets

            // Check if network configuration has changed
            EventBits_t uxBits = xEventGroupWaitBits(server->state_event, NET_CHANGED_BIT, true, true, 0);
            if (uxBits & NET_CHANGED_BIT) {
                NETLOGGING_LOGD("Network configuration changed");
                break; // break out of the inner while loop, back to the outer while loop to try to create the socket again
            }

            // Yield to other tasks
            vTaskDelay(pdMS_TO_TICKS(YIELD_TO_ALL_MS));
        } // end inner while

        NETLOGGING_LOGD("close and restart...");
        // Close the listener socket
        if (listen_sock != INVALID_SOCK) {
            close(listen_sock);
        }
        // Make sure all client sockets are closed
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            cleanup_client(&server->client[i]);
        }
    } // end outer while

    // Cleanup. Task is only responsible for freeing memory that it allocated.
    xEventGroupSetBits(server->state_event, STOPPED_BIT);
    NETLOGGING_LOGI("task stopped");
    vTaskDelete(NULL);
}

esp_err_t netlogging_sse_server_run(void)
{
    if (NULL == server) {
        return ESP_ERR_INVALID_STATE;
    }

    // Start Multicast Sender task
    server->task_run = true;
    server->start_count = 0;
    if( xTaskCreate(server_task, "HTTP SSE", 1024 * 6, NULL, 2, NULL) != pdPASS) {
        NETLOGGING_LOGE("xTaskCreate failed");
        server->task_run = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t netlogging_sse_server_wait_for_stop(void)
{
    if (NULL == server) {
        return ESP_ERR_INVALID_STATE;
    }
    EventBits_t uxBits = xEventGroupWaitBits(server->state_event, STOPPED_BIT, false, true, STOP_WAITTIME);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    if (uxBits & STOPPED_BIT)
    {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t netlogging_sse_server_stop(void)
{
    if (NULL == server) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Tell task to stop and delete itself */
    server->task_run = false;
    esp_err_t ret = netlogging_sse_server_wait_for_stop();
    return ret;
}

esp_err_t netlogging_sse_server_init(const sse_logging_param_t *param)
{
    NETLOGGING_LOGD("start see logging: port=%ld", param->port);

    // Allocate memory for the handle
    server = malloc(sizeof(struct server_handle_s));
    if (server == NULL) {
        NETLOGGING_LOGE("malloc fail");
        return ESP_ERR_NO_MEM;
    }
    memset(server, 0, sizeof(struct server_handle_s));
    server->param = *param;
    server->state_event = xEventGroupCreate();
    if (server->state_event == NULL) {
        NETLOGGING_LOGE("xEventGroupCreate failed");
        goto _init_failed;
    }
    // Register for events that indicate a change in network configuration
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &network_changed_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_changed_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &network_changed_handler, NULL);
    return ESP_OK;

_init_failed:
    netlogging_sse_server_deinit();
    return ESP_ERR_NO_MEM;
}

esp_err_t netlogging_sse_server_deinit(void)
{
    if (server)
    {
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, &network_changed_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_changed_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &network_changed_handler);
        if (server->state_event)
        {
            vEventGroupDelete(server->state_event);
        }
        free(server);
        server = NULL;
        return ESP_OK;
    }
    return ESP_FAIL;
}
