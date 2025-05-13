/*
SSE server

This example code is in the Public Domain (or CC0 licensed, at your option.)

Unless required by applicable law or agreed to in writing, this
software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
*/

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE // set log level in this file only
#include "esp_system.h"
#include "lwip/sockets.h"
#include "lwip/inet.h" // for inet_addr_from_ip4addr
#include "lwip/netdb.h" // for getaddrinfo
#include "esp_netif.h"
#include "net_logging_priv.h"

#define USE_DEFAULT_IF (1) // 1=bind to default interface, 0=bind to specific interface
#define RETRY_TIMEOUT_MS (3000) // retry timeout in ms
#define STOPPED_BIT (1UL << 0) // bit to signal that task has stopped
#define STOP_WAITTIME (8000 / portTICK_PERIOD_MS) // wait this long for task to stop

#define TAG "sse_log_sender"

// File content buffer for sse.html
extern const unsigned char sse_html_start[] asm("_binary_sse_html_start");
extern const unsigned char sse_html_end[] asm("_binary_sse_html_end");

#define STOP_SERVING_CLIENT	( 1 << 0 )
#define STOPPED_SERVING_CLIENT	( 1 << 1 )

struct handle_s
{
    sse_logging_param_t param;
    volatile bool task_run;
    EventGroupHandle_t *state_event;      /*!< Task's state event group */
};
static struct handle_s *handle = NULL;

static void serve_client(void *pvParameters) 
{
    NETLOGGING_LOGI("enter serve_client");
    void **params = (void **)pvParameters;
    const int *client_sock_ptr = (int *)params[0];
    int client_sock = *client_sock_ptr;
    const EventGroupHandle_t *client_serving_task_events_ptr = (EventGroupHandle_t *)params[1];
    EventGroupHandle_t client_serving_task_events = *client_serving_task_events_ptr;
    const size_t sse_html_size = sse_html_end - sse_html_start;

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
    // Create RingBuffer
    RingbufHandle_t xRingBuffer = xRingbufferCreate(CONFIG_NET_LOGGING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (NULL == xRingBuffer) {
        NETLOGGING_LOGE("xRingBufferCreate failed");
        goto _init_failed;
    }
    esp_err_t err = netlogging_register_recieveBuffer(xRingBuffer);
#else
    // Create MessageBuffer
    MessageBufferHandle_t xMessageBuffer = xMessageBufferCreate(CONFIG_NET_LOGGING_BUFFER_SIZE);
    if (NULL == xMessageBuffer) {
        NETLOGGING_LOGE("xMessageBufferCreate failed");
        goto _init_failed;
    }
    esp_err_t err = netlogging_register_recieveBuffer(xMessageBuffer);
#endif
    if (err != ESP_OK) {
        NETLOGGING_LOGE("netlogging_register_recieveBuffer failed");
        goto _init_failed;
    }

    // Receive HTTP request
    char request[1024];
    int req_len = recv(client_sock, request, sizeof(request) - 1, 0);
    NETLOGGING_LOGI("request received: %.*s", req_len, request);
    if (req_len <= 0) {
        NETLOGGING_LOGE("Connection closed or error");
        shutdown(client_sock, 0);
        close(client_sock);
        vTaskDelete(NULL);
    }

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
            "Connection: close\r\n"
            "\r\n",
            sse_html_size);

        // Send header
        send(client_sock, headers, strlen(headers), 0);

        // Send file content
        send(client_sock, sse_html_start, sse_html_size, 0);
    }
    // Check if the request is for the SSE endpoint
    else if (strstr(request, "GET /log-events HTTP") != NULL) {
        NETLOGGING_LOGI("SSE client connected");
        // Send SSE headers
        const char *headers = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "retry: 1000\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";

        send(client_sock, headers, strlen(headers), 0);
        NETLOGGING_LOGI("serving SSE");

        // Keep connection open and send SSE events
        while ((xEventGroupGetBits(client_serving_task_events) & STOP_SERVING_CLIENT) == 0) {
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
            size_t received;
            char *buffer = (char *)xRingbufferReceive(xRingBuffer, &received, pdMS_TO_TICKS(10));
#else
            char buffer[CONFIG_NET_LOGGING_MESSAGE_MAX_LENGTH];
            size_t received = xMessageBufferReceive(xMessageBufferSSE, buffer, sizeof(buffer), pdMS_TO_TICKS(10));
#endif

            if (received > 0) {
                // Format the buffer content as an SSE event
                char sse_event[512];
                snprintf(sse_event, sizeof(sse_event), "event: log-line\ndata: %.*s\n\n", (int)received, buffer);

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
                vRingbufferReturnItem(xRingBuffer, (void *)buffer);
#endif

                // Send the event
                int ret = send(client_sock, sse_event, strlen(sse_event), 0);
                if (ret < 0) {
                    NETLOGGING_LOGE("Error sending SSE event: errno %d", errno);
                    break;
                }
            }
        }
    }
    else {
        // Not found response for other paths
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Not Found";
        send(client_sock, not_found, strlen(not_found), 0);
    }

    // Close connection
    NETLOGGING_LOGI("closing connection");
    shutdown(client_sock, 0);
    close(client_sock);

_init_failed:
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
    netlogging_unregister_recieveBuffer(xRingBuffer);
    if (xRingBuffer != NULL) {
        vRingbufferDelete(xRingBuffer);
        xRingBuffer = NULL;
    }
#else   
    netlogging_unregister_recieveBuffer(xMessageBuffer);
    if (xMessageBuffer != NULL) {
        vMessageBufferDelete(xMessageBuffer);
        xMessageBuffer = NULL;
    }
#endif
    xEventGroupSetBits(client_serving_task_events, STOPPED_SERVING_CLIENT);
    vTaskDelete(NULL);
}

static void sse_server(void *pvParameters)
{

    while (handle->task_run) // Outer while loop to create socket
    {
        NETLOGGING_LOGI("starting HTTP Server Sent Events logging");
        int addr_family = AF_INET;
        int ip_protocol = IPPROTO_IP;

        // Create a server socket instead of a client one
        struct sockaddr_in server_addr;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(handle->param.port);

        int server_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (server_sock < 0) {
            NETLOGGING_LOGE("Unable to create socket: errno %d", errno);
            // wait and then try again
            vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
            continue;
        }

        // Set socket option to reuse address
        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Bind socket to address
        int err =
            bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err != 0) {
            NETLOGGING_LOGE("Socket unable to bind: errno %d", errno);
            // wait and then try again
            vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
            continue;
        }

        // Start listening
        err = listen(server_sock, 5);
        if (err != 0) {
            NETLOGGING_LOGE("Error listening on socket: errno %d", errno);
            // wait and then try again
            vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
            continue;
        }

        NETLOGGING_LOGI("SSE Server listening on port %d", handle->param.port);

        // Main server loop
        static EventGroupHandle_t client_serving_task_events;
        StaticEventGroup_t client_serving_task_events_data;
        client_serving_task_events = xEventGroupCreateStatic(&client_serving_task_events_data);
        TaskHandle_t client_task = NULL;
        static int client_sock;
        while (handle->task_run) // Inner loop to accept clients
        {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_sock < 0) {
                NETLOGGING_LOGE("Unable to accept connection: errno %d", errno);
                continue;
            }
            NETLOGGING_LOGI("Accepted connection from %s:%d",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Stop serving the previous client, if any
            if (client_task != NULL) {
                xEventGroupSetBits(client_serving_task_events, STOP_SERVING_CLIENT);
                NETLOGGING_LOGI("Waiting for client task to stop...");
                xEventGroupWaitBits(
                    client_serving_task_events,
                    STOPPED_SERVING_CLIENT,
                    pdTRUE,
                    pdFALSE,
                    portMAX_DELAY
                );
                client_task = NULL;
            }

            // serve new client
            xEventGroupClearBits(client_serving_task_events, STOP_SERVING_CLIENT | STOPPED_SERVING_CLIENT);
            void *client_pvParams[] = {
            (void *)&client_sock,
            (void *)&client_serving_task_events
            };
            xTaskCreate(
                serve_client,
                "LOGS_SSE_SERVE_CLIENT",
                1024 * 4,
                (void *)client_pvParams,
                2,
                &client_task
            );
        } // end inner while

        NETLOGGING_LOGI("close socket and restart...");
        if (server_sock != -1) {
            shutdown(server_sock, 0);
            close(server_sock);
        }
    } // end outer while

    // Cleanup. Task is only responsible for freeing memory that it allocated.
    xEventGroupSetBits(handle->state_event, STOPPED_BIT);
    NETLOGGING_LOGI("multicast_log_sender task stopped");
    vTaskDelete(NULL);
}

esp_err_t netlogging_sse_server_run(void)
{
    if (NULL == handle) {
        return ESP_ERR_INVALID_STATE;
    }

    // Start Multicast Sender task
    handle->task_run = true;
    return xTaskCreate(sse_server, "HTTP SSE", 1024 * 6, NULL, 2, NULL);
}

esp_err_t netlogging_sse_server_wait_for_stop(void)
{
    if (NULL == handle) {
        return ESP_ERR_INVALID_STATE;
    }
    EventBits_t uxBits = xEventGroupWaitBits(handle->state_event, STOPPED_BIT, false, true, STOP_WAITTIME);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    if (uxBits & STOPPED_BIT)
    {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t netlogging_sse_server_stop(void)
{
    if (NULL == handle) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Tell task to stop and delete itself */
    handle->task_run = false;
    esp_err_t ret = netlogging_sse_server_wait_for_stop();
    return ret;
}

esp_err_t netlogging_sse_server_init(const sse_logging_param_t *param)
{
    NETLOGGING_LOGI("start see logging: port=%ld", param->port);

    // Allocate memory for the handle
    handle = malloc(sizeof(struct handle_s));
    if (handle == NULL) {
        NETLOGGING_LOGE("malloc fail");
        return ESP_ERR_NO_MEM;
    }
    memset(handle, 0, sizeof(struct handle_s));
    handle->param = *param;
    handle->state_event = xEventGroupCreate();
    if (handle->state_event == NULL) {
        NETLOGGING_LOGE("xEventGroupCreate failed");
        goto _init_failed;
    }
    return ESP_OK;

_init_failed:
    netlogging_sse_server_deinit();
    return ESP_ERR_NO_MEM;
}

esp_err_t netlogging_sse_server_deinit(void)
{
    if (handle)
    {
        if (handle->state_event)
        {
            vEventGroupDelete(handle->state_event);
        }

        free(handle);
        handle = NULL;
        return ESP_OK;
    }
    return ESP_FAIL;
}
