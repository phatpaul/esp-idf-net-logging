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

esp_err_t netlogging_init(bool enableStdout);
esp_err_t netlogging_deinit(void);
esp_err_t netlogging_register_recieveBuffer(void *buffer);
esp_err_t netlogging_unregister_recieveBuffer(void *buffer);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} udp_logging_param_t;
esp_err_t netlogging_udp_client_init(const udp_logging_param_t *param);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} multicast_logging_param_t;
#define NETLOGGING_MULTICAST_DEFAULT_CONFIG() {  \
    .ipv4addr = "239.2.1.2",\
    .port = 2054,                    \
}
esp_err_t netlogging_multicast_sender_init(const multicast_logging_param_t *param);
esp_err_t netlogging_multicast_sender_run(void);
esp_err_t netlogging_multicast_sender_stop(void);
esp_err_t netlogging_multicast_sender_deinit(void);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} tcp_logging_param_t;
esp_err_t netlogging_tcp_client_init(const tcp_logging_param_t *param);

typedef struct {
    char url[128];
    char topic[64];
} mqtt_logging_param_t;
esp_err_t mqtt_logging_init(const mqtt_logging_param_t *param);

typedef struct {
    char url[128];
} http_logging_param_t;
esp_err_t http_logging_init(const http_logging_param_t *param);

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
