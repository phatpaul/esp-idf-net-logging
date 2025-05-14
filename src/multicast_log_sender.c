/*
    Multicast Log Sender

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/


#include "esp_system.h"
#include "lwip/sockets.h"
#include "lwip/inet.h" // for inet_addr_from_ip4addr
#include "lwip/netdb.h" // for getaddrinfo
#include "esp_netif.h"
#include "net_logging_priv.h"

#define MULTICAST_TTL (1) // 1=don't leave the subnet
#define USE_DEFAULT_IF (1) // 1=bind to default interface, 0=bind to specific interface
#define RETRY_TIMEOUT_MS (3000) // retry timeout in ms
#define STOPPED_BIT (1UL << 0) // bit to signal that task has stopped
#define STOP_WAITTIME (8000 / portTICK_PERIOD_MS) // wait this long for task to stop

#define TAG "multicast_log_sender"

struct server_handle_s
{
    multicast_logging_param_t param;
    volatile bool task_run;
    EventGroupHandle_t *state_event;      /*!< Task's state event group */
};
static struct server_handle_s *server = NULL;

static int create_multicast_ipv4_socket(struct in_addr bind_iaddr, uint16_t port)
{
    int sock = -1;
    int err = 0;
    char addrbuf[32] = {0};

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        NETLOGGING_LOGE("Failed to create socket. Error %d", errno);
        return -1;
    }
    // enable SO_REUSEADDR so servers restarted on the same ip addresses
    // do not require waiting for 2 minutes while the socket is in TIME_WAIT
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        NETLOGGING_LOGE("setsockopt(SO_REUSEADDR) failed");
    }

    // Configure source interface (bind to interface)
    struct sockaddr_in saddr = {0};
    saddr.sin_addr.s_addr = bind_iaddr.s_addr; // what interface IP to bind to.  Can be htonl(INADDR_ANY)
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(port);

    inet_ntoa_r(saddr.sin_addr.s_addr, addrbuf, sizeof(addrbuf) - 1);
    NETLOGGING_LOGI("Binding to interface %s...", addrbuf);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0)
    {
        NETLOGGING_LOGE("Failed to bind socket. Error %d", errno);
        goto err;
    }

    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0)
    {
        NETLOGGING_LOGE("Failed to set IP_MULTICAST_TTL. Error %d", errno);
        goto err;
    }

    // All set, socket is configured for sending
    return sock;

err:
    close(sock);
    return -1;
}

static int multicast_setup(int *sock_out, struct addrinfo **res_out, struct in_addr src_addr, const char *mcast_addr, uint16_t port)
{
    int err = 0;
    int sock = create_multicast_ipv4_socket(src_addr, port);
    if (sock < 0)
    {
        NETLOGGING_LOGE("Failed to create socket. Error %d", errno);
        return -1;
    }

    struct addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_socktype = SOCK_DGRAM,
    };
    hints.ai_family = AF_INET; // For an IPv4 socket
    struct addrinfo *res;

    err = getaddrinfo(mcast_addr, NULL, &hints, &res);
    if (0 != err) {
        NETLOGGING_LOGE("Failed getaddrinfo");
        return -1;
    }

    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);
    *res_out = res;
    *sock_out = sock;
    return err;
}

// UDP Multicast Log Sender Task
static void multicast_log_sender(void *pvParameters)
{
    NETLOGGING_LOGI("start multicast logging: ipaddr=[%s] port=%ld", server->param.ipv4addr, server->param.port);

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

    while (server->task_run) // Outer while loop to create socket
    {
        // Configure source interface
        struct in_addr src_addr = {0};
#if USE_DEFAULT_IF
        src_addr.s_addr = htonl(INADDR_ANY); // Bind the socket to any address
#else
        esp_netif_ip_info_t eth_ip_info;
        esp_netif_t *netif_eth = esp_netif_get_handle_from_ifkey("ETH_DEF");
        esp_netif_get_ip_info(netif_eth, &eth_ip_info);
        inet_addr_from_ip4addr(&src_addr, &eth_ip_info.ip);
#endif
        /* create the socket */
        int sock = -1;
        struct addrinfo *res_toFree = NULL;
        if (0 != multicast_setup(&sock, &res_toFree, src_addr, server->param.ipv4addr, server->param.port))
        {
            NETLOGGING_LOGE("Failed to setup");
            // wait and then try again
            vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
            continue;
        }

        while (server->task_run)  // Inner while loop to send data
        {
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
            size_t received;
            char *buffer = (char *)xRingbufferReceive(xRingBuffer, &received, portMAX_DELAY);
            //NETLOGGING_LOGI("xRingBufferReceive received=%d", received);
#else
            char buffer[CONFIG_NET_LOGGING_MESSAGE_MAX_LENGTH];
            size_t received = xMessageBufferReceive(xMessageBuffer, buffer, sizeof(buffer), portMAX_DELAY);
            //NETLOGGING_LOGI("xMessageBufferReceive received=%d", received);
#endif
            if (received > 0) {
                //NETLOGGING_LOGI("xMessageBufferReceive buffer=[%.*s]",received, buffer);
                int sendto_ret = sendto(sock, buffer, received, 0, res_toFree->ai_addr, res_toFree->ai_addrlen);
                if (sendto_ret < 0)
                {
                    NETLOGGING_LOGE("sendto failed. errno: %d", errno);
                    // wait and then try again
                    vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
                    break; // break out of the inner while loop, back to the outer while loop to try to create the socket again
                }

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
                vRingbufferReturnItem(xRingBuffer, (void *)buffer);
#endif
            }
            else {
                NETLOGGING_LOGE("BufferReceive fail");
                // wait and then try again
                vTaskDelay(RETRY_TIMEOUT_MS / portTICK_PERIOD_MS);
                break; // break out of the inner while loop, back to the outer while loop to try to create the socket again
            }
        } // end inner while

        NETLOGGING_LOGI("close socket and restart...");
        freeaddrinfo(res_toFree); // free the addrinfo struct
        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    } // end outer while

_init_failed:
    // Cleanup. Task is only responsible for freeing memory that it allocated.
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
    xEventGroupSetBits(server->state_event, STOPPED_BIT);
    NETLOGGING_LOGI("multicast_log_sender task stopped");
    vTaskDelete(NULL);
}

esp_err_t netlogging_multicast_sender_run(void)
{
    if (NULL == server) {
        return ESP_ERR_INVALID_STATE;
    }

    // Start Multicast Sender task
    server->task_run = true;
    return xTaskCreate(multicast_log_sender, "MCAST", 1024 * 6, NULL, 2, NULL);
}

esp_err_t netlogging_multicast_sender_wait_for_stop(void)
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

esp_err_t netlogging_multicast_sender_stop(void)
{
    if (NULL == server) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Tell task to stop and delete itself */
    server->task_run = false;
    esp_err_t ret = netlogging_multicast_sender_wait_for_stop();
    return ret;
}

esp_err_t netlogging_multicast_sender_init(const multicast_logging_param_t *param)
{
    NETLOGGING_LOGI("start multicast logging: ipaddr=[%s] port=%ld", param->ipv4addr, param->port);

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
    return ESP_OK;

_init_failed:
    netlogging_multicast_sender_deinit();
    return ESP_ERR_NO_MEM;
}

esp_err_t netlogging_multicast_sender_deinit(void)
{
    if (server)
    {
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
