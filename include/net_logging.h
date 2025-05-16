#ifndef NET_LOGGING_H_
#define NET_LOGGING_H_

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
#define EXTERN_C_BEGIN  extern "C" {
#define EXTERN_C_END    }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif

EXTERN_C_BEGIN

// Don't use ESP_LOGx in this module, as it will call logging_vprintf again, causing a infinite recursion and stack overflow.
// ESP_EARLY_LOGx macros are used instead.
#define NETLOGGING_LOGE(fmt, args...) ESP_EARLY_LOGE(TAG, fmt, ##args)
#define NETLOGGING_LOGW(fmt, args...) ESP_EARLY_LOGW(TAG, fmt, ##args)
#define NETLOGGING_LOGI(fmt, args...) ESP_EARLY_LOGI(TAG, fmt, ##args)
#define NETLOGGING_LOGD(fmt, args...) ESP_EARLY_LOGD(TAG, fmt, ##args)
#define NETLOGGING_LOGV(fmt, args...) ESP_EARLY_LOGV(TAG, fmt, ##args)

esp_err_t netlogging_init(bool enableStdout);
esp_err_t netlogging_deinit(void);
esp_err_t netlogging_register_recieveBuffer(void *buffer);
esp_err_t netlogging_unregister_recieveBuffer(void *buffer);

typedef struct {
    const char *ipv4addr;
    unsigned long port;
} udp_logging_param_t;
#define NETLOGGING_UDP_DEFAULT_CONFIG() {  \
    .ipv4addr = "255.255.255",\
    .port = 6789,             \
}
esp_err_t netlogging_udp_client_init(const udp_logging_param_t *param);
esp_err_t netlogging_udp_client_run(void);
esp_err_t netlogging_udp_client_stop(void);
esp_err_t netlogging_udp_client_deinit(void);

typedef struct {
    const char *ipv4addr;
    unsigned long port;
} multicast_logging_param_t;
#define NETLOGGING_MULTICAST_DEFAULT_CONFIG() {  \
    .ipv4addr = "239.2.1.2",\
    .port = 2054,           \
}
esp_err_t netlogging_multicast_sender_init(const multicast_logging_param_t *param);
esp_err_t netlogging_multicast_sender_run(void);
esp_err_t netlogging_multicast_sender_stop(void);
esp_err_t netlogging_multicast_sender_deinit(void);

typedef struct {
    const char *ipv4addr;
    unsigned long port;
} tcp_logging_param_t;
esp_err_t netlogging_tcp_client_init(const tcp_logging_param_t *param);
esp_err_t netlogging_tcp_client_run(void);
esp_err_t netlogging_tcp_client_stop(void);
esp_err_t netlogging_tcp_client_deinit(void);

typedef struct {
    const char *url;
} http_logging_param_t;
esp_err_t netlogging_http_client_init(const http_logging_param_t *param);
esp_err_t netlogging_http_client_run(void);
esp_err_t netlogging_http_client_stop(void);
esp_err_t netlogging_http_client_deinit(void);

typedef struct {
    unsigned long port;
} sse_logging_param_t;
#define NETLOGGING_SSE_DEFAULT_CONFIG() {  \
    .port = 8080,                    \
}
esp_err_t netlogging_sse_server_init(const sse_logging_param_t *param);
esp_err_t netlogging_sse_server_run(void);
esp_err_t netlogging_sse_server_stop(void);
esp_err_t netlogging_sse_server_deinit(void);

EXTERN_C_END
#endif /* NET_LOGGING_H_ */
